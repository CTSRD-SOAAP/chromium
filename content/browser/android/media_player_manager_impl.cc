// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/media_player_manager_impl.h"

#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/media_resource_getter_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "media/base/android/media_drm_bridge.h"

using media::MediaDrmBridge;
using media::MediaPlayerAndroid;

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
static const int kMediaPlayerThreshold = 1;

namespace media {

static MediaPlayerManager::FactoryFunction g_factory_function = NULL;

// static
CONTENT_EXPORT void MediaPlayerManager::RegisterFactoryFunction(
    FactoryFunction factory_function) {
  g_factory_function = factory_function;
}

// static
media::MediaPlayerManager* MediaPlayerManager::Create(
    content::RenderViewHost* render_view_host) {
  if (g_factory_function)
    return g_factory_function(render_view_host);
  return new content::MediaPlayerManagerImpl(render_view_host);
}

}  // namespace media

namespace content {

MediaPlayerManagerImpl::MediaPlayerManagerImpl(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      fullscreen_player_id_(-1),
      web_contents_(WebContents::FromRenderViewHost(render_view_host)) {
}

MediaPlayerManagerImpl::~MediaPlayerManagerImpl() {}

bool MediaPlayerManagerImpl::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaPlayerManagerImpl, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_EnterFullscreen, OnEnterFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ExitFullscreen, OnExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerInitialize, OnInitialize)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerStart, OnStart)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerSeek, OnSeek)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerPause, OnPause)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaPlayerRelease,
                        OnReleaseResources)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyMediaPlayer, OnDestroyPlayer)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DestroyAllMediaPlayers,
                        DestroyAllMediaPlayers)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DemuxerReady,
                        OnDemuxerReady)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ReadFromDemuxerAck,
                        OnReadFromDemuxerAck)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DurationChanged,
                        OnDurationChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_MediaSeekRequestAck,
                        OnMediaSeekRequestAck)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_InitializeCDM,
                        OnInitializeCDM)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_GenerateKeyRequest,
                        OnGenerateKeyRequest)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_AddKey, OnAddKey)
    IPC_MESSAGE_HANDLER(MediaKeysHostMsg_CancelKeyRequest,
                        OnCancelKeyRequest)
#if defined(GOOGLE_TV)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_NotifyExternalSurface,
                        OnNotifyExternalSurface)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaPlayerManagerImpl::FullscreenPlayerPlay() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    player->Start();
    Send(new MediaPlayerMsg_DidMediaPlayerPlay(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerImpl::FullscreenPlayerPause() {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    player->Pause();
    Send(new MediaPlayerMsg_DidMediaPlayerPause(
        routing_id(), fullscreen_player_id_));
  }
}

void MediaPlayerManagerImpl::FullscreenPlayerSeek(int msec) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player)
    player->SeekTo(base::TimeDelta::FromMilliseconds(msec));
}

void MediaPlayerManagerImpl::ExitFullscreen(bool release_media_player) {
  Send(new MediaPlayerMsg_DidExitFullscreen(
      routing_id(), fullscreen_player_id_));
  video_view_.reset();
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  fullscreen_player_id_ = -1;
  if (!player)
    return;
  if (release_media_player)
    player->Release();
  else
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void MediaPlayerManagerImpl::OnTimeUpdate(int player_id,
                                          base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaTimeUpdate(
      routing_id(), player_id, current_time));
}

void MediaPlayerManagerImpl::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  MediaPlayerAndroid* player = GetFullscreenPlayer();
  if (player) {
    player->SetVideoSurface(surface.Pass());
    Send(new MediaPlayerMsg_DidEnterFullscreen(
        routing_id(), player->player_id()));
  }
}

void MediaPlayerManagerImpl::OnMediaMetadataChanged(
    int player_id, base::TimeDelta duration, int width, int height,
    bool success) {
  Send(new MediaPlayerMsg_MediaMetadataChanged(
      routing_id(), player_id, duration, width, height, success));
  if (fullscreen_player_id_ != -1)
    video_view_->UpdateMediaMetadata();
}

