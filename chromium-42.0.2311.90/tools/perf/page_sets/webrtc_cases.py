# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class WebrtcCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(WebrtcCasesPage, self).__init__(url=url, page_set=page_set)

    with open(os.path.join(os.path.dirname(__file__),
                           'webrtc_track_peerconnections.js')) as javascript:
      self.script_to_evaluate_on_commit = javascript.read()


class Page1(WebrtcCasesPage):

  """ Why: Acquires a vga local stream. """

  def __init__(self, page_set):
    super(Page1, self).__init__(
        url=('http://googlechrome.github.io/webrtc/samples/web/content/'
             'getusermedia/gum/'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(10)


class Page2(WebrtcCasesPage):

  """ Why: Sets up a local WebRTC call. """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'peerconnection/pc1/'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ClickElement('button[id="startButton"]')
    action_runner.Wait(2)
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)
    action_runner.ClickElement('button[id="hangupButton"]')


class Page3(WebrtcCasesPage):

  """ Why: Acquires a high definition local stream. """

  def __init__(self, page_set):
    super(Page3, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'getusermedia/resolution/'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ClickElement('button[id="hd"]')
    action_runner.Wait(10)


class Page4(WebrtcCasesPage):

  """ Why: Sets up a WebRTC audio call with Opus. """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'peerconnection/audio/?codec=OPUS'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('codecSelector.value="OPUS";')
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)


class Page5(WebrtcCasesPage):

  """ Why: Sets up a WebRTC audio call with G722. """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'peerconnection/audio/?codec=G722'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('codecSelector.value="G722";')
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)


class Page6(WebrtcCasesPage):

  """ Why: Sets up a WebRTC audio call with PCMU. """

  def __init__(self, page_set):
    super(Page6, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'peerconnection/audio/?codec=PCMU'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('codecSelector.value="PCMU";')
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)


class Page7(WebrtcCasesPage):

  """ Why: Sets up a WebRTC audio call with iSAC 16K. """

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'peerconnection/audio/?codec=ISAC_16K'),
      page_set=page_set)

  def RunPageInteractions(self, action_runner):
    action_runner.ExecuteJavaScript('codecSelector.value="ISAC/16000";')
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)


class WebrtcCasesPageSet(page_set_module.PageSet):

  """ WebRTC tests for Real-time audio and video communication. """

  def __init__(self):
    super(WebrtcCasesPageSet, self).__init__(
      archive_data_file='data/webrtc_cases.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddUserStory(Page1(self))
    self.AddUserStory(Page2(self))
    self.AddUserStory(Page3(self))
    self.AddUserStory(Page1(self))
    self.AddUserStory(Page2(self))
    self.AddUserStory(Page3(self))
    self.AddUserStory(Page4(self))
    self.AddUserStory(Page5(self))
    self.AddUserStory(Page6(self))
    self.AddUserStory(Page7(self))
