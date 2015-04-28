# Copyright 2014 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'includes': [
    '../build/crashpad.gypi',
  ],
  'targets': [
    {
      'target_name': 'crashpad_snapshot',
      'type': 'static_library',
      'dependencies': [
        '../client/client.gyp:crashpad_client',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpu_architecture.h',
        'cpu_context.cc',
        'cpu_context.h',
        'exception_snapshot.h',
        'mac/cpu_context_mac.cc',
        'mac/cpu_context_mac.h',
        'mac/crashpad_info_client_options.cc',
        'mac/crashpad_info_client_options.h',
        'mac/exception_snapshot_mac.cc',
        'mac/exception_snapshot_mac.h',
        'mac/mach_o_image_annotations_reader.cc',
        'mac/mach_o_image_annotations_reader.h',
        'mac/mach_o_image_reader.cc',
        'mac/mach_o_image_reader.h',
        'mac/mach_o_image_segment_reader.cc',
        'mac/mach_o_image_segment_reader.h',
        'mac/mach_o_image_symbol_table_reader.cc',
        'mac/mach_o_image_symbol_table_reader.h',
        'mac/memory_snapshot_mac.cc',
        'mac/memory_snapshot_mac.h',
        'mac/module_snapshot_mac.cc',
        'mac/module_snapshot_mac.h',
        'mac/process_reader.cc',
        'mac/process_reader.h',
        'mac/process_snapshot_mac.cc',
        'mac/process_snapshot_mac.h',
        'mac/process_types.cc',
        'mac/process_types.h',
        'mac/process_types/all.proctype',
        'mac/process_types/crashpad_info.proctype',
        'mac/process_types/crashreporterclient.proctype',
        'mac/process_types/custom.cc',
        'mac/process_types/dyld_images.proctype',
        'mac/process_types/flavors.h',
        'mac/process_types/internal.h',
        'mac/process_types/loader.proctype',
        'mac/process_types/nlist.proctype',
        'mac/process_types/traits.h',
        'mac/system_snapshot_mac.cc',
        'mac/system_snapshot_mac.h',
        'mac/thread_snapshot_mac.cc',
        'mac/thread_snapshot_mac.h',
        'minidump/minidump_simple_string_dictionary_reader.cc',
        'minidump/minidump_simple_string_dictionary_reader.h',
        'minidump/minidump_string_list_reader.cc',
        'minidump/minidump_string_list_reader.h',
        'minidump/minidump_string_reader.cc',
        'minidump/minidump_string_reader.h',
        'minidump/module_snapshot_minidump.cc',
        'minidump/module_snapshot_minidump.h',
        'minidump/process_snapshot_minidump.cc',
        'minidump/process_snapshot_minidump.h',
        'memory_snapshot.h',
        'module_snapshot.h',
        'process_snapshot.h',
        'system_snapshot.h',
        'thread_snapshot.h',
        'win/process_reader_win.cc',
        'win/process_reader_win.h',
        'win/system_snapshot_win.cc',
        'win/system_snapshot_win.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lpowrprof.lib',
              '-lversion.lib',
            ],
          },
        }],
      ]
    },
    {
      'target_name': 'crashpad_snapshot_test_lib',
      'type': 'static_library',
      'dependencies': [
        'crashpad_snapshot',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/test_cpu_context.cc',
        'test/test_cpu_context.h',
        'test/test_exception_snapshot.cc',
        'test/test_exception_snapshot.h',
        'test/test_memory_snapshot.cc',
        'test/test_memory_snapshot.h',
        'test/test_module_snapshot.cc',
        'test/test_module_snapshot.h',
        'test/test_process_snapshot.cc',
        'test/test_process_snapshot.h',
        'test/test_system_snapshot.cc',
        'test/test_system_snapshot.h',
        'test/test_thread_snapshot.cc',
        'test/test_thread_snapshot.h',
      ],
    },
    {
      'target_name': 'crashpad_snapshot_test',
      'type': 'executable',
      'dependencies': [
        'crashpad_snapshot',
        '../client/client.gyp:crashpad_client',
        '../compat/compat.gyp:crashpad_compat',
        '../third_party/gtest/gtest.gyp:gtest',
        '../third_party/gtest/gtest.gyp:gtest_main',
        '../third_party/mini_chromium/mini_chromium.gyp:base',
        '../util/util.gyp:crashpad_util',
        '../util/util.gyp:crashpad_util_test_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'cpu_context_test.cc',
        'mac/cpu_context_mac_test.cc',
        'mac/crashpad_info_client_options_test.cc',
        'mac/mach_o_image_annotations_reader_test.cc',
        'mac/mach_o_image_reader_test.cc',
        'mac/mach_o_image_segment_reader_test.cc',
        'mac/process_reader_test.cc',
        'mac/process_types_test.cc',
        'mac/system_snapshot_mac_test.cc',
        'minidump/process_snapshot_minidump_test.cc',
        'win/system_snapshot_win_test.cc',
      ],
      'conditions': [
        ['OS=="mac"', {
          'dependencies': [
            'crashpad_snapshot_test_module',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenCL.framework',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'crashpad_snapshot_test_module',
          'type': 'loadable_module',
          'dependencies': [
            '../client/client.gyp:crashpad_client',
            '../third_party/mini_chromium/mini_chromium.gyp:base',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'mac/crashpad_info_client_options_test_module.cc',
          ],
        },
      ],
    }],
  ],
}