void MediaPlayerManagerImpl::OnPlaybackComplete(int player_id) {
  Send(new MediaPlayerMsg_MediaPlaybackCompleted(routing_id(), player_id));
  if (fullscreen_player_id_ != -1)
    video_view_->OnPlaybackComplete();
}

void MediaPlayerManagerImpl::OnMediaInterrupted(int player_id) {
  // Tell WebKit that the audio should be paused, then release all resources
  Send(new MediaPlayerMsg_DidMediaPlayerPause(routing_id(), player_id));
  OnReleaseResources(player_id);
}

void MediaPlayerManagerImpl::OnBufferingUpdate(
    int player_id, int percentage) {
  Send(new MediaPlayerMsg_MediaBufferingUpdate(
      routing_id(), player_id, percentage));
  if (fullscreen_player_id_ != -1)
    video_view_->OnBufferingUpdate(percentage);
}

void MediaPlayerManagerImpl::OnSeekComplete(int player_id,
                                            base::TimeDelta current_time) {
  Send(new MediaPlayerMsg_MediaSeekCompleted(
      routing_id(), player_id, current_time));
}

void MediaPlayerManagerImpl::OnError(int player_id, int error) {
  Send(new MediaPlayerMsg_MediaError(routing_id(), player_id, error));
  if (fullscreen_player_id_ != -1)
    video_view_->OnMediaPlayerError(error);
}

void MediaPlayerManagerImpl::OnVideoSizeChanged(
    int player_id, int width, int height) {
  Send(new MediaPlayerMsg_MediaVideoSizeChanged(routing_id(), player_id,
      width, height));
  if (fullscreen_player_id_ != -1)
    video_view_->OnVideoSizeChanged(width, height);
}

void MediaPlayerManagerImpl::OnReadFromDemuxer(
    int player_id, media::DemuxerStream::Type type, bool seek_done) {
  Send(new MediaPlayerMsg_ReadFromDemuxer(
      routing_id(), player_id, type, seek_done));
}

void MediaPlayerManagerImpl::RequestMediaResources(int player_id) {
  int num_active_player = 0;
  ScopedVector<MediaPlayerAndroid>::iterator it;
  for (it = players_.begin(); it != players_.end(); ++it) {
    if (!(*it)->IsPlayerReady())
      continue;

    // The player is already active, ignore it.
    if ((*it)->player_id() == player_id)
      return;
    else
      num_active_player++;
  }

  // Number of active players are less than the threshold, do nothing.
  if (num_active_player < kMediaPlayerThreshold)
    return;

  for (it = players_.begin(); it != players_.end(); ++it) {
    if ((*it)->IsPlayerReady() && !(*it)->IsPlaying() &&
        fullscreen_player_id_ != (*it)->player_id()) {
      (*it)->Release();
      Send(new MediaPlayerMsg_MediaPlayerReleased(
          routing_id(), (*it)->player_id()));
    }
  }
}

void MediaPlayerManagerImpl::ReleaseMediaResources(int player_id) {
  // Nothing needs to be done.
}

media::MediaResourceGetter* MediaPlayerManagerImpl::GetMediaResourceGetter() {
  if (!media_resource_getter_.get()) {
    RenderProcessHost* host = render_view_host()->GetProcess();
    BrowserContext* context = host->GetBrowserContext();
    StoragePartition* partition = host->GetStoragePartition();
    fileapi::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : NULL;
    media_resource_getter_.reset(new MediaResourceGetterImpl(
        context, file_system_context, host->GetID(), routing_id()));
  }
  return media_resource_getter_.get();
}

MediaPlayerAndroid* MediaPlayerManagerImpl::GetFullscreenPlayer() {
  return GetPlayer(fullscreen_player_id_);
}

MediaPlayerAndroid* MediaPlayerManagerImpl::GetPlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id)
      return *it;
  }
  return NULL;
}

MediaDrmBridge* MediaPlayerManagerImpl::GetDrmBridge(int media_keys_id) {
  for (ScopedVector<MediaDrmBridge>::iterator it = drm_bridges_.begin();
      it != drm_bridges_.end(); ++it) {
    if ((*it)->media_keys_id() == media_keys_id)
      return *it;
  }
  return NULL;
}

