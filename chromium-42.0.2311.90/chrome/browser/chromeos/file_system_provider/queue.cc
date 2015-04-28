// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_system_provider/queue.h"

namespace chromeos {
namespace file_system_provider {

Queue::Task::Task() : token(0) {
}

Queue::Task::Task(size_t token, const AbortableCallback& callback)
    : token(token), callback(callback) {
}

Queue::Task::~Task() {
}

Queue::Queue(size_t max_in_parallel)
    : max_in_parallel_(max_in_parallel),
      next_token_(1),
      weak_ptr_factory_(this) {
  DCHECK_LT(0u, max_in_parallel);
}

Queue::~Queue() {
}

size_t Queue::NewToken() {
  return next_token_++;
}

void Queue::Enqueue(size_t token, const AbortableCallback& callback) {
#if !NDEBUG
  DCHECK(executed_.find(token) == executed_.end());
  for (auto& task : pending_) {
    DCHECK(token != task.token);
  }
#endif
  pending_.push_back(Task(token, callback));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
}

void Queue::Complete(size_t token) {
  const auto it = executed_.find(token);
  DCHECK(it != executed_.end());
  completed_[token] = it->second;
  executed_.erase(it);
}

void Queue::Remove(size_t token) {
  const auto it = completed_.find(token);
  if (it != completed_.end()) {
    completed_.erase(it);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If not completed, then it must have been aborted.
  const auto aborted_it = aborted_.find(token);
  DCHECK(aborted_it != aborted_.end());
  aborted_.erase(aborted_it);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
}

void Queue::MaybeRun() {
  if (executed_.size() + completed_.size() == max_in_parallel_ ||
      !pending_.size()) {
    return;
  }

  DCHECK_GT(max_in_parallel_, executed_.size() + completed_.size());
  Task task = pending_.front();
  pending_.pop_front();

  executed_[task.token] = task;
  AbortCallback abort_callback = task.callback.Run();

  // It may happen that the task is completed and removed synchronously. Hence,
  // we need to check if the task is still in the executed collection.
  const auto executed_task_it = executed_.find(task.token);
  if (executed_task_it != executed_.end())
    executed_task_it->second.abort_callback = abort_callback;
}

void Queue::Abort(size_t token) {
  // Check if it's running.
  const auto it = executed_.find(token);
  if (it != executed_.end()) {
    Task task = it->second;
    aborted_[token] = task;
    executed_.erase(it);
    DCHECK(!task.abort_callback.is_null());
    task.abort_callback.Run();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Aborting not running tasks is linear. TODO(mtomasz): Optimize if feasible.
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (token == it->token) {
      aborted_[token] = *it;
      pending_.erase(it);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }

  // The task is already removed, marked as completed or aborted.
  NOTREACHED();
}

bool Queue::IsAborted(size_t token) {
#if !NDEBUG
  bool in_queue = executed_.find(token) != executed_.end() ||
                  completed_.find(token) != completed_.end() ||
                  aborted_.find(token) != aborted_.end();
  for (auto& task : pending_) {
    if (token == task.token) {
      in_queue = true;
      break;
    }
  }
  DCHECK(in_queue);
#endif
  return aborted_.find(token) != aborted_.end();
}

}  // namespace file_system_provider
}  // namespace chromeos
