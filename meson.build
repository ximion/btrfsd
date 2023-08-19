project('Btrfsd', 'c',
  meson_version: '>=0.60',
  default_options: ['c_std=c11', 'cpp_std=gnu++14'],

  license: 'LGPL-2.1+',
  version: '0.1.0',
)

cc = meson.get_compiler('c')
source_root = meson.project_source_root()

btrfsd_version = meson.project_version()

varr = btrfsd_version.split('.')
btrfsd_major_version = varr[0]
btrfsd_minor_version = varr[1]
btrfsd_micro_version = varr[2]

#
# Configure files
#
conf = configuration_data()
conf.set('BTRFSD_MAJOR_VERSION_CONF', btrfsd_major_version)
conf.set('BTRFSD_MINOR_VERSION_CONF', btrfsd_minor_version)
conf.set('BTRFSD_MICRO_VERSION_CONF', btrfsd_micro_version)
conf.set_quoted('PACKAGE_VERSION', btrfsd_version)
conf.set_quoted('GETTEXT_PACKAGE', 'btrfsd')
conf.set_quoted('LOCALEDIR',
                get_option('prefix') / get_option('localedir'))
conf.set_quoted('SHAREDSTATEDIR',
                get_option('prefix') / get_option('sharedstatedir'))
conf.set_quoted('LIBEXECDIR',
                get_option('prefix') / get_option('libexecdir'))
conf.set_quoted('DATADIR',
                get_option('prefix') / get_option('datadir'))
conf.set_quoted('LIBDIR',
                get_option('prefix') / get_option('libdir'))
conf.set_quoted('BINDIR',
                get_option('prefix') / get_option('bindir'))
conf.set_quoted('SYSCONFDIR',
                get_option('prefix') / get_option('sysconfdir'))
conf.set_quoted('BTRFS_CMD', '/usr/bin/btrfs')
conf.set('_DEFAULT_SOURCE', true)

configure_file(output: 'config.h', configuration: conf)
root_inc_dir = include_directories ('.')

if get_option('static-analysis') and host_machine.system() != 'windows'
  if cc.get_id() != 'gcc'
    error('You need to compile with GCC to run the static analyzer!')
  endif

  # enable statuc analyzer
  add_project_arguments(['-fanalyzer'], language : 'c')

  # make false-positive non-fatal with GCC 12 for now
  if cc.version().version_compare('>11<13')
    add_project_arguments(['-Wno-error=analyzer-use-of-uninitialized-value'], language : 'c')
  endif
endif

#
# Custom C flags
#
sanitizer_libs = []
if get_option('maintainer')
    maintainer_c_args = ['-Werror',
                         '-Wall',
                         '-Wextra',
                         '-Wcast-align',
                         '-Wno-uninitialized',
                         '-Wempty-body',
                         '-Winit-self',
                         '-Wnull-dereference',
                         '-Wfloat-equal',
                         '-Winline',
                         '-Wno-error=comment',
    ]
    add_project_arguments(maintainer_c_args, language: 'c')
endif

# a few compiler warning flags we always want enabled
add_project_arguments(
  cc.get_supported_arguments([
    '-Werror=shadow',
    '-Werror=empty-body',
    '-Werror=strict-prototypes',
    '-Werror=missing-prototypes',
    '-Werror=implicit-function-declaration',
    '-Werror=pointer-arith',
    '-Werror=missing-declarations',
    '-Werror=return-type',
    '-Werror=int-conversion',
    '-Werror=incompatible-pointer-types',
    '-Werror=misleading-indentation',
    '-Werror=missing-include-dirs',
    '-Werror=declaration-after-statement',
    '-Werror=format-security',

    '-Wno-missing-field-initializers',
    '-Wno-error=missing-field-initializers',
    '-Wno-unused-parameter',
    '-Wno-error=unused-parameter',
  ]),
  language: 'c'
)

if cc.get_id() == 'clang'
    # Clang doesn't understand autofree helpers on GMutexLocker and thinks
    # these variables are irrelevant, so this warning when used with Clang
    # gives many false-positives
    add_project_arguments(
      '-Wno-unused-variable',
      language: 'c'
    )
endif

#
# Dependencies
#
systemd_dep = dependency('systemd')
glib_dep = dependency('glib-2.0', version: '>= 2.72')
gobject_dep = dependency('gobject-2.0', version: '>= 2.72')
gio_dep = dependency('gio-2.0', version: '>= 2.72')
json_glib_dep = dependency('json-glib-1.0', version: '>= 1.6.2')
mount_dep = dependency('mount')

#
# Modules
#
fs   = import('fs')
glib = import('gnome')

#
# Directories
#
subdir('src/')
subdir('data/')
subdir('tests/')
subdir('docs/')
