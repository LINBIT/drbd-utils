# Introduction
LINBIT provides two DRBD Volume Replication agents for VCS, `DRBDConfigure`, and `DRBDPrimary`.
These can be used to do both on-site synchronous data replication, or
cross-site/desaster-recovery asynchronous data replication,
optionally with DRBD Proxy for buffering and compression to mitigate the impact
of high latency low bandwidth replication links on the primary site application performance.

Currently this is not intended to be used for "parallel" applications.

This is intended to be used in a failover scenario only, where at most one
single system will have exclusive access to the data volume at a given time.

## DRBDConfigure
This agent is used to bring up DRBD resources.

It takes the single argument ResName, which denotes the DRBD resource to
operate on.  The DRBD resource referred to by ResName must already be defined
in the DRBD specific configuration files.  See the DRBD User's Guide for
details.

As storage backend, DRBD can deal with both shared disks (accessible by
serveral systems), or per-system "exclusively/directly attached storage" disks.

In a shared disk setup, where one group of systems (siteA) can access one shared disk,
and an other group of systems (siteB) can access another shared disk, you would
use one non-parallel instance of `DRBDConfigure` per site, to have two systems,
one at each site, replicate between the shared disks of those sites.

In a directly attached storage setup, you would use a parallel service group,
that will bring up one instance of `DRBDConfigure` for every node participating
in the replication (typically all on one site).

## DRBDPrimary
This agent controls which system can access a DRBD resource.

Bringing a resource of type DRBDPrimary online causes the DRBD resource on that
system to become DRBD Primary. The DRBD resource has to be already configured,
so this typically needs to have a dependency on a DRBDConfigure type resource
with the same ResName argument.

This dependency can be expressed by placing DRBDConfigure in some (parallel, if
applicable) "intrastructure" group, and DRBDPrimary in an "application" group,
and adding a "online local hard" link between those groups.

# Agent Setup
On all systems,
- Copy `DRBDConfigure`, `DRBDPrimary`, and `drbd_inc.sh` to `/opt/LINBITvcsag/bin/`
- Change directory to these directories and create the necessary symlinks:
```
# cd /opt/LINBITvcsag/bin/DRBDConfigure && ln -s ${VCS_HOME}/bin/Script51Agent DRBDConfigureAgent
# cd /opt/LINBITvcsag/bin/DRBDPrimary && ln -s ${VCS_HOME}/bin/Script51Agent DRBDPrimaryAgent
```
- Make the types known to VCS:
```
# mkdir -p /etc/LINBITvcsag/ha/conf/DRBD{Configure,Primary}
# cp DRBDConfigureTypes.cf /etc/LINBITvcsag/ha/conf/DRBDConfigure
# cp DRBDPrimaryTypes.cf /etc/LINBITvcsag/ha/conf/DRBDPrimary
```
- Check if the types are known (`hatype -list`). If not:
```
# LINBITvcsag=/opt/LINBITvcsag
# haconf -makerw
# for agent in DRBDConfigure DRBDPrimary; do
#   hatype -add $agent
#   hatype -modify $agent SourceFile "./${agent}Types.cf"
#   hatype -modify $agent AgentDirectory "$LINBITvcsag/bin/$agent"
#   hatype -modify $agent ArgList ResName
#   haattr -add $agent ResName -string
# done
# haconf -dump -makero
```
It should be sufficient to do this last part on only one system while VCS is running,
it will propagate automatically.  You should have put the agent scripts into
place on all systems first.

# Cluster Setup
In the following we show a simple example where the cluster consists of two nodes, and where the backing
devices already exist. We configure two groups (`DRBDGrp`, and `DRBDPriGrp`), that act on the DRBD resource `r0`.
This DRBD resource must already be defined in the DRBD configuration files.

## The DRBDGrp Group
After the following steps the resource should be up and connected on both nodes:

