

# make manual pages
xsltproc = find_program('xsltproc')
custom_target('man-btrfsd',
    input: 'btrfsd.8.xml',
    output: 'btrfsd.8',
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man8'),
    command: [
        xsltproc,
        '--nonet',
        '--stringparam', 'man.output.quietly', '1',
        '--stringparam', 'funcsynopsis.style', 'ansi',
        '--stringparam', 'man.th.extra1.suppress', '1',
        '-o', '@OUTPUT@',
        'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
        '@INPUT@'
    ]
)