void MediaPlayerManagerImpl::DestroyAllMediaPlayers() {
  players_.clear();
  if (fullscreen_player_id_ != -1) {
    video_view_.reset();
    fullscreen_player_id_ = -1;
  }
}

void MediaPlayerManagerImpl::OnMediaSeekRequest(
    int player_id, base::TimeDelta time_to_seek, unsigned seek_request_id) {
  Send(new MediaPlayerMsg_MediaSeekRequest(
      routing_id(), player_id, time_to_seek, seek_request_id));
}

void MediaPlayerManagerImpl::OnMediaConfigRequest(int player_id) {
  Send(new MediaPlayerMsg_MediaConfigRequest(routing_id(), player_id));
}

void MediaPlayerManagerImpl::OnKeyAdded(int media_keys_id,
                                        const std::string& session_id) {
  Send(new MediaKeysMsg_KeyAdded(routing_id(), media_keys_id, session_id));
}

void MediaPlayerManagerImpl::OnKeyError(int media_keys_id,
                                        const std::string& session_id,
                                        media::MediaKeys::KeyError error_code,
                                        int system_code) {
  Send(new MediaKeysMsg_KeyError(routing_id(), media_keys_id,
                                   session_id, error_code, system_code));
}

void MediaPlayerManagerImpl::OnKeyMessage(int media_keys_id,
                                          const std::string& session_id,
                                          const std::string& message,
                                          const std::string& destination_url) {
  Send(new MediaKeysMsg_KeyMessage(routing_id(), media_keys_id, session_id,
                                     message, destination_url));
}

#if defined(GOOGLE_TV)
void MediaPlayerManagerImpl::AttachExternalVideoSurface(int player_id,
                                                        jobject surface) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player) {
    player->SetVideoSurface(
        gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
  }
}

void MediaPlayerManagerImpl::DetachExternalVideoSurface(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SetVideoSurface(gfx::ScopedJavaSurface());
}

void MediaPlayerManagerImpl::OnNotifyExternalSurface(
    int player_id, bool is_request, const gfx::RectF& rect) {
  if (!web_contents_)
    return;

  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->NotifyExternalSurface(player_id, is_request, rect);
}
#endif

void MediaPlayerManagerImpl::OnEnterFullscreen(int player_id) {
  DCHECK_EQ(fullscreen_player_id_, -1);

  if (video_view_.get()) {
    fullscreen_player_id_ = player_id;
    video_view_->OpenVideo();
  } else if (!ContentVideoView::HasContentVideoView()) {
    // In Android WebView, two ContentViewCores could both try to enter
    // fullscreen video, we just ignore the second one.
    fullscreen_player_id_ = player_id;
    WebContents* web_contents =
        WebContents::FromRenderViewHost(render_view_host());
    ContentViewCoreImpl* content_view_core_impl =
        ContentViewCoreImpl::FromWebContents(web_contents);
    video_view_.reset(new ContentVideoView(content_view_core_impl->GetContext(),
        content_view_core_impl->GetContentVideoViewClient(), this));
  }
}

void MediaPlayerManagerImpl::OnExitFullscreen(int player_id) {
  if (fullscreen_player_id_ == player_id) {
    MediaPlayerAndroid* player = GetPlayer(player_id);
    if (player)
      player->SetVideoSurface(gfx::ScopedJavaSurface());
    video_view_->OnExitFullscreen();
  }
}

void MediaPlayerManagerImpl::OnInitialize(
    int player_id,
    const GURL& url,
    media::MediaPlayerAndroid::SourceType source_type,
    const GURL& first_party_for_cookies) {
  RemovePlayer(player_id);

  RenderProcessHost* host = render_view_host()->GetProcess();
  AddPlayer(media::MediaPlayerAndroid::Create(
      player_id, url, source_type, first_party_for_cookies,
      host->GetBrowserContext()->IsOffTheRecord(), this));
}

void MediaPlayerManagerImpl::OnStart(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Start();
}

void MediaPlayerManagerImpl::OnSeek(int player_id, base::TimeDelta time) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->SeekTo(time);
}

