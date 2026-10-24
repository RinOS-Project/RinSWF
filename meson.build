project('rinswf', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++20', 'warning_level=3'])
zlib_dep = dependency('zlib', required: true)
inc = include_directories('include')
rinswf_lib = library('rinswf', ['src/swf.cpp', 'src/tags.cpp', 'src/display_list.cpp'], include_directories: inc,
  dependencies: zlib_dep, install: true, version: meson.project_version())
rinswf_dep = declare_dependency(link_with: rinswf_lib, include_directories: inc)
install_headers('include/rinswf/swf.hpp', 'include/rinswf/tags.hpp', 'include/rinswf/display_list.hpp', subdir: 'rinswf')
pkg = import('pkgconfig')
pkg.generate(rinswf_lib, name: 'RinSWF', description: 'SWF parser and runtime library',
  filebase: 'rinswf', subdirs: 'rinswf')
subdir('tests')
