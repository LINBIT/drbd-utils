# to be included from user/v*/Makefiles

../shared/%:
	$(MAKE) -C $(@D) $(@F)
drbd_buildtag.o: ../shared/drbd_buildtag.c

# from make documentation, automatic prerequisites
.%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.drbdmeta_scanner.d: ../shared/drbdmeta_scanner.c
all-dep := $(filter-out drbd_buildtag.d,$(all-obj:%.o=.%.d))

ifeq ($(MAKECMDGOALS),$(filter-out clean distclean,$(MAKECMDGOALS)))
-include $(all-dep)
endif
