// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "cc/test/test_now_source.h"
#include "content/renderer/scheduler/task_queue_selector.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;

namespace content {
namespace {

class SelectorForTest : public TaskQueueSelector {
 public:
  SelectorForTest() {}

  void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) override {
    work_queues_ = work_queues;
  }

  bool SelectWorkQueueToService(size_t* out_queue_index) override {
    if (queues_to_service_.empty())
      return false;
    *out_queue_index = queues_to_service_.front();
    queues_to_service_.pop_front();
    return true;
  }

  void AppendQueueToService(size_t queue_index) {
    queues_to_service_.push_back(queue_index);
  }

  const std::vector<const base::TaskQueue*>& work_queues() {
    return work_queues_;
  }

  void AsValueInto(base::trace_event::TracedValue* state) const override {
  }

 private:
  std::deque<size_t> queues_to_service_;
  std::vector<const base::TaskQueue*> work_queues_;

  DISALLOW_COPY_AND_ASSIGN(SelectorForTest);
};

class TaskQueueManagerTest : public testing::Test {
 protected:
  void Initialize(size_t num_queues) {
    test_task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner());
    selector_ = make_scoped_ptr(new SelectorForTest);
    manager_ = make_scoped_ptr(
        new TaskQueueManager(num_queues, test_task_runner_, selector_.get()));
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    message_loop_.reset(new base::MessageLoop());
    selector_ = make_scoped_ptr(new SelectorForTest);
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues, message_loop_->task_runner(), selector_.get()));
  }

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  scoped_ptr<SelectorForTest> selector_;
  scoped_ptr<TaskQueueManager> manager_;
  scoped_ptr<base::MessageLoop> message_loop_;
};

void PostFromNestedRunloop(base::MessageLoop* message_loop,
                           base::SingleThreadTaskRunner* runner,
                           std::vector<std::pair<base::Closure, bool>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  for (std::pair<base::Closure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, pair.first);
    } else {
      runner->PostNonNestableTask(FROM_HERE, pair.first);
    }
  }
  message_loop->RunUntilIdle();
}

void TestTask(int value, std::vector<int>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);
  EXPECT_EQ(3u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[3] = {
      manager_->TaskRunnerForQueue(0),
      manager_->TaskRunnerForQueue(1),
      manager_->TaskRunnerForQueue(2)};

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));
  runners[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 3, 5, 2, 4, 6));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 4, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 5, &run_order), true));

  runner->PostTask(
      FROM_HERE,
      base::Bind(&PostFromNestedRunloop, message_loop_.get(), runner,
                 base::Unretained(&tasks_to_post_from_nested_loop)));

  message_loop_->RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 4, 5, 3));
}

TEST_F(TaskQueueManagerTest, QueuePolling) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);
  EXPECT_EQ(delay, test_task_runner_->NextPendingTaskDelay());
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  EXPECT_TRUE(run_order.empty());

  // The task is inserted to the incoming queue only after the delay.
  test_task_runner_->RunPendingTasks();
  EXPECT_FALSE(manager_->IsQueueEmpty(0));
  EXPECT_TRUE(run_order.empty());

  // After the delay the task runs normally.
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotStayDelayed) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);
  test_task_runner_->RunPendingTasks();

  // Reload the work queue so we see the next pending task. It should no longer
  // be marked as delayed.
  manager_->PumpQueue(0);
  EXPECT_TRUE(selector_->work_queues()[0]->front().delayed_run_time.is_null());

  // Let the task run normally.
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // However polling still works.
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  // After pumping the task runs normally.
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // When pumping is enabled the task runs normally.
  manager_->SetAutoPump(0, true);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  // Since we haven't appended a work queue to be selected, the task doesn't
  // run.
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Pumping the queue again with a selected work queue runs the task.
  manager_->PumpQueue(0);
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithDelayedTask) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // After pumping the task runs normally.
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting two tasks and pumping twice should result in two tasks in the work
  // queue.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_->PumpQueue(0);
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  manager_->PumpQueue(0);

  EXPECT_EQ(2u, selector_->work_queues()[0]->size());
}

void ReentrantTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int countdown,
                       std::vector<int>* out_result) {
  out_result->push_back(countdown);
  if (--countdown) {
    runner->PostTask(FROM_HERE,
                     Bind(&ReentrantTestTask, runner, countdown, out_result));
  }
}

TEST_F(TaskQueueManagerTest, ReentrantPosting) {
  Initialize(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, Bind(&ReentrantTestTask, runner, 3, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_.reset();
  selector_.reset();
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<int>* run_order) {
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::Thread thread("TestThread");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runner, &run_order));
  thread.Stop();

  selector_->AppendQueueToService(0);
  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner) {
  runner->PostTask(
      FROM_HERE, Bind(&RePostingTestTask, base::Unretained(runner.get())));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u);

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&RePostingTestTask, runner));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybePostDoWorkOnMainRunner there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTasks().size());
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 1, &run_order), true));

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 0, &run_order));
  runner->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(), runner,
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  message_loop_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

void AdvanceNowTestTask(int value,
                        std::vector<int>* out_result,
                        scoped_refptr<cc::TestNowSource> time_source,
                        base::TimeDelta delta) {
  TestTask(value, out_result);
  time_source->AdvanceNow(delta);
}

TEST_F(TaskQueueManagerTest, InterruptWorkBatchForDelayedTask) {
  scoped_refptr<cc::TestNowSource> clock(cc::TestNowSource::Create());
  Initialize(1u);

  manager_->SetWorkBatchSize(2);
  manager_->SetTimeSourceForTesting(clock);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  runner->PostTask(
      FROM_HERE, base::Bind(&AdvanceNowTestTask, 2, &run_order, clock, delta));
  runner->PostTask(
      FROM_HERE, base::Bind(&AdvanceNowTestTask, 3, &run_order, clock, delta));

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(5));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);

  // At this point we have two posted tasks: one for DoWork and one of the
  // delayed task. Only the first non-delayed task should get executed because
  // the work batch is interrupted by the pending delayed task.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 2u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(2));

  // Running all remaining tasks should execute both pending tasks.
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

}  // namespace
}  // namespace content
