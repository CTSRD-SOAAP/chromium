// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "content/renderer/scheduler/renderer_task_queue_selector.h"
#include "ui/gfx/frame_time.h"

namespace content {

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : renderer_task_queue_selector_(new RendererTaskQueueSelector()),
      task_queue_manager_(
          new TaskQueueManager(TASK_QUEUE_COUNT,
                               main_task_runner,
                               renderer_task_queue_selector_.get())),
      control_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(CONTROL_TASK_QUEUE)),
      default_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(DEFAULT_TASK_QUEUE)),
      compositor_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(COMPOSITOR_TASK_QUEUE)),
      loading_task_runner_(
          task_queue_manager_->TaskRunnerForQueue(LOADING_TASK_QUEUE)),
      current_policy_(NORMAL_PRIORITY_POLICY),
      last_input_type_(blink::WebInputEvent::Undefined),
      input_stream_state_(INPUT_INACTIVE),
      policy_may_need_update_(&incoming_signals_lock_),
      weak_factory_(this) {
  weak_renderer_scheduler_ptr_ = weak_factory_.GetWeakPtr();
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_renderer_scheduler_ptr_);
  end_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_renderer_scheduler_ptr_));
  idle_task_runner_ = make_scoped_refptr(new SingleThreadIdleTaskRunner(
      task_queue_manager_->TaskRunnerForQueue(IDLE_TASK_QUEUE),
      base::Bind(&RendererSchedulerImpl::CurrentIdleTaskDeadlineCallback,
                 weak_renderer_scheduler_ptr_)));
  renderer_task_queue_selector_->SetQueuePriority(
      CONTROL_TASK_QUEUE, RendererTaskQueueSelector::CONTROL_PRIORITY);
  renderer_task_queue_selector_->DisableQueue(IDLE_TASK_QUEUE);
  task_queue_manager_->SetAutoPump(IDLE_TASK_QUEUE, false);
  // TODO(skyostil): Increase this to 4 (crbug.com/444764).
  task_queue_manager_->SetWorkBatchSize(1);

  for (size_t i = 0; i < TASK_QUEUE_COUNT; i++) {
    task_queue_manager_->SetQueueName(
        i, TaskQueueIdToString(static_cast<QueueId>(i)));
  }
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

void RendererSchedulerImpl::Shutdown() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_queue_manager_.reset();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::DefaultTaskRunner() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return default_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return idle_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return loading_task_runner_;
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return;

  EndIdlePeriod();
  estimated_next_frame_begin_ = args.frame_time + args.interval;
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return;

  base::TimeTicks now(Now());
  if (now < estimated_next_frame_begin_) {
    StartIdlePeriod();
    control_task_runner_->PostDelayedTask(FROM_HERE,
                                          end_idle_period_closure_.callback(),
                                          estimated_next_frame_begin_ - now);
  }
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // TODO(rmcilroy): Implement long idle times.
}

void RendererSchedulerImpl::DidReceiveInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidReceiveInputEventOnCompositorThread");
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if (web_input_event.type == blink::WebInputEvent::MouseMove &&
      (web_input_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    UpdateForInputEvent(web_input_event.type);
    return;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::isMouseEventType(web_input_event.type) ||
      blink::WebInputEvent::isKeyboardEventType(web_input_event.type)) {
    return;
  }
  UpdateForInputEvent(web_input_event.type);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  UpdateForInputEvent(blink::WebInputEvent::Undefined);
}

