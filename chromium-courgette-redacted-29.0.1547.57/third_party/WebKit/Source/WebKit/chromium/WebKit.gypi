#
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'variables': {
        'webkit_unittest_files': [
            'tests/ArenaTestHelpers.h',
            'tests/AssociatedURLLoaderTest.cpp',
            'tests/Canvas2DLayerBridgeTest.cpp',
            'tests/Canvas2DLayerManagerTest.cpp',
            'tests/ChromeClientImplTest.cpp',
            'tests/DragImageTest.cpp',
            'tests/DrawingBufferTest.cpp',
            'tests/FakeWebPlugin.cpp',
            'tests/FakeWebPlugin.h',
            'tests/FakeWebGraphicsContext3D.h',
            'tests/FilterOperationsTest.cpp',
            'tests/FrameLoaderClientImplTest.cpp',
            'tests/FrameTestHelpers.cpp',
            'tests/FrameTestHelpers.h',
            'tests/GraphicsContextTest.cpp',
            'tests/GraphicsLayerTest.cpp',
            'tests/IDBBindingUtilitiesTest.cpp',
            'tests/IDBKeyPathTest.cpp',
            'tests/IDBRequestTest.cpp',
            'tests/IDBTransactionTest.cpp',
            'tests/ImageFilterBuilderTest.cpp',
            'tests/ImageLayerChromiumTest.cpp',
            'tests/KeyboardTest.cpp',
            'tests/LinkHighlightTest.cpp',
            'tests/ListenerLeakTest.cpp',
            'tests/MemoryInfo.cpp',
            'tests/OpaqueRectTrackingContentLayerDelegateTest.cpp',
            'tests/OpenTypeVerticalDataTest.cpp',
            'tests/PODArenaTest.cpp',
            'tests/PODIntervalTreeTest.cpp',
            'tests/PODRedBlackTreeTest.cpp',
            'tests/PaintAggregatorTest.cpp',
            'tests/PopupContainerTest.cpp',
            'tests/PrerenderingTest.cpp',
            'tests/ProgrammaticScrollTest.cpp',
            'tests/RegionTest.cpp',
            'tests/RenderLayerBackingTest.cpp',
            'tests/RenderTableCellTest.cpp',
            'tests/RenderTableRowTest.cpp',
            'tests/ScrollingCoordinatorChromiumTest.cpp',
            'tests/TreeTestHelpers.cpp',
            'tests/TreeTestHelpers.h',
            'tests/URLTestHelpers.cpp',
            'tests/URLTestHelpers.h',
            'tests/WebFrameTest.cpp',
            'tests/WebImageTest.cpp',
            'tests/WebInputEventConversionTest.cpp',
            'tests/WebInputEventFactoryTestMac.mm',
            'tests/WebPageNewSerializerTest.cpp',
            'tests/WebPageSerializerTest.cpp',
            'tests/WebPluginContainerTest.cpp',
            'tests/WebUserGestureTokenTest.cpp',
            'tests/WebURLRequestTest.cpp',
            'tests/WebURLResponseTest.cpp',
            'tests/WebViewTest.cpp',
        ],

        'conditions': [
            ['OS=="win"', {
                'webkit_unittest_files': [
                    'tests/LocaleWinTest.cpp',
                    # FIXME: Port PopupMenuTest to Linux and Mac.
                    'tests/PopupMenuTest.cpp',
                    'tests/TransparencyWinTest.cpp',
                    'tests/UniscribeHelperTest.cpp',
                    'tests/WebPageNewSerializerTest.cpp',
                    'tests/WebPageSerializerTest.cpp',
                ],
            }],
            ['OS=="mac"', {
                'webkit_unittest_files': [
                    'tests/LocaleMacTest.cpp',
                ],
            }],
            ['OS!="mac"', {
                'webkit_unittest_files': [
                    # Mac uses ScrollAnimatorMac instead of ScrollAnimatorNone.
                    'tests/ScrollAnimatorNoneTest.cpp',
                ],
            }],
            ['os_posix==1 and OS!="mac"', {
                'webkit_unittest_files': [
                    'tests/LocaleICUTest.cpp',
                ],
            }],
            ['toolkit_uses_gtk == 1', {
                'webkit_unittest_files': [
                    'tests/KeyCodeConversionTestGtk.cpp',
                    'tests/WebInputEventFactoryTestGtk.cpp',
                ],
            }],
        ],
    },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
