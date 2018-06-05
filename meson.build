project('gst-sensors', 'c',
    version: '0.1',
    meson_version : '>= 0.45.0',
    license: 'LGPL2+',
    default_options: [
        'c_std=c11',
        'buildtype=debug',
        'warning_level=3',
        'werror=true',
        ])

cflags = [
    '-Wno-unused-parameter',
    '-DHAVE_CONFIG_H=1'
]
add_project_arguments(cflags, language: 'c')

# Config header generation.
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('GST_LICENSE', meson.project_license())
conf.set_quoted('GST_PACKAGE_NAME', meson.project_name())
conf.set_quoted('GST_PACKAGE_ORIGIN', 'github/XevoInc')
configure_file(
    input: 'src/config.h.meson',
    output: 'config.h',
    configuration : conf)

# Dependencies.
libgps_dep = dependency('libgps')
deps = [
    dependency('gstreamer-1.0'),
    dependency('gstreamer-base-1.0'),
    libgps_dep
]

# gstreamer package variables
plugins_install_dir = '@0@/gstreamer-1.0'.format(get_option('libdir'))

# gstsensors library.
src_files = [
    'src/gstgpsd.c',
]
lib = library(
    'gstsensors',
    src_files,
    install: true,
    install_dir: plugins_install_dir,
    dependencies: deps)
gst_sensors_dep = declare_dependency(link_with: lib)

# Examples.
executable(
    'gpscrack',
    ['example/gpscrack.c'],
    dependencies: libgps_dep
)
