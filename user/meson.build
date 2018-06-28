project('drbd-utils', 'c')
flex = find_program('flex')

if not flex.found()
	error('flex not found')
endif

lgen = generator(flex,
output : '@PLAINNAME@.c',
arguments : ['-s', '-o', '@OUTPUT@', '@INPUT@'])

lfiles_83=lgen.process('v83/drbdadm_scanner.fl')

drbdsetup_83_sources = files('''
  v83/drbdsetup.c
  v83/drbdtool_common.c
  v83/drbd_strings.c
  shared/shared_tool.c
  shared/drbd_buildtag.c
'''.split())

drbdadm_83_sources = files('''
  v83/drbdadm_parser.c
  v83/drbdadm_main.c
  v83/drbdadm_adjust.c
  v83/drbdtool_common.c
  v83/drbdadm_usage_cnt.c
  v83/drbdadm_minor_table.c
  shared/shared_tool.c
  shared/shared_main.c
  shared/shared_parser.c
  shared/drbd_buildtag.c
'''.split())
drbdadm_83_sources += [lfiles_83]

incdir = include_directories('v83', 'v83/linux', 'shared')
executable('drbdsetup-83', drbdsetup_83_sources, include_directories: incdir)
executable('drbdadm-83', drbdadm_83_sources, include_directories: incdir)

lfiles_84=lgen.process('v84/drbdadm_scanner.fl')

drbdsetup_84_sources = files('''
  shared/libgenl.c
  v84/registry.c
  v84/drbdsetup.c
  v84/drbdtool_common.c
  v84/drbd_strings.c
  v84/config_flags.c
  v84/drbd_nla.c
  v84/drbdsetup_colors.c
  shared/wrap_printf.c
  shared/shared_tool.c
  shared/drbd_buildtag.c
'''.split())

drbdadm_84_sources = files('''
  v84/drbdadm_parser.c
  v84/drbdadm_main.c
  v84/drbdadm_adjust.c
  v84/drbdtool_common.c
  v84/drbdadm_usage_cnt.c
  v84/registry.c
  v84/config_flags.c
  v84/drbd_nla.c
  shared/drbd_buildtag.c
  shared/libgenl.c
  shared/shared_tool.c
  shared/shared_main.c
  shared/shared_parser.c
'''.split())
drbdadm_84_sources += [lfiles_84]

incdir = include_directories('v84', 'v84/linux', 'shared')
# executable('drbdsetup-84', drbdsetup_84_sources, include_directories: incdir)
# executable('drbdadm-84', drbdadm_84_sources, include_directories: incdir)

lfiles_9=lgen.process('v9/drbdadm_scanner.fl')

drbdsetup_9_sources = files('''
  shared/libgenl.c
  v9/registry.c
  v9/drbdsetup.c
  v9/drbdtool_common.c
  v9/config_flags.c
  v9/drbd_nla.c
  v9/drbdsetup_colors.c
  shared/wrap_printf.c
  shared/shared_tool.c
  shared/drbd_buildtag.c
'''.split())

drbdadm_9_sources = files('''
  v9/drbdadm_parser.c
  v9/drbdadm_main.c
  v9/drbdadm_postparse.c
  v9/drbdadm_adjust.c
  v9/drbdtool_common.c
  v9/drbdadm_usage_cnt.c
  v9/registry.c
  v9/config_flags.c
  v9/drbd_nla.c
  ../drbd-headers/drbd_strings.c
  shared/drbd_buildtag.c
  shared/libgenl.c
  shared/shared_tool.c
  shared/shared_main.c
  shared/shared_parser.c
'''.split())
drbdadm_9_sources += [lfiles_9]

incdir = include_directories('v9', '../drbd-headers', 'shared')
executable('drbdsetup-9', drbdsetup_9_sources, include_directories: incdir)
executable('drbdadm-9', drbdadm_9_sources, include_directories: incdir)
