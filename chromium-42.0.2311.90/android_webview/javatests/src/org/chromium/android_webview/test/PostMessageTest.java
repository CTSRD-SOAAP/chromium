// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Build;
import android.test.suitebuilder.annotation.SmallTest;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;
import static org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwMessagePortService;
import org.chromium.android_webview.MessagePort;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;

/**
 * The tests for content postMessage API.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
public class PostMessageTest extends AwTestBase {

    private static final String SOURCE_ORIGIN = "";
    // Timeout to failure, in milliseconds
    private static final long TIMEOUT = scaleTimeout(5000);

    // Inject to the page to verify received messages.
    private static class MessageObject {
        private boolean mReady;
        private String mData;
        private String mOrigin;
        private int[] mPorts;
        private Object mLock = new Object();

        public void setMessageParams(String data, String origin, int[] ports) {
            synchronized (mLock) {
                mData = data;
                mOrigin = origin;
                mPorts = ports;
                mReady = true;
                mLock.notify();
            }
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                if (!mReady) mLock.wait(TIMEOUT);
            }
        }

        public String getData() {
            return mData;
        }

        public String getOrigin() {
            return mOrigin;
        }

        public int[] getPorts() {
            return mPorts;
        }
    }

    private MessageObject mMessageObject;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mMessageObject = new MessageObject();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        enableJavaScriptOnUiThread(mAwContents);

        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mAwContents.addPossiblyUnsafeJavascriptInterface(mMessageObject,
                            "messageObject", null);
                }
            });
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        mWebServer = TestWebServer.start();
    }

    @Override
    protected void tearDown() throws Exception {
        mWebServer.shutdown();
        super.tearDown();
    }

    private static final String WEBVIEW_MESSAGE = "from_webview";
    private static final String JS_MESSAGE = "from_js";

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            if (e.ports != null && e.ports.length > 0) {"
            + "               e.ports[0].postMessage(\"" + JS_MESSAGE + "\");"
            + "            }"
            + "        }"
            + "   </script>"
            + "</body></html>";

    private void loadPage(String page) throws Throwable {
        final String url = mWebServer.setResponse("/test.html", page,
                CommonResources.getTextHtmlHeaders(true));
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        loadPage(TEST_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                        null);
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSamePortTwiceViaPostMessageToFrameNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                mAwContents.postMessageToFrame(null, "1", mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
                // Retransfer the port. This should fail with an exception.
                try {
                    mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(),
                            new MessagePort[]{channel[1]});
                } catch (IllegalStateException ex) {
                    latch.countDown();
                    return;
                }
                fail();
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // channel[0] and channel[1] are entangled ports, establishing a channel. Verify
    // it is not allowed to transfer channel[0] on channel[0].postMessage.
    // TODO(sgurun) Note that the related case of posting channel[1] via
    // channel[0].postMessage does not throw a JS exception at present. We do not throw
    // an exception in this case either since the information of entangled port is not
    // available at the source port. We need a new mechanism to implement to prevent
    // this case.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferSourcePortViaMessageChannelNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                try {
                    channel[0].postMessage("1", new MessagePort[]{channel[0]});
                } catch (IllegalStateException ex) {
                    latch.countDown();
                    return;
                }
                fail();
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a frame.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToFrameNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                channel[1].close();
                try {
                    mAwContents.postMessageToFrame(null, "1", mWebServer.getBaseUrl(),
                            new MessagePort[]{channel[1]});
                } catch (IllegalStateException ex) {
                    latch.countDown();
                    return;
                }
                fail();
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a port.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel1 = mAwContents.createMessageChannel();
                MessagePort[] channel2 = mAwContents.createMessageChannel();
                channel2[1].close();
                try {
                    channel1[0].postMessage("1", new MessagePort[]{channel2[1]});
                } catch (IllegalStateException ex) {
                    latch.countDown();
                    return;
                }
                fail();
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    // Verify messages cannot be posted to closed ports.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToClosedPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                channel[0].close();
                try {
                    channel[0].postMessage("1", null);
                } catch (IllegalStateException ex) {
                    latch.countDown();
                    return;
                }
                fail();
            }
        });
        boolean ignore = latch.await(TIMEOUT, java.util.concurrent.TimeUnit.MILLISECONDS);
    }

    private static class ChannelContainer {
        private boolean mReady;
        private MessagePort[] mChannel;
        private Object mLock = new Object();
        private String mMessage;

        public void set(MessagePort[] channel) {
            mChannel = channel;
        }
        public MessagePort[] get() {
            return mChannel;
        }

        public void setMessage(String message) {
            synchronized (mLock) {
                mMessage = message;
                mReady = true;
                mLock.notify();
            }
        }

        public String getMessage() {
            return mMessage;
        }

        public void waitForMessage() throws InterruptedException {
            synchronized (mLock) {
                if (!mReady) mLock.wait(TIMEOUT);
            }
        }
    }

    // Verify that messages from JS can be waited on a UI thread.
    // TODO(sgurun) this test turned out to be flaky. When it fails, it always fails in IPC.
    // When a postmessage is received, an IPC message is sent from browser to renderer
    // to convert the postmessage from WebSerializedScriptValue to a string. The IPC is sent
    // and seems to be received by IPC in renderer, but then nothing else seems to happen.
    // The issue seems like blocking the UI thread causes a racing SYNC ipc from renderer
    // to browser to block waiting for UI thread, and this would in turn block renderer
    // doing the conversion.
    @DisabledTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testReceiveMessageInBackgroundThread() throws Throwable {
        loadPage(TEST_PAGE);
        final ChannelContainer channelContainer = new ChannelContainer();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                // verify communication from JS to Java.
                channelContainer.set(channel);
                channel[0].setWebEventHandler(new MessagePort.WebEventHandler() {
                    @Override
                    public void onMessage(String message) {
                        channelContainer.setMessage(message);
                    }
                });
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WEBVIEW_MESSAGE, mMessageObject.getData());
        assertEquals(SOURCE_ORIGIN, mMessageObject.getOrigin());
        // verify that one message port is received at the js side
        assertEquals(1, mMessageObject.getPorts().length);
        // wait until we receive a message from JS
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    channelContainer.waitForMessage();
                } catch (InterruptedException e) {
                    // ignore.
                }
            }
        });
        assertEquals(JS_MESSAGE, channelContainer.getMessage());
    }

    private static final String ECHO_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        onmessage = function (e) {"
            + "            var myPort = e.ports[0];"
            + "            myPort.onmessage = function(e) {"
            + "                myPort.postMessage(e.data + \"" + JS_MESSAGE + "\"); }"
            + "        }"
            + "   </script>"
            + "</body></html>";


    // Call on non-UI thread.
    private void waitUntilPortReady(final MessagePort port) throws Throwable {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(
                        new Callable<Boolean>() {
                            @Override
                            public Boolean call() throws Exception {
                                return port.isReady();
                            }
                        });
            }
        });
    }

    private static final String HELLO = "HELLO";

    // Message channels are created on UI thread in a pending state. They are
    // initialized at a later stage. Verify that a message port that is initialized
    // can be transferred to JS and full communication can happen on it.
    // Do this by sending a message to JS and let it echo'ing the message with
    // some text prepended to it.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelUsingInitializedPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        final MessagePort[] channel = ThreadUtils.runOnUiThreadBlocking(
                new Callable<MessagePort[]>() {
                    @Override
                    public MessagePort[] call() {
                        return mAwContents.createMessageChannel();
                    }
                });

        waitUntilPortReady(channel[0]);
        waitUntilPortReady(channel[1]);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                channel[0].setWebEventHandler(new MessagePort.WebEventHandler() {
                    @Override
                    public void onMessage(String message) {
                        channelContainer.setMessage(message);
                    }
                });
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
                channel[0].postMessage(HELLO, null);
            }
        });
        // wait for the asynchronous response from JS
        channelContainer.waitForMessage();
        assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    // Verify that a message port can be used immediately (even if it is in
    // pending state) after creation. In particular make sure the message port can be
    // transferred to JS and full communication can happen on it.
    // Do this by sending a message to JS and let it echo'ing the message with
    // some text prepended to it.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelUsingPendingPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                channel[0].setWebEventHandler(new MessagePort.WebEventHandler() {
                    @Override
                    public void onMessage(String message) {
                        channelContainer.setMessage(message);
                    }
                });
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
                channel[0].postMessage(HELLO, null);
            }
        });
        // Wait for the asynchronous response from JS.
        channelContainer.waitForMessage();
        assertEquals(HELLO + JS_MESSAGE, channelContainer.getMessage());
    }

    // Verify that a message port can be used for message transfer when both
    // ports are owned by same Webview.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelCommunicationWithinWebView() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                channel[1].setWebEventHandler(new MessagePort.WebEventHandler() {
                    @Override
                    public void onMessage(String message) {
                        channelContainer.setMessage(message);
                    }
                });
                channel[0].postMessage(HELLO, null);
            }
        });
        // Wait for the asynchronous response from JS.
        channelContainer.waitForMessage();
        assertEquals(HELLO, channelContainer.getMessage());
    }

    // concats all the data fields of the received messages and makes it
    // available as page title.
    private static final String TITLE_PAGE =
            "<!DOCTYPE html><html><body>"
            + "    <script>"
            + "        var received = \"\";"
            + "        onmessage = function (e) {"
            + "            received = received + e.data;"
            + "            document.title = received;"
            + "        }"
            + "   </script>"
            + "</body></html>";

    // Call on non-UI thread.
    private void expectTitle(final String title) throws Throwable {
        assertTrue("Received title " + mAwContents.getTitle() + " while expecting " + title,
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ThreadUtils.runOnUiThreadBlockingNoException(
                                new Callable<Boolean>() {
                                    @Override
                                    public Boolean call() throws Exception {
                                        return mAwContents.getTitle().equals(title);
                                    }
                                });
                    }
                }));
    }

    // Post a message with a pending port to a frame and then post a bunch of messages
    // after that. Make sure that they are not ordered at the receiver side.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToFrameNotReordersMessages() throws Throwable {
        loadPage(TITLE_PAGE);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                mAwContents.postMessageToFrame(null, "1", mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
                mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(), null);
                mAwContents.postMessageToFrame(null, "3", mWebServer.getBaseUrl(), null);
            }
        });
        expectTitle("123");
    }

    private static class TestMessagePort extends MessagePort {

        private boolean mReady;
        private MessagePort mPort;
        private Object mLock = new Object();

        public TestMessagePort(AwMessagePortService service) {
            super(service);
        }

        public void setMessagePort(MessagePort port) {
            mPort = port;
        }

        public void setReady(boolean ready) {
            synchronized (mLock) {
                mReady = ready;
            }
        }
        @Override
        public boolean isReady() {
            synchronized (mLock) {
                return mReady;
            }
        }
        @Override
        public int portId() {
            return mPort.portId();
        }
        @Override
        public void setPortId(int id) {
            mPort.setPortId(id);
        }
        @Override
        public void close() {
            mPort.close();
        }
        @Override
        public boolean isClosed() {
            return mPort.isClosed();
        }
        @Override
        public void setWebEventHandler(WebEventHandler handler) {
            mPort.setWebEventHandler(handler);
        }
        @Override
        public void onMessage(String message) {
            mPort.onMessage(message);
        }
        @Override
        public void postMessage(String message, MessagePort[] msgPorts) throws
                IllegalStateException {
            mPort.postMessage(message, msgPorts);
        }
    }

    // Post a message with a pending port to a frame and then post a message that
    // is pending after that. Make sure that when first message becomes ready,
    // the subsequent not-ready message is not sent.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToFrameNotSendsPendingMessages() throws Throwable {
        loadPage(TITLE_PAGE);
        final TestMessagePort testPort =
                new TestMessagePort(getAwBrowserContext().getMessagePortService());
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                mAwContents.postMessageToFrame(null, "1", mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[1]});
                mAwContents.postMessageToFrame(null, "2", mWebServer.getBaseUrl(), null);
                MessagePort[] channel2 = mAwContents.createMessageChannel();
                // Test port is in a pending state so it should not be transferred.
                testPort.setMessagePort(channel2[0]);
                mAwContents.postMessageToFrame(null, "3", mWebServer.getBaseUrl(),
                        new MessagePort[]{testPort});
            }
        });
        expectTitle("12");
    }

    private static final String WORKER_MESSAGE = "from_worker";

    // Listen for messages. Pass port 1 to worker and use port 2 to receive messages from
    // from worker.
    private static final String TEST_PAGE_FOR_PORT_TRANSFER =
            "<!DOCTYPE html><html><body>"
            + "    <script type=\"text/javascript\">"
            + "        var worker = new Worker(\"worker.js\");"
            + "        onmessage = function (e) {"
            + "            if (e.data == \"" + WEBVIEW_MESSAGE + "\") {"
            + "                worker.postMessage(\"worker_port\", [e.ports[0]]);"
            + "                var messageChannelPort = e.ports[1];"
            + "                messageChannelPort.onmessage = receiveWorkerMessage;"
            + "            }"
            + "        };"
            + "        function receiveWorkerMessage(e) {"
            + "            if (e.data == \"" + WORKER_MESSAGE + "\") {"
            + "                messageObject.setMessageParams(e.data, e.origin, e.ports);"
            + "            }"
            + "        };"
            + "   </script>"
            + "</body></html>";

    private static final String WORKER_SCRIPT =
            "onmessage = function(e) {"
            + "    if (e.data == \"worker_port\") {"
            + "        var toWindow = e.ports[0];"
            + "        toWindow.postMessage(\"" + WORKER_MESSAGE + "\");"
            + "        toWindow.start();"
            + "    }"
            + "}";

    // Test if message ports created at the native side can be transferred
    // to JS side, to establish a communication channel between a worker and a frame.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferPortsToWorker() throws Throwable {
        mWebServer.setResponse("/worker.js", WORKER_SCRIPT,
                CommonResources.getTextJavascriptHeaders(true));
        loadPage(TEST_PAGE_FOR_PORT_TRANSFER);
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                MessagePort[] channel = mAwContents.createMessageChannel();
                mAwContents.postMessageToFrame(null, WEBVIEW_MESSAGE, mWebServer.getBaseUrl(),
                        new MessagePort[]{channel[0], channel[1]});
            }
        });
        mMessageObject.waitForMessage();
        assertEquals(WORKER_MESSAGE, mMessageObject.getData());
    }
}
