
btrfsd_src = [
    'btrfsd.c',
]

btrfsd_exe = executable('btrfsd',
    [btrfsd_src],
    dependencies: [
        glib_dep,
        gobject_dep,
        gio_dep,
        json_glib_dep,
    ],
    include_directories: [root_inc_dir],
    install_dir: get_option('libexecdir'),
    install: true
)
