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
include $(drbdsetup-obj:%.o=.%.d) $(drbdadm-obj:%.o=.%.d) $(drbdmeta-obj:%.o=.%.d)
