# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 0,
    'cc_tests_source_files': [
      'hash_pair_unittest.cc',
      'active_animation_unittest.cc',
      'damage_tracker_unittest.cc',
      'delay_based_time_source_unittest.cc',
      'draw_quad_unittest.cc',
      'delegated_renderer_layer_impl_unittest.cc',
      'frame_rate_controller_unittest.cc',
      'heads_up_display_unittest.cc',
      'keyframed_animation_curve_unittest.cc',
      'layer_animation_controller_unittest.cc',
      'layer_impl_unittest.cc',
      'layer_iterator_unittest.cc',
      'layer_quad_unittest.cc',
      'layer_sorter_unittest.cc',
      'layer_tree_host_common_unittest.cc',
      'layer_tree_host_impl_unittest.cc',
      'layer_tree_host_unittest.cc',
      'math_util_unittest.cc',
      'occlusion_tracker_unittest.cc',
      'prioritized_texture_unittest.cc',
      'quad_culler_unittest.cc',
      'software_renderer_unittest.cc',
      'render_pass_unittest.cc',
      'render_surface_filters_unittest.cc',
      'render_surface_unittest.cc',
      'gl_renderer_unittest.cc',
      'resource_provider_unittest.cc',
      'scheduler_state_machine_unittest.cc',
      'scheduler_unittest.cc',
      'scoped_texture_unittest.cc',
      'scrollbar_animation_controller_linear_fade_unittest.cc',
      'solid_color_layer_impl_unittest.cc',
      'texture_update_controller_unittest.cc',
      'thread_task_unittest.cc',
      'CCThreadedTest.h',
      'tiled_layer_impl_unittest.cc',
      'timer_unittest.cc',
      'content_layer_unittest.cc',
      'float_quad_unittest.cc',
      'layer_unittest.cc',
      'scrollbar_layer_unittest.cc',
      'texture_copier_unittest.cc',
      'texture_layer_unittest.cc',
      'texture_uploader_unittest.cc',
      'tiled_layer_unittest.cc',
      'tree_synchronizer_unittest.cc',
    ],
    'cc_tests_support_files': [
      'test/animation_test_common.cc',
      'test/animation_test_common.h',
      'test/compositor_fake_web_graphics_context_3d.h',
      'test/fake_graphics_context.h',
      'test/fake_graphics_context_3d_unittest.cc',
      'test/fake_layer_tree_host_client.cc',
      'test/fake_layer_tree_host_client.h',
      'test/fake_web_compositor_output_surface.h',
      'test/fake_web_compositor_software_output_device.h',
      'test/fake_web_graphics_context_3d.h',
      'test/fake_web_scrollbar_theme_geometry.h',
      'test/geometry_test_utils.cc',
      'test/geometry_test_utils.h',
      'test/layer_test_common.cc',
      'test/layer_test_common.h',
      'test/layer_tree_test_common.cc',
      'test/layer_tree_test_common.h',
      'test/mock_quad_culler.cc',
      'test/mock_quad_culler.h',
      'test/occlusion_tracker_test_common.h',
      'test/render_pass_test_common.h',
      'test/scheduler_test_common.cc',
      'test/scheduler_test_common.h',
      'test/test_common.h',
      'test/tiled_layer_test_common.cc',
      'test/tiled_layer_test_common.h',
      'test/web_compositor_initializer.h',
    ],
  },
  'targets': [
    {
      'target_name': 'cc_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
        'cc.gyp:cc',
        'cc_test_support',
      ],
      'sources': [
        'test/run_all_unittests.cc',
        '<@(cc_tests_source_files)',
      ],
      'include_dirs': [
        'stubs',
        'test',
        '.',
        '../third_party/WebKit/Source/Platform/chromium',
      ],
      'conditions': [
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      'target_name': 'cc_test_support',
      'type': 'static_library',
      'include_dirs': [
        'stubs',
        'test',
        '.',
        '..',
        '../third_party/WebKit/Source/Platform/chromium',
      ],
      'dependencies': [
        '../ui/gl/gl.gyp:gl',
        '../testing/gtest.gyp:gtest',
        '../testing/gmock.gyp:gmock',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/Source/WTF/WTF.gyp/WTF.gyp:wtf',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit_wtf_support',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '../webkit/compositor_bindings/compositor_bindings.gyp:webkit_compositor_support',
        '../webkit/support/webkit_support.gyp:glue',
      ],
      'sources': [
        '<@(cc_tests_support_files)',
        'test/test_webkit_platform.cc',
        'test/test_webkit_platform.h',
      ],
    },
  ],
  'conditions': [
    # Special target to wrap a gtest_target_type==shared_library
    # cc_unittests into an android apk for execution.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'cc_unittests_apk',
          'type': 'none',
          'dependencies': [
            'cc_unittests',
          ],
          'variables': {
            'test_suite_name': 'cc_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)cc_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
