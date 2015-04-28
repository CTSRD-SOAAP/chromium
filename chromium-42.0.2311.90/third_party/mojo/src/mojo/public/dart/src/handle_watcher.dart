// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;

class _MojoHandleWatcherNatives {
  static int sendControlData(
      int controlHandle, int mojoHandle, SendPort port, int data)
      native "MojoHandleWatcher_SendControlData";
  static List recvControlData(int controlHandle)
      native "MojoHandleWatcher_RecvControlData";
  static int setControlHandle(int controlHandle)
      native "MojoHandleWatcher_SetControlHandle";
  static int getControlHandle()
      native "MojoHandleWatcher_GetControlHandle";
}

// The MojoHandleWatcher sends a stream of events to application isolates that
// register Mojo handles with it. Application isolates make the following calls:
//
// add(handle, port, signals) - Instructs the MojoHandleWatcher isolate to add
//     'handle' to the set of handles it watches, and to notify the calling
//     isolate only for the events specified by 'signals' using the send port
//     'port'
//
// remove(handle) - Instructs the MojoHandleWatcher isolate to remove 'handle'
//     from the set of handles it watches. This allows the application isolate
//     to, e.g., pause the stream of events.
//
// close(handle) - Notifies the HandleWatcherIsolate that a handle it is
//     watching should be removed from its set and closed.
class MojoHandleWatcher {
  // Control commands.
  static const int ADD = 0;
  static const int REMOVE = 1;
  static const int CLOSE = 2;
  static const int TIMER = 3;
  static const int SHUTDOWN = 4;

  static int _encodeCommand(int cmd, [int signals = 0]) =>
      (cmd << 2) | (signals & MojoHandleSignals.kReadWrite);
  static int _decodeCommand(int cmd) => cmd >> 2;

  // The Mojo handle over which control messages are sent.
  int _controlHandle;

  // Whether the handle watcher should shut down.
  bool _shutdown;

  // The list of handles being watched.
  List<int> _handles;
  int _handleCount;

  // A port for each handle on which to send events back to the isolate that
  // owns the handle.
  List<SendPort> _ports;

  // The signals that we care about for each handle.
  List<int> _signals;

  // A mapping from Mojo handles to their indices in _handles.
  Map<int, int> _handleIndices;

  // Since we are not storing wrapped handles, a dummy handle for when we need
  // a MojoHandle.
  MojoHandle _tempHandle;

  // Priority queue of timers registered with the watcher.
  TimerQueue _timerQueue;

  MojoHandleWatcher(this._controlHandle) :
      _shutdown = false,
      _handles = new List<int>(),
      _ports = new List<SendPort>(),
      _signals = new List<int>(),
      _handleIndices = new Map<int, int>(),
      _handleCount = 1,
      _tempHandle = new MojoHandle(MojoHandle.INVALID),
      _timerQueue = new TimerQueue() {
    // Setup control handle.
    _handles.add(_controlHandle);
    _ports.add(null);  // There is no port for the control handle.
    _signals.add(MojoHandleSignals.kReadable);
    _handleIndices[_controlHandle] = 0;
  }

  static void _handleWatcherIsolate(int consumerHandle) {
    MojoHandleWatcher watcher = new MojoHandleWatcher(consumerHandle);
    while (!watcher._shutdown) {
      int deadline = watcher._processTimerDeadlines();
      MojoWaitManyResult mwmr = MojoHandle.waitMany(
          watcher._handles, watcher._signals, deadline);
      if (mwmr.result.isOk && mwmr.index == 0) {
        watcher._handleControlMessage();
      } else if (mwmr.result.isOk && (mwmr.index > 0)) {
        int handle = watcher._handles[mwmr.index];
        // Route event.
        watcher._routeEvent(mwmr.index);
        // Remove the handle from the list.
        watcher._removeHandle(handle);
      } else if (!mwmr.result.isDeadlineExceeded) {
        // Some handle was closed, but not by us.
        // Find it and close it on our side.
        watcher._pruneClosedHandles(mwmr.states);
      }
    }
  }

