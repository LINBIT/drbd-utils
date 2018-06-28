if compile_drbdmon
drbdmon_sources = files('''
	Args.cpp
	CompactDisplay.cpp
	comparators.cpp
	ConfigOption.cpp
	DebugDisplay.cpp
	Display.cpp
	DrbdConnection.cpp
	DrbdMon.cpp
	drbdmon_main.cpp
	DrbdResource.cpp
	DrbdRole.cpp
	DrbdVolume.cpp
	EventsIo.cpp
	EventsSourceSpawner.cpp
	MessageLog.cpp
	StateFlags.cpp
	StringTokenizer.cpp
	utils.cpp
	VolumesContainer.cpp
	cppdsaext/src/dsaext.cpp
'''.split())

incdir = include_directories('cppdsaext/src', '../shared', '../..')
executable('drbdmon', drbdmon_sources,
	include_directories: incdir,
	install_dir: get_option('sbindir'))
endif
