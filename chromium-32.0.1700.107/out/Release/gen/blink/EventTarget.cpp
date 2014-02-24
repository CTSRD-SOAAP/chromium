/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/events/EventTargetFactory.h"

#include "EventTargetHeaders.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

PassRefPtr<EventTarget> EventTargetFactory::create(const String& type)
{
    if (type == "FontFaceSet")
        return FontFaceSet::create();
    if (type == "MessagePort")
        return MessagePort::create();
    if (type == "Node")
        return Node::create();
    if (type == "WebKitNamedFlow")
        return NamedFlow::create();
    if (type == "FileReader")
        return FileReader::create();
    if (type == "MediaController")
        return MediaController::create();
    if (type == "InputMethodContext")
        return InputMethodContext::create();
    if (type == "TextTrack")
        return TextTrack::create();
    if (type == "TextTrackCue")
        return TextTrackCue::create();
    if (type == "TextTrackList")
        return TextTrackList::create();
    if (type == "ApplicationCache")
        return ApplicationCache::create();
    if (type == "EventSource")
        return EventSource::create();
    if (type == "Performance")
        return Performance::create();
    if (type == "Window")
        return DOMWindow::create();
    if (type == "SVGElementInstance")
        return SVGElementInstance::create();
    if (type == "DedicatedWorkerGlobalScope")
        return DedicatedWorkerGlobalScope::create();
    if (type == "SharedWorker")
        return SharedWorker::create();
    if (type == "SharedWorkerGlobalScope")
        return SharedWorkerGlobalScope::create();
    if (type == "Worker")
        return Worker::create();
    if (type == "XMLHttpRequest")
        return XMLHttpRequest::create();
    if (type == "XMLHttpRequestUpload")
        return XMLHttpRequestUpload::create();
#if ENABLE(ENCRYPTED_MEDIA_V2)
    if (type == "MediaKeySession")
        return MediaKeySession::create();
#endif
    if (type == "FileWriter")
        return FileWriter::create();
    if (type == "IDBDatabase")
        return IDBDatabase::create();
    if (type == "IDBOpenDBRequest")
        return IDBOpenDBRequest::create();
    if (type == "IDBRequest")
        return IDBRequest::create();
    if (type == "IDBTransaction")
        return IDBTransaction::create();
    if (type == "MediaSource")
        return MediaSource::create();
    if (type == "SourceBuffer")
        return SourceBuffer::create();
    if (type == "SourceBufferList")
        return SourceBufferList::create();
    if (type == "WebKitMediaSource")
        return WebKitMediaSource::create();
    if (type == "WebKitSourceBufferList")
        return WebKitSourceBufferList::create();
    if (type == "MediaStream")
        return MediaStream::create();
    if (type == "MediaStreamTrack")
        return MediaStreamTrack::create();
    if (type == "RTCDTMFSender")
        return RTCDTMFSender::create();
    if (type == "RTCDataChannel")
        return RTCDataChannel::create();
    if (type == "RTCPeerConnection")
        return RTCPeerConnection::create();
    if (type == "Notification")
        return Notification::create();
    if (type == "SpeechRecognition")
        return SpeechRecognition::create();
    if (type == "SpeechSynthesisUtterance")
        return SpeechSynthesisUtterance::create();
#if ENABLE(WEB_AUDIO)
    if (type == "AudioContext")
        return AudioContext::create();
#endif
#if ENABLE(WEB_AUDIO)
    if (type == "AudioNode")
        return AudioNode::create();
#endif
    if (type == "MIDIAccess")
        return MIDIAccess::create();
    if (type == "MIDIInput")
        return MIDIInput::create();
    if (type == "MIDIPort")
        return MIDIPort::create();
    if (type == "WebSocket")
        return WebSocket::create();
    return 0;
}

} // namespace WebCore
