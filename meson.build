project('drbd-utils', 'c',
  version: '9.5.1')
# default_options: ['cpp_std=c++11'])

compile_drbdmon = false
if get_option('with-drbdmon')
  if add_languages('cpp', required: false) and meson.get_compiler('cpp').has_argument('-std=c++11')
   	compile_drbdmon = true
  else
	 warning('disabling drbdmon compile: You do noth have a sufficient C++11 compiler')
  endif
endif

# check for programs
flex = find_program('flex')
grep = find_program('grep')
drbd_buildtag = find_program('user/shared/drbd_buildtag.sh')
git = find_program('git', required: false)

# generator function for flex files
lgen = generator(flex,
  output : '@PLAINNAME@.c',
  arguments : ['-s', '-o', '@OUTPUT@', '@INPUT@'])

# configuration data that will get exposed as config.h
conf = configuration_data()
conf.set_quoted('PACKAGE_NAME',  meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING',  meson.project_name() + ' ' + meson.project_version())
conf.set_quoted('PACKAGE_BUGREPORT', 'drbd-user@lists.linbit.com')
conf.set_quoted('PACKAGE_URL', 'https://www.linbit.com')
if get_option('with-83-support')
  conf.set('DRBD_LEGACY_83', true)
endif
if get_option('with-84-support')
  conf.set('DRBD_LEGACY_84', true)
endif

nlhdr = '/usr/include/linux/genetlink.h'
foreach define: ['CTRL_ATTR_VERSION', 'CTRL_ATTR_HDRSIZE', 'CTRL_ATTR_MCAST_GROUPS']
  if run_command(grep, '-wq', define, nlhdr).returncode() == 0
	 conf.set('HAVE_' + define, true)
  endif
endforeach

## compiler checks
ccompiler = meson.get_compiler('c')
if ccompiler.has_function('getentropy', prefix: '#include <unistd.h>')
  conf.set('HAVE_GETENTROPY', true)
endif

delmcast = '''#include <linux/genetlink.h>
static unsigned int dummy = CTRL_CMD_DELMCAST_GRP;
'''
if ccompiler.compiles(delmcast, name: 'CTRL_CMD_DELMCAST_GRP')
  conf.set('HAVE_CTRL_CMD_DELMCAST_GRP', true)
endif

configure_file(input: 'user/shared/mconfig.h.in',
  output: 'config.h',
  configuration: conf)

# drbd_buildtag.[ch] nonsense
drbd_buildtag_h = custom_target('foo_drbd_buildtag_h',
	 output: ['drbd_buildtag.h'],
	 command: [drbd_buildtag, '@OUTPUT@'])

drbd_buildtag_c = custom_target('foo_drbd_buildtag_c',
	 output: ['drbd_buildtag.c'],
	 command: [drbd_buildtag, '@OUTPUT@'])

# projects to build
if get_option('with-83-support')
  subdir('user/v83')
endif
if get_option('with-84-support')
  subdir('user/v84')
endif
if compile_drbdmon
  subdir('user/drbdmon')
endif
subdir('user/v9')
