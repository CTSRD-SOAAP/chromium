// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of core;


class _MojoDataPipeNatives {
  static List MojoCreateDataPipe(
      int elementBytes, int capacityBytes, int flags)
      native "MojoDataPipe_Create";

  static List MojoWriteData(int handle, ByteData data, int numBytes, int flags)
      native "MojoDataPipe_WriteData";

  static List MojoBeginWriteData(int handle, int bufferBytes, int flags)
      native "MojoDataPipe_BeginWriteData";

  static int MojoEndWriteData(int handle, int bytesWritten)
      native "MojoDataPipe_EndWriteData";

  static List MojoReadData(int handle, ByteData data, int numBytes, int flags)
      native "MojoDataPipe_ReadData";

  static List MojoBeginReadData(int handle, int bufferBytes, int flags)
      native "MojoDataPipe_BeginReadData";

  static int MojoEndReadData(int handle, int bytesRead)
      native "MojoDataPipe_EndReadData";
}


class MojoDataPipeProducer {
  static const int FLAG_NONE = 0;
  static const int FLAG_ALL_OR_NONE = 1 << 0;

  MojoHandle handle;
  MojoResult status;
  final int elementBytes;

  MojoDataPipeProducer(
      this.handle, [this.status = MojoResult.OK, this.elementBytes = 1]);

  int write(ByteData data, [int numBytes = -1, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    int data_numBytes = (numBytes == -1) ? data.lengthInBytes : numBytes;
    List result = _MojoDataPipeNatives.MojoWriteData(
        handle.h, data, data_numBytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  ByteData beginWrite(int bufferBytes, [int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    List result = _MojoDataPipeNatives.MojoBeginWriteData(
        handle.h, bufferBytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  MojoResult endWrite(int bytesWritten) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    int result = _MojoDataPipeNatives.MojoEndWriteData(handle.h, bytesWritten);
    status = new MojoResult(result);
    return status;
  }
}


class MojoDataPipeConsumer {
  static const int FLAG_NONE = 0;
  static const int FLAG_ALL_OR_NONE = 1 << 0;
  static const int FLAG_MAY_DISCARD = 1 << 1;
  static const int FLAG_QUERY = 1 << 2;
  static const int FLAG_PEEK = 1 << 3;

  MojoHandle handle;
  MojoResult status;
  final int elementBytes;

  MojoDataPipeConsumer(
      this.handle, [this.status = MojoResult.OK, this.elementBytes = 1]);

  int read(ByteData data, [int numBytes = -1, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }

    int data_numBytes = (numBytes == -1) ? data.lengthInBytes : numBytes;
    List result = _MojoDataPipeNatives.MojoReadData(
        handle.h, data, data_numBytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  ByteData beginRead([int bufferBytes = 0, int flags = 0]) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    List result = _MojoDataPipeNatives.MojoBeginReadData(
        handle.h, bufferBytes, flags);
    if (result == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return null;
    }

    assert((result is List) && (result.length == 2));
    status = new MojoResult(result[0]);
    return result[1];
  }

  MojoResult endRead(int bytesRead) {
    if (handle == null) {
      status = MojoResult.INVALID_ARGUMENT;
      return status;
    }
    int result = _MojoDataPipeNatives.MojoEndReadData(handle.h, bytesRead);
    status = new MojoResult(result);
    return status;
  }

  int query() => read(null, 0, FLAG_QUERY);
}


class MojoDataPipe {
  static const int FLAG_NONE = 0;
  static const int FLAG_MAY_DISCARD = 1 << 0;
  static const int DEFAULT_ELEMENT_SIZE = 1;
  static const int DEFAULT_CAPACITY = 0;

  MojoDataPipeProducer producer;
  MojoDataPipeConsumer consumer;
  MojoResult status;

  MojoDataPipe._internal() {
    producer = null;
    consumer = null;
    status = MojoResult.OK;
  }

  factory MojoDataPipe([int elementBytes = DEFAULT_ELEMENT_SIZE,
                        int capacityBytes = DEFAULT_CAPACITY,
                        int flags = FLAG_NONE]) {
    List result = _MojoDataPipeNatives.MojoCreateDataPipe(
        elementBytes, capacityBytes, flags);
    if (result == null) {
      return null;
    }
    assert((result is List) && (result.length == 3));
    MojoHandle producerHandle = new MojoHandle(result[1]);
    MojoHandle consumerHandle = new MojoHandle(result[2]);
    MojoDataPipe pipe = new MojoDataPipe._internal();
    pipe.producer = new MojoDataPipeProducer(
        producerHandle, new MojoResult(result[0]), elementBytes);
    pipe.consumer = new MojoDataPipeConsumer(
        consumerHandle, new MojoResult(result[0]), elementBytes);
    pipe.status = new MojoResult(result[0]);
    return pipe;
  }
}
