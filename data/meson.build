
systemd_unit_dir = systemd_dep.get_variable(pkgconfig: 'systemdsystemunitdir')

bdsd_data = configuration_data()
bdsd_data.set('BTRFSD_INSTALL_BIN', get_option('libexecdir') / 'btrfsd')

bdsd_service = configure_file(
    input: 'btrfsd.service.in',
    output: 'btrfsd.service',
    configuration: bdsd_data
)

install_data(bdsd_service,
             install_dir: systemd_unit_dir)
install_data('btrfsd.timer',
             install_dir: systemd_unit_dir)
