# FIXME make "configure" able
SE_TYPE = targeted
SE_PRIORITY = 200

all: drbd.pp.bz2

install:
	install -D -m 0644 -t $(DESTDIR)/usr/share/selinux/packages/$(SE_TYPE)/ drbd.pp.bz2
	# we did not change the upstream interface, we do not provide a custom interface
	# no install -D -m 0644 -t $(DESTDIR)/usr/share/selinux/devel/include/contrib/ drbd.if

# live system
semodule-install: all
	semodule -X${SE_PRIORITY} -i drbd.pp.bz2

semodule-uninstall:
	semodule -lfull | grep drbd
	semodule -X${SE_PRIORITY} -r drbd

# should we feel the need to change the _interface_,
# we need a custom interface file, and a dependency on it here: ipp-drbd.if 
drbd.pp: drbd.te drbd.fc
	@if ! test -r /usr/share/selinux/devel/Makefile ; then \
	  printf "\n*** %s\n*** %s\n\n" \
	    "Cannot make selinux module, /usr/share/selinux/devel/Makefile not found." \
	    "Do you need to install selinux-policy-devel?" >&2; \
	  exit 1; \
	fi
	make -f /usr/share/selinux/devel/Makefile $@

# Apparently generates from the "installed" policy/interface, not from the
# policy defined here.  So would need to first build and install.  Does not
# contain much useful information anyways, so I'm going to not include it yet.
# drbd_selinux.8: semodule-install
# 	sepolicy manpage -p . -d drbd_t

drbd.pp.bz2: drbd.pp
	bzip2 -f -9 $^

clean:
	rm -f *.tc *.pp *.pp.bz2 *_selinux.8
	rm -rf tmp

.PHONY: all clean install semodule-install
