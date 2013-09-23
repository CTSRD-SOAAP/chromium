# GYP file to build various tools.
#
# To build on Linux:
#  ./gyp_skia tools.gyp && make tools
#
{
  'includes': [
    'apptype_console.gypi',
  ],
  'targets': [
    {
      # Build all executable targets defined below.
      'target_name': 'tools',
      'type': 'none',
      'dependencies': [
        'bench_pictures',
        'filter',
        'lua_pictures',
        'lua_app',
        'pinspect',
        'render_pdfs',
        'render_pictures',
        'skdiff',
        'skhello',
        'skimage',
      ],
      'conditions': [
        ['skia_shared_lib',
          {
            'dependencies': [
              'sklua', # This can only be built if skia is built as a shared library
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'skdiff',
      'type': 'executable',
      'sources': [
        '../tools/skdiff.cpp',
        '../tools/skdiff.h',
        '../tools/skdiff_html.cpp',
        '../tools/skdiff_html.h',
        '../tools/skdiff_main.cpp',
        '../tools/skdiff_utils.cpp',
        '../tools/skdiff_utils.h',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
      ],
    },
    {
      'target_name': 'skimagediff',
      'type': 'executable',
      'sources': [
        '../tools/skdiff.cpp',
        '../tools/skdiff.h',
        '../tools/skdiff_html.cpp',
        '../tools/skdiff_html.h',
        '../tools/skdiff_image.cpp',
        '../tools/skdiff_utils.cpp',
        '../tools/skdiff_utils.h',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
      ],
    },
    {
      'target_name': 'skhello',
      'type': 'executable',
      'dependencies': [
        'skia_lib.gyp:skia_lib',
      ],
      'conditions': [
        [ 'skia_os == "nacl"', {
          'sources': [
            '../platform_tools/nacl/src/nacl_hello.cpp',
          ],
        }, {
          'sources': [
            '../tools/skhello.cpp',
          ],
          'dependencies': [
            'pdf.gyp:pdf',
            'flags.gyp:flags',
          ],
        }],
      ],
    },
    {
      'target_name': 'skimage',
      'type': 'executable',
      'sources': [
        '../tools/skimage_main.cpp',
      ],
      'include_dirs': [
        # For SkBitmapHasher.h
        '../src/utils/',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'flags.gyp:flags',
        'gm.gyp:gm_expectations',
        'jsoncpp.gyp:jsoncpp',
        'utils.gyp:utils',
      ],
    },

    {
      'target_name': 'lua_app',
      'type': 'executable',
      'sources': [
        '../tools/lua/lua_app.cpp',
        '../src/utils/SkLua.cpp',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'effects.gyp:effects',
        'utils.gyp:utils',
        'images.gyp:images',
        'pdf.gyp:pdf',
        'ports.gyp:ports',
        'lua.gyp:lua',
      ],
    },
    {
      'target_name': 'lua_pictures',
      'type': 'executable',
      'sources': [
        '../tools/lua/lua_pictures.cpp',
        '../src/utils/SkLuaCanvas.cpp',
        '../src/utils/SkLua.cpp',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'effects.gyp:effects',
        'utils.gyp:utils',
        'images.gyp:images',
        'tools.gyp:picture_renderer',
        'tools.gyp:picture_utils',
        'pdf.gyp:pdf',
        'ports.gyp:ports',
        'flags.gyp:flags',
        'lua.gyp:lua',
      ],
    },
    {
      'target_name': 'render_pictures',
      'type': 'executable',
      'sources': [
        '../tools/render_pictures_main.cpp',
      ],
      'include_dirs': [
        '../src/pipe/utils/',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'tools.gyp:picture_renderer',
        'tools.gyp:picture_utils',
        'flags.gyp:flags',
      ],
    },
    {
      'target_name': 'bench_pictures',
      'type': 'executable',
      'sources': [
        '../bench/SkBenchLogger.h',
        '../bench/SkBenchLogger.cpp',
        '../bench/TimerData.h',
        '../bench/TimerData.cpp',
        '../tools/bench_pictures_main.cpp',
        '../tools/PictureBenchmark.cpp',
      ],
      'include_dirs': [
        '../bench',
        '../src/lazy/',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'tools.gyp:picture_utils',
        'tools.gyp:picture_renderer',
        'bench.gyp:bench_timer',
        'flags.gyp:flags',
      ],
    },
    {
      'target_name': 'picture_renderer',
      'type': 'static_library',
      'sources': [
        '../tools/PictureRenderer.h',
        '../tools/PictureRenderer.cpp',
        '../tools/PictureRenderingFlags.h',
        '../tools/PictureRenderingFlags.cpp',
        '../tools/CopyTilesRenderer.h',
        '../tools/CopyTilesRenderer.cpp',
        '../src/pipe/utils/SamplePipeControllers.h',
        '../src/pipe/utils/SamplePipeControllers.cpp',
      ],
      'include_dirs': [
        '../src/core/',
        '../src/pipe/utils/',
        '../src/utils/',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'tools.gyp:picture_utils',
        'flags.gyp:flags',
      ],
      'conditions': [
        ['skia_gpu == 1',
          {
            'include_dirs' : [
              '../src/gpu',
            ],
          },
        ],
      ],
    },
    {
      'target_name': 'render_pdfs',
      'type': 'executable',
      'sources': [
        '../tools/render_pdfs_main.cpp',
        '../tools/PdfRenderer.cpp',
        '../tools/PdfRenderer.h',
      ],
      'include_dirs': [
        '../src/pipe/utils/',
        '../src/utils/',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'pdf.gyp:pdf',
        'tools.gyp:picture_utils',
      ],
      'conditions': [
        ['skia_win_debuggers_path and skia_os == "win"',
          {
            'dependencies': [
              'tools.gyp:win_dbghelp',
            ],
          },
        ],
        # VS static libraries don't have a linker option. We must set a global
        # project linker option, or add it to each executable.
        ['skia_win_debuggers_path and skia_os == "win" and '
         'skia_arch_width == 64',
          {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  '<(skia_win_debuggers_path)/x64/DbgHelp.lib',
                ],
              },
            },
          },
        ],
        ['skia_win_debuggers_path and skia_os == "win" and '
         'skia_arch_width == 32',
          {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  '<(skia_win_debuggers_path)/DbgHelp.lib',
                ],
              },
            },
          },
        ],
      ],
    },
    {
      'target_name': 'picture_utils',
      'type': 'static_library',
      'sources': [
        '../tools/picture_utils.cpp',
        '../tools/picture_utils.h',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
        '../tools/',
        ],
      },
    },
    {
      'target_name': 'pinspect',
      'type': 'executable',
      'sources': [
        '../tools/pinspect.cpp',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'tools.gyp:picture_renderer',
      ],
    },
    {
      'target_name': 'filter',
      'type': 'executable',
      'include_dirs' : [
        '../src/core',
        '../src/utils/debugger',
      ],
      'sources': [
        '../tools/filtermain.cpp',
        '../tools/path_utils.h',
        '../tools/path_utils.cpp',
        '../src/utils/debugger/SkDrawCommand.h',
        '../src/utils/debugger/SkDrawCommand.cpp',
        '../src/utils/debugger/SkDebugCanvas.h',
        '../src/utils/debugger/SkDebugCanvas.cpp',
        '../src/utils/debugger/SkObjectParser.h',
        '../src/utils/debugger/SkObjectParser.cpp',
      ],
      'dependencies': [
        'skia_lib.gyp:skia_lib',
        'tools.gyp:picture_utils',
      ],
    },
  ],
  'conditions': [
    ['skia_shared_lib',
      {
        'targets': [
          {
            'target_name': 'sklua',
            'product_name': 'skia',
            'product_prefix': '',
            'product_dir': '<(PRODUCT_DIR)/',
            'type': 'shared_library',
            'sources': [
              '../src/utils/SkLuaCanvas.cpp',
              '../src/utils/SkLua.cpp',
            ],
            'include_dirs': [
              '../third_party/lua/src/',
            ],
            'dependencies': [
              'lua.gyp:lua',
              'pdf.gyp:pdf',
              'skia_lib.gyp:skia_lib',
            ],
            'conditions': [
              ['skia_os != "win"',
                {
                  'ldflags': [
                    '-Wl,-rpath,\$$ORIGIN,--enable-new-dtags',
                  ],
                },
              ],
            ],
          },
        ],
      },
    ],
    ['skia_win_debuggers_path and skia_os == "win"',
      {
        'targets': [
          {
            'target_name': 'win_dbghelp',
            'type': 'static_library',
            'defines': [
              'SK_CDB_PATH="<(skia_win_debuggers_path)"',
            ],
            'sources': [
              '../tools/win_dbghelp.h',
              '../tools/win_dbghelp.cpp',
            ],
          },
        ],
      },
    ],
    ['skia_os == "win"',
      {
        'targets': [
          {
            'target_name': 'win_lcid',
            'type': 'executable',
            'sources': [
              '../tools/win_lcid.cpp',
            ],
          },
        ],
      },
    ],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
