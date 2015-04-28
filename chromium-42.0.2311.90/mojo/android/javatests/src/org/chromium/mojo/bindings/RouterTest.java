// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.mojo.MojoTestCase;
import org.chromium.mojo.bindings.BindingsTestUtils.CapturingErrorHandler;
import org.chromium.mojo.bindings.BindingsTestUtils.RecordingMessageReceiverWithResponder;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.nio.ByteBuffer;
import java.util.ArrayList;

/**
 * Testing {@link Router}
 */
public class RouterTest extends MojoTestCase {

    private MessagePipeHandle mHandle;
    private Router mRouter;
    private RecordingMessageReceiverWithResponder mReceiver;
    private CapturingErrorHandler mErrorHandler;

    /**
     * @see MojoTestCase#setUp()
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Core core = CoreImpl.getInstance();
        Pair<MessagePipeHandle, MessagePipeHandle> handles = core.createMessagePipe(null);
        mHandle = handles.first;
        mRouter = new RouterImpl(handles.second);
        mReceiver = new RecordingMessageReceiverWithResponder();
        mRouter.setIncomingMessageReceiver(mReceiver);
        mErrorHandler = new CapturingErrorHandler();
        mRouter.setErrorHandler(mErrorHandler);
        mRouter.start();
    }

    /**
     * Testing sending a message via the router that expected a response.
     */
    @SmallTest
    public void testSendingToRouterWithResponse() {
        final int requestMessageType = 0xdead;
        final int responseMessageType = 0xbeaf;

        // Sending a message expecting a response.
        MessageHeader header = new MessageHeader(requestMessageType,
                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, 0);
        Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        header.encode(encoder);
        mRouter.acceptWithResponder(encoder.getMessage(), mReceiver);
        ByteBuffer receiveBuffer = ByteBuffer.allocateDirect(header.getSize());
        MessagePipeHandle.ReadMessageResult result = mHandle.readMessage(receiveBuffer, 0,
                MessagePipeHandle.ReadFlags.NONE);

        assertEquals(MojoResult.OK, result.getMojoResult());
        MessageHeader receivedHeader = new Message(
                receiveBuffer, new ArrayList<Handle>()).asServiceMessage().getHeader();

        assertEquals(header.getType(), receivedHeader.getType());
        assertEquals(header.getFlags(), receivedHeader.getFlags());
        assertTrue(receivedHeader.getRequestId() != 0);

        // Sending the response.
        MessageHeader responseHeader = new MessageHeader(responseMessageType,
                MessageHeader.MESSAGE_IS_RESPONSE_FLAG, receivedHeader.getRequestId());
        encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        responseHeader.encode(encoder);
        Message responseMessage = encoder.getMessage();
        mHandle.writeMessage(responseMessage.getData(), new ArrayList<Handle>(),
                MessagePipeHandle.WriteFlags.NONE);
        runLoopUntilIdle();

        assertEquals(1, mReceiver.messages.size());
        ServiceMessage receivedResponseMessage = mReceiver.messages.get(0).asServiceMessage();
        assertEquals(MessageHeader.MESSAGE_IS_RESPONSE_FLAG,
                receivedResponseMessage.getHeader().getFlags());
        assertEquals(responseMessage.getData(), receivedResponseMessage.getData());
    }

    /**
     * Testing receiving a message via the router that expected a response.
     */
    @SmallTest
    public void testReceivingViaRouterWithResponse() {
        final int requestMessageType = 0xdead;
        final int responseMessageType = 0xbeef;
        final int requestId = 0xdeadbeaf;

        // Sending a message expecting a response.
        MessageHeader header = new MessageHeader(requestMessageType,
                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, requestId);
        Encoder encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        header.encode(encoder);
        Message headerMessage = encoder.getMessage();
        mHandle.writeMessage(headerMessage.getData(), new ArrayList<Handle>(),
                MessagePipeHandle.WriteFlags.NONE);
        runLoopUntilIdle();

        assertEquals(1, mReceiver.messagesWithReceivers.size());
        Pair<Message, MessageReceiver> receivedMessage =
                mReceiver.messagesWithReceivers.get(0);
        assertEquals(headerMessage.getData(), receivedMessage.first.getData());

        // Sending the response.
        MessageHeader responseHeader = new MessageHeader(responseMessageType,
                MessageHeader.MESSAGE_EXPECTS_RESPONSE_FLAG, requestId);
        encoder = new Encoder(CoreImpl.getInstance(), header.getSize());
        responseHeader.encode(encoder);
        Message message = encoder.getMessage();
        receivedMessage.second.accept(message);
        ByteBuffer receivedResponseMessage = ByteBuffer.allocateDirect(responseHeader.getSize());
        MessagePipeHandle.ReadMessageResult result = mHandle.readMessage(receivedResponseMessage, 0,
                MessagePipeHandle.ReadFlags.NONE);

        assertEquals(MojoResult.OK, result.getMojoResult());
        assertEquals(message.getData(), receivedResponseMessage);
    }
}
