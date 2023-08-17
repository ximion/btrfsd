
btrfsd_src = [
    'btd-btrfs-mount.h',
    'btd-btrfs-mount.c',
    'btd-mount-record.h',
    'btd-mount-record.c',
    'btd-scheduler.h',
    'btd-scheduler.c',
    'btd-utils.h',
    'btd-utils.c',
]

btrfsd_res = glib.compile_resources (
    'btrfsd-resources', 'btrfsd.gresource.xml',
    c_name: 'btd',
    source_dir: ['../data']
)

btrfsd_deps = [
    glib_dep,
    gobject_dep,
    gio_dep,
    json_glib_dep,
    mount_dep,
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