void RendererSchedulerImpl::UpdateForInputEvent(
    blink::WebInputEvent::Type type) {
  base::AutoLock lock(incoming_signals_lock_);

  InputStreamState new_input_stream_state =
      ComputeNewInputStreamState(input_stream_state_, type, last_input_type_);

  if (input_stream_state_ != new_input_stream_state) {
    // Update scheduler policy if we should start a new policy mode.
    input_stream_state_ = new_input_stream_state;
    policy_may_need_update_.SetLocked(true);
    PostUpdatePolicyOnControlRunner(base::TimeDelta());
  }
  last_input_time_ = Now();
  last_input_type_ = type;
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return false;

  MaybeUpdatePolicy();
  // The touchstart and compositor policies indicate a strong likelihood of
  // high-priority work in the near future.
  return SchedulerPolicy() == COMPOSITOR_PRIORITY_POLICY ||
         SchedulerPolicy() == TOUCHSTART_PRIORITY_POLICY;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return false;

  MaybeUpdatePolicy();
  // We only yield if we are in the compositor priority and there is compositor
  // work outstanding, or if we are in the touchstart response priority.
  // Note: even though the control queue is higher priority we don't yield for
  // it since these tasks are not user-provided work and they are only intended
  // to run before the next task, not interrupt the tasks.
  switch (SchedulerPolicy()) {
    case NORMAL_PRIORITY_POLICY:
      return false;

    case COMPOSITOR_PRIORITY_POLICY:
      return !task_queue_manager_->IsQueueEmpty(COMPOSITOR_TASK_QUEUE);

    case TOUCHSTART_PRIORITY_POLICY:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

void RendererSchedulerImpl::CurrentIdleTaskDeadlineCallback(
    base::TimeTicks* deadline_out) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  *deadline_out = estimated_next_frame_begin_;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::SchedulerPolicy() const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  return current_policy_;
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::PostUpdatePolicyOnControlRunner(
    base::TimeDelta delay) {
  control_task_runner_->PostDelayedTask(
      FROM_HERE, update_policy_closure_, delay);
}

void RendererSchedulerImpl::UpdatePolicy() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!task_queue_manager_)
    return;

  base::AutoLock lock(incoming_signals_lock_);
  base::TimeTicks now;
  policy_may_need_update_.SetLocked(false);

  Policy new_policy = NORMAL_PRIORITY_POLICY;
  if (input_stream_state_ != INPUT_INACTIVE) {
    base::TimeDelta new_priority_duration =
        base::TimeDelta::FromMilliseconds(kPriorityEscalationAfterInputMillis);
    base::TimeTicks new_priority_end(last_input_time_ + new_priority_duration);
    base::TimeDelta time_left_in_policy = new_priority_end - Now();
    if (time_left_in_policy > base::TimeDelta()) {
      PostUpdatePolicyOnControlRunner(time_left_in_policy);
      new_policy =
          input_stream_state_ == INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE
              ? TOUCHSTART_PRIORITY_POLICY
              : COMPOSITOR_PRIORITY_POLICY;
    } else {
      // Reset |input_stream_state_| to ensure
      // DidReceiveInputEventOnCompositorThread will post an UpdatePolicy task
      // when it's next called.
      input_stream_state_ = INPUT_INACTIVE;
    }
  }

  if (new_policy == current_policy_)
    return;

  switch (new_policy) {
    case COMPOSITOR_PRIORITY_POLICY:
      renderer_task_queue_selector_->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, RendererTaskQueueSelector::HIGH_PRIORITY);
      // TODO(scheduler-dev): Add a task priority between HIGH and BEST_EFFORT
      // that still has some guarantee of running.
      renderer_task_queue_selector_->SetQueuePriority(
          LOADING_TASK_QUEUE, RendererTaskQueueSelector::BEST_EFFORT_PRIORITY);
      break;
    case TOUCHSTART_PRIORITY_POLICY:
      renderer_task_queue_selector_->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, RendererTaskQueueSelector::HIGH_PRIORITY);
      renderer_task_queue_selector_->DisableQueue(LOADING_TASK_QUEUE);
      break;
    case NORMAL_PRIORITY_POLICY:
      renderer_task_queue_selector_->SetQueuePriority(
          COMPOSITOR_TASK_QUEUE, RendererTaskQueueSelector::NORMAL_PRIORITY);
      renderer_task_queue_selector_->SetQueuePriority(
          LOADING_TASK_QUEUE, RendererTaskQueueSelector::NORMAL_PRIORITY);
      break;
  }
  DCHECK(renderer_task_queue_selector_->IsQueueEnabled(COMPOSITOR_TASK_QUEUE));
  if (new_policy != TOUCHSTART_PRIORITY_POLICY)
    DCHECK(renderer_task_queue_selector_->IsQueueEnabled(LOADING_TASK_QUEUE));

  current_policy_ = new_policy;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(now));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.policy", current_policy_);
}

void RendererSchedulerImpl::StartIdlePeriod() {
  TRACE_EVENT_ASYNC_BEGIN0("renderer.scheduler",
                           "RendererSchedulerIdlePeriod", this);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  renderer_task_queue_selector_->EnableQueue(
      IDLE_TASK_QUEUE, RendererTaskQueueSelector::BEST_EFFORT_PRIORITY);
  task_queue_manager_->PumpQueue(IDLE_TASK_QUEUE);
}

