// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.content.browser.WebContentsObserver;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;

/**
 * Routes notifications from WebContents to AwContentsClient and other listeners.
 */
public class AwWebContentsObserver extends WebContentsObserver {
    private final WeakReference<AwContents> mAwContents;
    private final AwContentsClient mAwContentsClient;
    private boolean mStartedNonApiProvisionalLoadInMainFrame = false;

    public AwWebContentsObserver(
            WebContents webContents, AwContents awContents,  AwContentsClient awContentsClient) {
        super(webContents);
        mAwContents = new WeakReference<AwContents>(awContents);
        mAwContentsClient = awContentsClient;
    }

    boolean hasStartedNonApiProvisionalLoadInMainFrame() {
        return mStartedNonApiProvisionalLoadInMainFrame;
    }

    @Override
    public void didFinishLoad(long frameId, String validatedUrl, boolean isMainFrame) {
        String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        boolean isErrorUrl =
                unreachableWebDataUrl != null && unreachableWebDataUrl.equals(validatedUrl);
        if (isMainFrame && !isErrorUrl) {
            mAwContentsClient.onPageFinished(validatedUrl);
        }
    }

    @Override
    public void didFailLoad(boolean isProvisionalLoad,
            boolean isMainFrame, int errorCode, String description, String failingUrl) {
        String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        boolean isErrorUrl =
                unreachableWebDataUrl != null && unreachableWebDataUrl.equals(failingUrl);
        if (isMainFrame && !isErrorUrl) {
            if (errorCode != NetError.ERR_ABORTED) {
                // This error code is generated for the following reasons:
                // - WebView.stopLoading is called,
                // - the navigation is intercepted by the embedder via shouldOverrideNavigation.
                //
                // The Android WebView does not notify the embedder of these situations using
                // this error code with the WebViewClient.onReceivedError callback.
                mAwContentsClient.onReceivedError(
                        ErrorCodeConversionHelper.convertErrorCode(errorCode), description,
                                failingUrl);
            }
            // Need to call onPageFinished after onReceivedError (if there is an error) for
            // backwards compatibility with the classic webview.
            mAwContentsClient.onPageFinished(failingUrl);
        }
    }

    @Override
    public void didNavigateMainFrame(String url, String baseUrl,
            boolean isNavigationToDifferentPage, boolean isFragmentNavigation) {
        // This is here to emulate the Classic WebView firing onPageFinished for main frame
        // navigations where only the hash fragment changes.
        if (isFragmentNavigation) {
            mAwContentsClient.onPageFinished(url);
        }
    }

    @Override
    public void didNavigateAnyFrame(String url, String baseUrl, boolean isReload) {
        mAwContentsClient.doUpdateVisitedHistory(url, isReload);
    }

    @Override
    public void didStartProvisionalLoadForFrame(
            long frameId,
            long parentFrameId,
            boolean isMainFrame,
            String validatedUrl,
            boolean isErrorPage,
            boolean isIframeSrcdoc) {
        if (!isMainFrame) return;
        AwContents awContents = mAwContents.get();
        if (awContents != null) {
            NavigationEntry pendingEntry = awContents.getNavigationController().getPendingEntry();
            if (pendingEntry != null
                    && (pendingEntry.getTransition() & PageTransition.FROM_API) == 0) {
                mStartedNonApiProvisionalLoadInMainFrame = true;
            }
        }
    }
}
