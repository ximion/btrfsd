
btrfsd_src = [
    'btd-btrfs-mount.h',
    'btd-btrfs-mount.c',
    'btd-scheduler.h',
    'btd-scheduler.c',
    'btd-utils.h',
    'btd-utils.c',
]

btrfsd_deps = [
    glib_dep,
    gobject_dep,
    gio_dep,
    json_glib_dep,
    mount_dep,
]

btrfsd_lib = static_library(
    'btrfsd',
    sources: [btrfsd_src],
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