```
# haconf -makerw
# hagrp -add DRBDGrp
VCS NOTICE V-16-1-10136 Group added; populating SystemList and setting the Parallel attribute recommended before adding resources
# hagrp -modify DRBDGrp SystemList linbit01 0 linbit02 1
# hagrp -modify DRBDGrp Parallel 1
# hagrp -modify DRBDGrp AutoStartList linbit01 linbit02
# hares -add r0conf DRBDConfigure DRBDGrp
VCS NOTICE V-16-1-10242 Resource added. Enabled attribute must be set before agent monitors
# hares -modify r0conf ResName r0
# hares -modify r0conf Enabled 1
# haconf -dump
```

## The DRBDPriGrp Group
After this step the resource should be Primary on one system node:

```
# haconf -makerw
# hagrp -add DRBDPriGrp
VCS NOTICE V-16-1-10136 Group added; populating SystemList and setting the Parallel attribute recommended before adding resources
# hagrp -modify DRBDPriGrp SystemList linbit01 0 linbit02 1
# hagrp -modify DRBDPriGrp AutoStartList linbit01 linbit02
# hares -add r0primary DRBDPrimary DRBDPriGrp
VCS NOTICE V-16-1-10242 Resource added. Enabled attribute must be set before agent monitors
# hares -modify r0primary ResName r0
```

If this is the first time this specific DRBD resource is promoted to Primary,
you may need to manually force it into Primary role, or otherwise trigger
respectively skip the DRBD initial synchronization. See the DRBD User's Guide
for details, this only covers integration of an already defined DRBD resource
with VCS.

## Final Steps
Finally we create a dependency between these groups, and enable the DRBDPriGrp:
```
# hagrp -link DRBDPriGrp DRBDGrp online local hard
# hares -modify r0primary Enabled 1
# haconf -dump
```

The output then should look like this:

```
[root@linbit01 DRBDConfigure]# hastatus -sum

-- SYSTEM STATE
-- System               State                Frozen

A  linbit01          RUNNING              0
A  linbit02          RUNNING              0

-- GROUP STATE
-- Group           System               Probed     AutoDisabled    State

B  DRBDGrp       linbit01          Y          N               ONLINE
B  DRBDGrp       linbit02          Y          N               ONLINE
B  DRBDPriGrp    linbit01          Y          N               ONLINE
B  DRBDPriGrp    linbit02          Y          N               OFFLINE
```

## Make use of DRBD for your application on VCS
You can now add your application services and dependencies (file system mounts,
IPs, etc) into their own APPGrp group, and have that depend on the DRBDPriGrp.
Or add them directly to the DRBDPriGrp, and add an explicit dependency on the
DRBDPrimary type resource.

### Add Example Mount resource
Assuming that your `r0` DRBD resource defines one volume with device `/dev/drbd0`,
and further assuming that it contains an ext4 file system, and should be mounted to
the already on all systems created mount point `/mnt/r0`, you could now add a
`Mount` type resource:
```
# haconf -makerw
# hares -add r0mount Mount DRBDPriGrp
VCS NOTICE V-16-1-10242 Resource added. Enabled attribute must be set before agent monitors
# hares -modify r0mount FSType ext4
# hares -modify r0mount FsckOpt %-y
# hares -modify r0mount MountPoint /mnt/r0
# hares -modify r0mount BlockDevice /dev/drbd0
# hares -link r0mount r0primary
# hares -modify r0mount Enabled 1
# haconf -dump -makero
```

## Testing Switchover
You can test a switchover using `hagrp -switch DRBDPriGrp -any`.
The expected response is umount and demote on the currently active system,
and a promote and mount on the other system.
The result should then look like this:

```
[root@linbit02 ~]# hastatus -sum ; df -TPh /dev/drbd0

-- SYSTEM STATE
-- System               State                Frozen

A  linbit01          RUNNING              0
A  linbit02          RUNNING              0

-- GROUP STATE
-- Group           System               Probed     AutoDisabled    State

B  DRBDGrp         linbit01          Y          N               ONLINE
B  DRBDGrp         linbit02          Y          N               ONLINE
B  DRBDPriGrp      linbit01          Y          N               OFFLINE
B  DRBDPriGrp      linbit02          Y          N               ONLINE
Filesystem     Type  Size  Used Avail Use% Mounted on
/dev/drbd0     ext4   ...  ....   ...   2% /mnt/r0
```

