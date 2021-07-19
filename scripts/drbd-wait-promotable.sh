#!/bin/bash
# Waits for a resource to become promotable, then exits.

while read line ; do
  [[ $line = *may_promote:yes* ]] && exit 0;
done < <( exec drbdsetup events2 "$1" )

# In case drbdsetup encountered issues, report an error.
exit 1
