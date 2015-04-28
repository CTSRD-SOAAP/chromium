// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Proxy extends core.MojoEventStreamListener {
  Map<int, Completer> _completerMap;
  int _nextId = 0;

  Proxy(core.MojoMessagePipeEndpoint endpoint) :
      _completerMap = {},
      super(endpoint);

  Proxy.fromHandle(core.MojoHandle handle) :
      _completerMap = {},
      super.fromHandle(handle);

  Proxy.unbound() :
      _completerMap = {},
      super.unbound();

  void handleResponse(ServiceMessage reader);

  void handleRead() {
    // Query how many bytes are available.
    var result = endpoint.query();
    assert(result.status.isOk || result.status.isResourceExhausted);

    // Read the data.
    var bytes = new ByteData(result.bytesRead);
    var handles = new List<core.MojoHandle>(result.handlesRead);
    result = endpoint.read(bytes, result.bytesRead, handles);
    assert(result.status.isOk || result.status.isResourceExhausted);
    var message = new ServiceMessage.fromMessage(new Message(bytes, handles));
    handleResponse(message);
  }

  void handleWrite() {
    throw 'Unexpected write signal in proxy.';
  }

  void sendMessage(Struct message, int name) {
    if (!isOpen) {
      listen();
    }
    var header = new MessageHeader(name);
    var serviceMessage = message.serializeWithHeader(header);
    endpoint.write(serviceMessage.buffer,
                   serviceMessage.buffer.lengthInBytes,
                   serviceMessage.handles);
    if (!endpoint.status.isOk) {
      throw "message pipe write failed";
    }
  }

  Future sendMessageWithRequestId(
      Struct message, int name, int id, int flags) {
    if (!isOpen) {
      listen();
    }
    if (id == -1) {
      id = _nextId++;
    }

    var header = new MessageHeader.withRequestId(name, flags, id);
    var serviceMessage = message.serializeWithHeader(header);
    endpoint.write(serviceMessage.buffer,
                   serviceMessage.buffer.lengthInBytes,
                   serviceMessage.handles);
    if (!endpoint.status.isOk) {
      throw "message pipe write failed";
    }

    var completer = new Completer();
    _completerMap[id] = completer;
    return completer.future;
  }

  // Need a getter for this for access in subclasses.
  Map<int, Completer> get completerMap => _completerMap;
}
