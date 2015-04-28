// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "content/common/frame_replication_state.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FrameTree;
class Navigator;
class RenderFrameHostImpl;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
class CONTENT_EXPORT FrameTreeNode {
 public:

  FrameTreeNode(FrameTree* frame_tree,
                Navigator* navigator,
                RenderFrameHostDelegate* render_frame_delegate,
                RenderViewHostDelegate* render_view_delegate,
                RenderWidgetHostDelegate* render_widget_delegate,
                RenderFrameHostManager::Delegate* manager_delegate,
                const std::string& name);

  ~FrameTreeNode();

  bool IsMainFrame() const;

  void AddChild(scoped_ptr<FrameTreeNode> child,
                int process_id,
                int frame_routing_id);
  void RemoveChild(FrameTreeNode* child);

  // Clears process specific-state in this node to prepare for a new process.
  void ResetForNewProcess();

  FrameTree* frame_tree() const {
    return frame_tree_;
  }

  Navigator* navigator() {
    return navigator_.get();
  }

  RenderFrameHostManager* render_manager() {
    return &render_manager_;
  }

  int64 frame_tree_node_id() const {
    return frame_tree_node_id_;
  }

  const std::string& frame_name() const {
    return replication_state_.name;
  }

  size_t child_count() const {
    return children_.size();
  }

  FrameTreeNode* parent() const { return parent_; }

  FrameTreeNode* child_at(size_t index) const {
    return children_[index];
  }

  const GURL& current_url() const {
    return current_url_;
  }

  void set_current_url(const GURL& url) {
    current_url_ = url;
  }

  void set_current_origin(const url::Origin& origin) {
    replication_state_.origin = origin;
  }

  void set_sandbox_flags(SandboxFlags sandbox_flags) {
    replication_state_.sandbox_flags = sandbox_flags;
  }

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return replication_state_.origin.IsSameAs(node.replication_state_.origin);
  }

  const FrameReplicationState& current_replication_state() const {
    return replication_state_;
  }

  void set_is_loading(bool is_loading) {
    is_loading_ = is_loading;
  }
  bool is_loading() const {
    return is_loading_;
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  bool IsDescendantOf(FrameTreeNode* other) const;

 private:
  void set_parent(FrameTreeNode* parent) { parent_ = parent; }

  // The next available browser-global FrameTreeNode ID.
  static int64 next_frame_tree_node_id_;

  // The FrameTree that owns us.
  FrameTree* frame_tree_;  // not owned.

  // The Navigator object responsible for managing navigations at this node
  // of the frame tree.
  scoped_refptr<Navigator> navigator_;

  // Manages creation and swapping of RenderFrameHosts for this frame.  This
  // must be declared before |children_| so that it gets deleted after them.
  //  That's currently necessary so that RenderFrameHostImpl's destructor can
  // call GetProcess.
  RenderFrameHostManager render_manager_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int64 frame_tree_node_id_;

  // The parent node of this frame. NULL if this node is the root or if it has
  // not yet been attached to the frame tree.
  FrameTreeNode* parent_;

  // The immediate children of this specific frame.
  ScopedVector<FrameTreeNode> children_;

  // Track the current frame's last committed URL, so we can estimate the
  // process impact of out-of-process iframes.
  // TODO(creis): Remove this when we can store subframe URLs in the
  // NavigationController.
  GURL current_url_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  FrameReplicationState replication_state_;

  // Boolean value indicating whether the frame is in the process of loading
  // a document or not. In cross-process transfer navigation the DidStartLoading
  // message is received from both existing RenderFrame and from the pending
  // RenderFrame. However, there will be only one DidStopLoading message sent by
  // the pending-which-becomes-current RenderFrame. Since both renderers belong
  // to the FrameTreeNode, it is better to ask it about the loading status than
  // RenderFrameHost or using a counter to balance the events out.
  bool is_loading_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
