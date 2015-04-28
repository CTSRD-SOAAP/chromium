// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;
class DevToolsAgentHostImpl;
class WorkerDevToolsAgentHost;

// A base class of SharedWorkerDevToolsManager and ServiceWorkerDevToolsManager,
// provides common default implementation for them.
// This class lives on UI thread.
class CONTENT_EXPORT WorkerDevToolsManager {
 public:
  typedef std::pair<int, int> WorkerId;

  DevToolsAgentHostImpl* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                       int worker_route_id);
  void AddAllAgentHosts(std::vector<scoped_refptr<DevToolsAgentHost>>* result);
  void WorkerReadyForInspection(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);

 protected:
  typedef std::map<WorkerId, WorkerDevToolsAgentHost*> AgentHostMap;
  friend class SharedWorkerDevToolsManagerTest;

  WorkerDevToolsManager();
  virtual ~WorkerDevToolsManager();
  void RemoveInspectedWorkerData(WorkerId id);
  void WorkerRestarted(const WorkerId& id, const AgentHostMap::iterator& it);
  AgentHostMap& workers() { return workers_; }

 private:
  AgentHostMap workers_;
  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_WORKER_DEVTOOLS_MANAGER_H_
