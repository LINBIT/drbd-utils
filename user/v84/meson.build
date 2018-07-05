lfiles_adm_84=lgen.process('drbdadm_scanner.fl')

drbdsetup_84_sources = files('''
  ../shared/libgenl.c
  registry.c
  drbdsetup.c
  drbdtool_common.c
  drbd_strings.c
  config_flags.c
  drbd_nla.c
  drbdsetup_colors.c
  ../shared/wrap_printf.c
  ../shared/shared_tool.c
'''.split())
drbdsetup_84_sources += [drbd_buildtag_h, drbd_buildtag_c]

drbdadm_84_sources = files('''
  drbdadm_parser.c
  drbdadm_main.c
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
'''.split())
drbdadm_84_sources += [lfiles_adm_84]
drbdadm_84_sources += [drbd_buildtag_h, drbd_buildtag_c]

incdir = include_directories('linux', '../shared', '../..')
drbdsetup_84 = executable('drbdsetup-84', drbdsetup_84_sources, include_directories: incdir)
executable('drbdadm-84', drbdadm_84_sources, include_directories: incdir)
