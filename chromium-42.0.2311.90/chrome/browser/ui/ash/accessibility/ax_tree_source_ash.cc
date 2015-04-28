// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/ax_tree_source_ash.h"

#include <vector>

#include "chrome/browser/accessibility/ax_tree_id_registry.h"
#include "chrome/browser/ui/ash/accessibility/automation_manager_ash.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/controls/webview/webview.h"

using views::AXAuraObjCache;
using views::AXAuraObjWrapper;

AXTreeSourceAsh::AXTreeSourceAsh() {
  root_.reset(
      new AXRootObjWrapper(AXAuraObjCache::GetInstance()->GetNextID()));
}

AXTreeSourceAsh::~AXTreeSourceAsh() {
  root_.reset();
}

void AXTreeSourceAsh::DoDefault(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  CHECK(obj);
  obj->DoDefault();
}

void AXTreeSourceAsh::Focus(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  CHECK(obj);
  obj->Focus();
}

void AXTreeSourceAsh::MakeVisible(int32 id) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  CHECK(obj);
  obj->MakeVisible();
}

void AXTreeSourceAsh::SetSelection(int32 id, int32 start, int32 end) {
  AXAuraObjWrapper* obj = AXAuraObjCache::GetInstance()->Get(id);
  CHECK(obj);
  obj->SetSelection(start, end);
}

AXAuraObjWrapper* AXTreeSourceAsh::GetRoot() const {
  return root_.get();
}

AXAuraObjWrapper* AXTreeSourceAsh::GetFromId(int32 id) const {
  if (id == root_->GetID())
    return root_.get();
  return AXAuraObjCache::GetInstance()->Get(id);
}

int32 AXTreeSourceAsh::GetId(AXAuraObjWrapper* node) const {
  return node->GetID();
}

void AXTreeSourceAsh::GetChildren(AXAuraObjWrapper* node,
    std::vector<AXAuraObjWrapper*>* out_children) const {
  node->GetChildren(out_children);
}

AXAuraObjWrapper* AXTreeSourceAsh::GetParent(AXAuraObjWrapper* node) const {
  AXAuraObjWrapper* parent = node->GetParent();
  if (!parent && node->GetID() != root_->GetID())
    parent = root_.get();
  return parent;
}

bool AXTreeSourceAsh::IsValid(AXAuraObjWrapper* node) const {
  return node && node->GetID() != -1;
}

bool AXTreeSourceAsh::IsEqual(AXAuraObjWrapper* node1,
                                AXAuraObjWrapper* node2) const {
  if (!node1 || !node2)
    return false;

  return node1->GetID() == node2->GetID() && node1->GetID() != -1;
}

AXAuraObjWrapper* AXTreeSourceAsh::GetNull() const {
  return NULL;
}

void AXTreeSourceAsh::SerializeNode(
    AXAuraObjWrapper* node, ui::AXNodeData* out_data) const {
  node->Serialize(out_data);

  if (out_data->role == ui::AX_ROLE_WEB_VIEW) {
    views::View* view = static_cast<views::AXViewObjWrapper*>(node)->view();
    content::WebContents* contents =
        static_cast<views::WebView*>(view)->GetWebContents();
    content::RenderFrameHost* rfh = contents->GetMainFrame();
    if (rfh) {
      int process_id = rfh->GetProcess()->GetID();
      int routing_id = rfh->GetRoutingID();
      int ax_tree_id = AXTreeIDRegistry::GetInstance()->GetOrCreateAXTreeID(
          process_id, routing_id);
      out_data->AddIntAttribute(ui::AX_ATTR_CHILD_TREE_ID, ax_tree_id);
    }
  }
}

std::string AXTreeSourceAsh::ToString(
    AXAuraObjWrapper* root, std::string prefix) {
  ui::AXNodeData data;
  root->Serialize(&data);
  std::string output = prefix + data.ToString() + '\n';

  std::vector<AXAuraObjWrapper*> children;
  root->GetChildren(&children);

  prefix += prefix[0];
  for (size_t i = 0; i < children.size(); ++i)
    output += ToString(children[i], prefix);

  return output;
}
