lfiles_83=lgen.process('drbdadm_scanner.fl')

drbdsetup_83_sources = files('''
  drbdsetup.c
  drbdtool_common.c
  drbd_strings.c
  ../shared/shared_tool.c
'''.split())
drbdsetup_83_sources += [drbd_buildtag_h, drbd_buildtag_c]

drbdadm_83_sources = files('''
  drbdadm_parser.c
  drbdadm_main.c
  drbdadm_adjust.c
  drbdtool_common.c
  drbdadm_usage_cnt.c
  drbdadm_minor_table.c
  ../shared/shared_tool.c
  ../shared/shared_main.c
  ../shared/shared_parser.c
'''.split())
drbdadm_83_sources += [lfiles_83]
drbdadm_83_sources += [drbd_buildtag_h, drbd_buildtag_c]

incdir = include_directories('linux', '../shared', '../..')
executable('drbdsetup-83', drbdsetup_83_sources, include_directories: incdir)
executable('drbdadm-83', drbdadm_83_sources, include_directories: incdir)
