// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var onStanzaStr = null;
var onError = null;
var parser = null;

module('XmppStreamParser', {
  setup: function() {
    onStanzaStr = sinon.spy();
    onError = sinon.spy();
    function onStanza(stanza) {
      onStanzaStr(new XMLSerializer().serializeToString(stanza));
    }
    parser = new remoting.XmppStreamParser();
    parser.setCallbacks(onStanza, onError);
  }
});


test('should parse XMPP stream', function() {
  parser.appendData(base.encodeUtf8('<stream><iq>text</iq>'));
  sinon.assert.calledWith(onStanzaStr, '<iq>text</iq>');
});

test('should handle multiple incoming stanzas', function() {
  parser.appendData(base.encodeUtf8('<stream><iq>text</iq><iq>more text</iq>'));
  sinon.assert.calledWith(onStanzaStr, '<iq>text</iq>');
  sinon.assert.calledWith(onStanzaStr, '<iq>more text</iq>');
});

test('should ignore whitespace between stanzas', function() {
  parser.appendData(base.encodeUtf8('<stream> <iq>text</iq>'));
  sinon.assert.calledWith(onStanzaStr, '<iq>text</iq>');
});

test('should assemble messages from small chunks', function() {
  parser.appendData(base.encodeUtf8('<stream><i'));
  parser.appendData(base.encodeUtf8('q>'));

  // Split one UTF-8 sequence into two chunks
  var data = base.encodeUtf8('😃');
  parser.appendData(data.slice(0, 2));
  parser.appendData(data.slice(2));

  parser.appendData(base.encodeUtf8('</iq>'));

  sinon.assert.calledWith(onStanzaStr, '<iq>😃</iq>');
});

test('should stop parsing on errors', function() {
  parser.appendData(base.encodeUtf8('<stream>error<iq>text</iq>'));
  sinon.assert.calledWith(onError);
  sinon.assert.notCalled(onStanzaStr);
});

test('should fail on invalid stream header', function() {
  parser.appendData(base.encodeUtf8('<stream p=\'>'));
  sinon.assert.calledWith(onError);
});

test('should fail on loose text', function() {
  parser.appendData(base.encodeUtf8('stream'));
  sinon.assert.calledWith(onError);
});

test('should fail on loose text with incomplete UTF-8 sequences', function() {
  var buffer = base.encodeUtf8('<stream>ф')
  // Crop last byte.
  buffer = buffer.slice(0, buffer.byteLength - 1);
  parser.appendData(buffer);
  sinon.assert.calledWith(onError);
});

test('should fail on incomplete UTF-8 sequences', function() {
  var buffer = base.encodeUtf8('<stream><iq>ф')
  // Crop last byte.
  buffer = buffer.slice(0, buffer.byteLength - 1);
  parser.appendData(buffer);
  parser.appendData(base.encodeUtf8('</iq>'));
  sinon.assert.calledWith(onError);
});

})();