void RendererSchedulerImpl::EndIdlePeriod() {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing && !estimated_next_frame_begin_.is_null() &&
      base::TimeTicks::Now() > estimated_next_frame_begin_) {
    TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
        "renderer.scheduler",
        "RendererSchedulerIdlePeriod",
        this,
        "DeadlineOverrun",
        estimated_next_frame_begin_.ToInternalValue());
  }
  TRACE_EVENT_ASYNC_END0("renderer.scheduler",
                         "RendererSchedulerIdlePeriod", this);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  end_idle_period_closure_.Cancel();
  renderer_task_queue_selector_->DisableQueue(IDLE_TASK_QUEUE);
}

void RendererSchedulerImpl::SetTimeSourceForTesting(
    scoped_refptr<cc::TestNowSource> time_source) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  time_source_ = time_source;
  task_queue_manager_->SetTimeSourceForTesting(time_source);
}

base::TimeTicks RendererSchedulerImpl::Now() const {
  return UNLIKELY(time_source_) ? time_source_->Now() : base::TimeTicks::Now();
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::PollableNeedsUpdateFlag(
    base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::~PollableNeedsUpdateFlag() {
}

void RendererSchedulerImpl::PollableNeedsUpdateFlag::SetLocked(bool value) {
  write_lock_->AssertAcquired();
  base::subtle::Release_Store(&flag_, value);
}

bool RendererSchedulerImpl::PollableNeedsUpdateFlag::IsSet() const {
  return base::subtle::Acquire_Load(&flag_) != 0;
}

// static
const char* RendererSchedulerImpl::TaskQueueIdToString(QueueId queue_id) {
  switch (queue_id) {
    case DEFAULT_TASK_QUEUE:
      return "default_tq";
    case COMPOSITOR_TASK_QUEUE:
      return "compositor_tq";
    case IDLE_TASK_QUEUE:
      return "idle_tq";
    case CONTROL_TASK_QUEUE:
      return "control_tq";
    case LOADING_TASK_QUEUE:
      return "loading_tq";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* RendererSchedulerImpl::PolicyToString(Policy policy) {
  switch (policy) {
    case NORMAL_PRIORITY_POLICY:
      return "normal";
    case COMPOSITOR_PRIORITY_POLICY:
      return "compositor";
    case TOUCHSTART_PRIORITY_POLICY:
      return "touchstart";
    default:
      NOTREACHED();
      return nullptr;
  }
}

const char* RendererSchedulerImpl::InputStreamStateToString(
    InputStreamState state) {
  switch (state) {
    case INPUT_INACTIVE:
      return "inactive";
    case INPUT_ACTIVE:
      return "active";
    case INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE:
      return "active_and_awaiting_touchstart_response";
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  incoming_signals_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = Now();
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetString("current_policy", PolicyToString(current_policy_));
  state->SetString("input_stream_state",
                   InputStreamStateToString(input_stream_state_));
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble("last_input_time",
                   (last_input_time_ - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "estimated_next_frame_begin",
      (estimated_next_frame_begin_ - base::TimeTicks()).InMillisecondsF());

  return state;
}

RendererSchedulerImpl::InputStreamState
RendererSchedulerImpl::ComputeNewInputStreamState(
    InputStreamState current_state,
    blink::WebInputEvent::Type new_input_type,
    blink::WebInputEvent::Type last_input_type) {
  switch (new_input_type) {
    case blink::WebInputEvent::TouchStart:
      return INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE;

    case blink::WebInputEvent::TouchMove:
      // Observation of consecutive touchmoves is a strong signal that the page
      // is consuming the touch sequence, in which case touchstart response
      // prioritization is no longer necessary. Otherwise, the initial touchmove
      // should preserve the touchstart response pending state.
      if (current_state == INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE) {
        return last_input_type == blink::WebInputEvent::TouchMove
                   ? INPUT_ACTIVE
                   : INPUT_ACTIVE_AND_AWAITING_TOUCHSTART_RESPONSE;
      }
      break;

    case blink::WebInputEvent::GestureTapDown:
    case blink::WebInputEvent::GestureShowPress:
    case blink::WebInputEvent::GestureFlingCancel:
    case blink::WebInputEvent::GestureScrollEnd:
      // With no observable effect, these meta events do not indicate a
      // meaningful touchstart response and should not impact task priority.
      return current_state;

    default:
      break;
  }
  return INPUT_ACTIVE;
}

}  // namespace content