void MediaPlayerManagerImpl::OnPause(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->Pause();
}

void MediaPlayerManagerImpl::OnReleaseResources(int player_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  // Don't release the fullscreen player when tab visibility changes,
  // it will be released when user hit the back/home button or when
  // OnDestroyPlayer is called.
  if (player && player_id != fullscreen_player_id_)
    player->Release();

#if defined(GOOGLE_TV)
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  if (view)
    view->NotifyExternalSurface(player_id, false, gfx::RectF());
#endif
}

void MediaPlayerManagerImpl::OnDestroyPlayer(int player_id) {
  RemovePlayer(player_id);
  if (fullscreen_player_id_ == player_id)
    fullscreen_player_id_ = -1;
}

void MediaPlayerManagerImpl::OnDemuxerReady(
    int player_id,
    const media::MediaPlayerHostMsg_DemuxerReady_Params& params) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->DemuxerReady(params);
}

void MediaPlayerManagerImpl::OnReadFromDemuxerAck(
    int player_id,
    const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->ReadFromDemuxerAck(params);
}

void MediaPlayerManagerImpl::OnMediaSeekRequestAck(
    int player_id, unsigned seek_request_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->OnSeekRequestAck(seek_request_id);
}

void MediaPlayerManagerImpl::OnInitializeCDM(int media_keys_id,
                                             const std::vector<uint8>& uuid) {
  // TODO(qinmin/xhwang): Create a MediaDrmBridge.
}

void MediaPlayerManagerImpl::OnGenerateKeyRequest(
    int media_keys_id,
    const std::string& type,
    const std::vector<uint8>& init_data) {
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (drm_bridge) {
    drm_bridge->GenerateKeyRequest(type, &init_data[0], init_data.size());
  }
}

void MediaPlayerManagerImpl::OnAddKey(int media_keys_id,
                                      const std::vector<uint8>& key,
                                      const std::vector<uint8>& init_data,
                                      const std::string& session_id) {
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (drm_bridge) {
    drm_bridge->AddKey(&key[0], key.size(), &init_data[0], init_data.size(),
                       session_id);
  }
}

void MediaPlayerManagerImpl::OnCancelKeyRequest(int media_keys_id,
                                                const std::string& session_id) {
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (drm_bridge)
    drm_bridge->CancelKeyRequest(session_id);
}

void MediaPlayerManagerImpl::OnDurationChanged(
    int player_id, const base::TimeDelta& duration) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (player)
    player->DurationChanged(duration);
}

void MediaPlayerManagerImpl::AddPlayer(MediaPlayerAndroid* player) {
  DCHECK(!GetPlayer(player->player_id()));
  players_.push_back(player);
}

void MediaPlayerManagerImpl::RemovePlayer(int player_id) {
  for (ScopedVector<MediaPlayerAndroid>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it)->player_id() == player_id) {
      players_.erase(it);
      break;
    }
  }
}

void MediaPlayerManagerImpl::AddDrmBridge(int media_keys_id,
                                          const std::vector<uint8>& uuid) {
  DCHECK(!GetDrmBridge(media_keys_id));
  drm_bridges_.push_back(MediaDrmBridge::Create(media_keys_id, uuid));
}

void MediaPlayerManagerImpl::RemoveDrmBridge(int media_keys_id) {
  for (ScopedVector<MediaDrmBridge>::iterator it = drm_bridges_.begin();
      it != drm_bridges_.end(); ++it) {
    if ((*it)->media_keys_id() == media_keys_id) {
      drm_bridges_.erase(it);
      break;
    }
  }
}

void MediaPlayerManagerImpl::OnSetMediaKeys(int player_id, int media_keys_id) {
  MediaPlayerAndroid* player = GetPlayer(player_id);
  if (!player)
    return;
  MediaDrmBridge* drm_bridge = GetDrmBridge(media_keys_id);
  if (!drm_bridge)
    return;
  // TODO(qinmin): add the logic to decide whether we should create the
  // fullscreen surface for EME lv1.
  player->SetDrmBridge(drm_bridge);
}

}  // namespace content
