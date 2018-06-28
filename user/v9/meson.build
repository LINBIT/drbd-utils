lfiles_adm_9=lgen.process('drbdadm_scanner.fl')
lfiles_meta=lgen.process('../shared/drbdmeta_scanner.fl')

drbdsetup_9_sources = files('''
  registry.c
  drbdsetup.c
  drbdtool_common.c
  config_flags.c
  drbd_nla.c
  drbdsetup_colors.c
  ../shared/libgenl.c
  ../shared/wrap_printf.c
  ../shared/shared_tool.c
  ../../drbd-headers/drbd_strings.c
'''.split())
drbdsetup_9_sources += [drbd_buildtag_h, drbd_buildtag_c]

drbdadm_9_sources = files('''
  drbdadm_parser.c
  drbdadm_main.c
  drbdadm_postparse.c
  drbdadm_dump.c
  drbdadm_adjust.c
  drbdtool_common.c
  drbdadm_usage_cnt.c
  registry.c
  config_flags.c
  drbd_nla.c
  ../shared/libgenl.c
  ../shared/shared_tool.c
  ../shared/shared_main.c
  ../shared/shared_parser.c
  ../../drbd-headers/drbd_strings.c
'''.split())
drbdadm_9_sources += [lfiles_adm_9]
drbdadm_9_sources += [drbd_buildtag_h, drbd_buildtag_c]

drbdmeta_sources = files('''
  ../shared/drbdmeta.c
  drbdtool_common.c
  ../shared/shared_tool.c
  ../../drbd-headers/drbd_strings.c
'''.split())
drbdmeta_sources += [lfiles_meta]
drbdmeta_sources += [drbd_buildtag_h, drbd_buildtag_c]

incdir = include_directories('../../drbd-headers', '../../user', '../shared', '../..')
idir = get_option('sbindir')
executable('drbdsetup', drbdsetup_9_sources,
	include_directories: incdir, install_dir: idir)
executable('drbdadm', drbdadm_9_sources,
	include_directories: incdir, install_dir: idir)
executable('drbdmeta', drbdmeta_sources,
	include_directories: incdir, install_dir: idir)
