test_exe = executable('rinswf-parser-test', 'parser_test.cpp', dependencies: rinswf_dep)
test('parser', test_exe)
tags_test_exe = executable('rinswf-tags-test', 'tags_test.cpp', dependencies: rinswf_dep)
test('tags', tags_test_exe)

display_test_exe = executable('rinswf-display-list-test', 'display_list_test.cpp', dependencies: rinswf_dep)
test('display-list', display_test_exe)

shapes_test_exe = executable('rinswf-shapes-test', 'shapes_test.cpp', dependencies: rinswf_dep)
test('shapes', shapes_test_exe)

bitmap_test_exe = executable('rinswf-bitmaps-test', 'bitmaps_test.cpp', dependencies: [rinswf_dep, zlib_dep])
test('bitmaps', bitmap_test_exe)

jpeg_test_exe = executable('rinswf-jpeg-test', 'jpeg_test.cpp', dependencies: [rinswf_dep, zlib_dep])
test('jpeg-assets', jpeg_test_exe)

renderer_test_exe = executable('rinswf-renderer-test', 'renderer_test.cpp', dependencies: rinswf_dep)
test('renderer', renderer_test_exe)

frame_render_test_exe = executable('rinswf-frame-renderer-test', 'frame_renderer_test.cpp', dependencies: rinswf_dep)
test('frame-renderer', frame_render_test_exe)

avm1_test_exe = executable('rinswf-avm1-test', 'avm1_test.cpp', dependencies: rinswf_dep)
test('avm1', avm1_test_exe)