  void _routeEvent(int idx) {
    int client_handle = _handles[idx];
    var signals = new MojoHandleSignals(_signals[idx]);
    SendPort port = _ports[idx];

    _tempHandle.h = client_handle;
    bool readyWrite = signals.isWritable && _tempHandle.readyWrite;
    bool readyRead = signals.isReadable && _tempHandle.readyRead;
    _tempHandle.h = MojoHandle.INVALID;

    var event = MojoHandleSignals.NONE;
    event += readyRead ? MojoHandleSignals.READABLE : MojoHandleSignals.NONE;
    event += readyWrite ? MojoHandleSignals.WRITABLE : MojoHandleSignals.NONE;
    port.send([signals.value, event.value]);
  }

  void _handleControlMessage() {
    List result = _MojoHandleWatcherNatives.recvControlData(_controlHandle);
    // result[0] = mojo handle if any, or a timer deadline in milliseconds.
    // result[1] = SendPort if any.
    // result[2] = command << 2 | WRITABLE | READABLE

    var signals = new MojoHandleSignals(
        result[2] & MojoHandleSignals.kReadWrite);
    int command = _decodeCommand(result[2]);
    switch (command) {
      case ADD:
        _addHandle(result[0], result[1], signals);
        break;
      case REMOVE:
        _removeHandle(result[0]);
        break;
      case CLOSE:
        _close(result[0]);
        break;
      case TIMER:
        _timer(result[1], result[0]);
        break;
      case SHUTDOWN:
        _shutdownHandleWatcher(result[1]);
        break;
      default:
        throw "Invalid Command: $command";
        break;
    }
  }

