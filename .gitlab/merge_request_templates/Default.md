- [ ] This change does not need a ChangeLog entry (minor fix, documentation, CI, ...)

----------

Every merge request either adds an entry to the "Unreleased" section of
`ChangeLog`, or ticks the box above. The `check-changelog` job fails
otherwise. Ticking the box after the pipeline ran does not start a new
pipeline; retry the job.

A ChangeLog entry describes what changed for users of DRBD in one or two
lines, for example:

```
 * drbdadm: add dump-superblock command
```
