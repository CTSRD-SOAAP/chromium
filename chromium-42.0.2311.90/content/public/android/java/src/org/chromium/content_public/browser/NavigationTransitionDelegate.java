// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

/**
 * An interface that allows the embedder to be notified of navigation transition
 * related events and respond to them.
 */
public interface NavigationTransitionDelegate {
    /**
     * Called when the navigation is deferred immediately after the response started.
     * @param markup The markup coming as part of response.
     * @param cssSelector The CSS selectors, which is to be applied to transition layer's
     *                    makrup.
     * @param enteringColor The background color of the entering document, as a String
     *                      representing a legal CSS color value. This is inserted into
     *                      the transition layer's markup after the entering stylesheets
     *                      have been applied.
     */
    public void didDeferAfterResponseStarted(
            String markup, String cssSelector, String enteringColor);

    /**
     * Called when a navigation transition has been detected, and we need to check
     * if it's supported.
     */
    public boolean willHandleDeferAfterResponseStarted();

    /**
     * Called when the navigation is deferred immediately after the response
     * started.
     */
    public void addEnteringStylesheetToTransition(String stylesheet);

    /**
     * Notifies that a navigation transition is started for a given frame.
     * @param frameId A positive, non-zero integer identifying the navigating frame.
     */
    public void didStartNavigationTransitionForFrame(long frameId);

    /**
     * Add transition element's name, position and size.
     * @param name The name of the transition element.
     * @param x The x position of the transition element.
     * @param y The y position of the transition element.
     * @param width The width of the transition element.
     * @param height The height of the transition element.
     */
    public void addNavigationTransitionElements(String name, int x, int y, int width, int height);

    /**
     * Called immediately after transition elements are added.
     * @param cssSelector The CSS selector, which is to be used by Activity Transitions
     *                    or applied to the transition layer's markup later.
     */
    public void onTransitionElementsFetched(String cssSelector);
}
