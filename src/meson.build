
btrfsd_src = [
    'btrfsd.c',
    'btd-btrfs-mount.h',
    'btd-btrfs-mount.c',
    'btd-utils.h',
    'btd-utils.c',
]

btrfsd_exe = executable('btrfsd',
    [btrfsd_src],
    dependencies: [
        glib_dep,
        gobject_dep,
        gio_dep,
        json_glib_dep,
        mount_dep,
    ],
    include_directories: [root_inc_dir],
    install_dir: get_option('libexecdir'),
    install: true
)
