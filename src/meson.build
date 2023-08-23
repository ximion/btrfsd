
btrfsd_src = [
    'btd-filesystem.h',
    'btd-filesystem.c',
    'btd-fs-record.h',
    'btd-fs-record.c',
    'btd-mailer.h',
    'btd-mailer.c',
    'btd-scheduler.h',
    'btd-scheduler.c',
    'btd-logging.h',
    'btd-logging.c',
    'btd-utils.h',
    'btd-utils.c',
]

btrfsd_res = glib.compile_resources (
    'btd-resources', 'btrfsd.gresource.xml',
    c_name: 'btd',
    source_dir: ['../data']
)

btrfsd_deps = [
    glib_dep,
    gobject_dep,
    gio_dep,
    json_glib_dep,
    mount_dep,
    libsystemd_dep,
]

btrfsd_lib = static_library(
    'btrfsd',
    sources: [btrfsd_src, btrfsd_res],
    dependencies: btrfsd_deps,
    include_directories: [root_inc_dir],
)

btrfsd_exe = executable(
    'btrfsd',
    ['btrfsd.c'],
    dependencies: btrfsd_deps,
    link_with: btrfsd_lib,
    include_directories: [root_inc_dir],
    install_dir: get_option('libexecdir'),
    install: true,
)
