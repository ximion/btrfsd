# Meson definition for Btrfsd Tests

test_btd_exe = executable ('as-test_misc',
    ['test-btd.c'],
    dependencies: btrfsd_deps,
    link_with: btrfsd_lib,
    include_directories: [root_inc_dir, include_directories ('../src')]
)
test ('test_btd',
    test_btd_exe,
)