  void _addHandle(int mojoHandle, SendPort port, MojoHandleSignals signals) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      _handles.add(mojoHandle);
      _ports.add(port);
      _signals.add(signals.value);
      _handleIndices[mojoHandle] = _handleCount;
      _handleCount++;
    } else {
      assert(_ports[idx] == port);
      assert(_handles[idx] == mojoHandle);
      _signals[idx] |= signals.value;
    }
  }

  void _removeHandle(int mojoHandle) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      throw "Remove on a non-existent handle: idx = $idx.";
    }
    if (idx == 0) {
      throw "The control handle (idx = 0) cannot be removed.";
    }
    // We don't use List.removeAt so that we know how to fix-up _handleIndices.
    if (idx == _handleCount - 1) {
      int handle = _handles[idx];
      _handleIndices[handle] = null;
      _handles.removeLast();
      _signals.removeLast();
      _ports.removeLast();
      _handleCount--;
    } else {
      int last = _handleCount - 1;
      _handleIndices[_handles[idx]] = null;
      _handles[idx] = _handles[last];
      _signals[idx] = _signals[last];
      _ports[idx] = _ports[last];
      _handles.removeLast();
      _signals.removeLast();
      _ports.removeLast();
      _handleIndices[_handles[idx]] = idx;
      _handleCount--;
    }
  }

  void _close(int mojoHandle, {bool pruning : false}) {
    int idx = _handleIndices[mojoHandle];
    if (idx == null) {
      // A client may request to close a handle that has already been closed on
      // the other side and pruned, but before receiving notification from the
      // handle watcher.
      return;
    }
    if (idx == 0) {
      throw "The control handle (idx = 0) cannot be closed.";
    }
    _tempHandle.h = _handles[idx];
    _tempHandle.close();
    _tempHandle.h = MojoHandle.INVALID;
    if (pruning) {
      // If this handle is being pruned, notify the application isolate
      // by sending MojoHandleSignals.PEER_CLOSED.
      _ports[idx].send([_signals[idx], MojoHandleSignals.kPeerClosed]);
    }
    _removeHandle(mojoHandle);
  }

  // Returns the next timer deadline in units of microseconds from 'now'.
  int _processTimerDeadlines() {
    int now = (new DateTime.now()).millisecondsSinceEpoch;
    while (_timerQueue.hasTimer && (now >= _timerQueue.currentTimeout)) {
      _timerQueue.currentPort.send(null);
      _timerQueue.removeCurrent();
      now = (new DateTime.now()).millisecondsSinceEpoch;
    }
    return _timerQueue.hasTimer ? (_timerQueue.currentTimeout - now) * 1000
                                : MojoHandle.DEADLINE_INDEFINITE;
  }

  void _timer(SendPort port, int deadline) {
    _timerQueue.updateTimer(port, deadline);
  }

  void _pruneClosedHandles(List<MojoHandleSignalsState> states) {
    List<int> closed = new List();
    for (var i = 0; i < _handles.length; i++) {
      if (states != null) {
        var signals = new MojoHandleSignals(states[i].satisfied_signals);
        if (signals.isPeerClosed) {
          closed.add(_handles[i]);
        }
      } else {
        _tempHandle.h = _handles[i];
        MojoWaitResult mwr = _tempHandle.wait(MojoHandleSignals.kReadWrite, 0);
        if ((!mwr.result.isOk) && (!mwr.result.isDeadlineExceeded)) {
          closed.add(_handles[i]);
        }
        _tempHandle.h = MojoHandle.INVALID;
      }
    }
    for (var h in closed) {
      _close(h, pruning: true);
    }
    // '_close' updated the '_handles' array, so at this point the '_handles'
    // array and the caller's 'states' array are mismatched.
  }

  void _shutdownHandleWatcher(SendPort shutdownSendPort) {
    _shutdown = true;
    _tempHandle.h = _controlHandle;
    _tempHandle.close();
    _tempHandle.h = MojoHandle.INVALID;
    shutdownSendPort.send(null);
  }

  static MojoResult _sendControlData(MojoHandle mojoHandle,
                                     SendPort port,
                                     int data) {
    int controlHandle = _MojoHandleWatcherNatives.getControlHandle();
    if (controlHandle == MojoHandle.INVALID) {
      return MojoResult.FAILED_PRECONDITION;
    }

    int rawHandle = MojoHandle.INVALID;
    if (mojoHandle != null) {
      rawHandle = mojoHandle.h;
    }
    var result = _MojoHandleWatcherNatives.sendControlData(
        controlHandle, rawHandle, port, data);
    return new MojoResult(result);
  }

  // Starts up the MojoHandleWatcher isolate. Should be called only once
  // per VM process.
  static Future<Isolate> _start() {
    // Make a control message pipe,
    MojoMessagePipe pipe = new MojoMessagePipe();
    int consumerHandle = pipe.endpoints[0].handle.h;
    int producerHandle = pipe.endpoints[1].handle.h;

    // Call setControlHandle with the other end.
    assert(producerHandle != MojoHandle.INVALID);
    _MojoHandleWatcherNatives.setControlHandle(producerHandle);

    // Spawn the handle watcher isolate with the MojoHandleWatcher,
    return Isolate.spawn(_handleWatcherIsolate, consumerHandle);
  }

  // Causes the MojoHandleWatcher isolate to exit. Should be called only
  // once per VM process.
  static void _stop() {
    // Create a port for notification that the handle watcher has shutdown.
    var shutdownReceivePort = new ReceivePort();
    var shutdownSendPort = shutdownReceivePort.sendPort;

    // Send the shutdown command.
    _sendControlData(null, shutdownSendPort, _encodeCommand(SHUTDOWN));

    // Close the control handle.
    int controlHandle = _MojoHandleWatcherNatives.getControlHandle();
    var handle = new MojoHandle(controlHandle);
    handle.close();

    // Invalidate the control handle.
    _MojoHandleWatcherNatives.setControlHandle(MojoHandle.INVALID);

    // Wait for the handle watcher isolate to exit.
    shutdownReceivePort.first.then((_) {
      shutdownReceivePort.close();
    });
  }

  static MojoResult close(MojoHandle mojoHandle) {
    return _sendControlData(mojoHandle, null, _encodeCommand(CLOSE));
  }

  static MojoResult add(MojoHandle mojoHandle, SendPort port, int signals) {
    return _sendControlData(mojoHandle, port, _encodeCommand(ADD, signals));
  }

  static MojoResult remove(MojoHandle mojoHandle) {
    return _sendControlData(mojoHandle, null, _encodeCommand(REMOVE));
  }

  static MojoResult timer(Object ignored, SendPort port, int deadline) {
    // The deadline will be unwrapped before sending to the handle watcher.
    return _sendControlData(
        new MojoHandle(deadline), port, _encodeCommand(TIMER));
  }
}
