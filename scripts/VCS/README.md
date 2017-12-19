# Introduction
Currently we provide two agents, `DRBDConfigure`, and `DRBDPrimary`.

## DRBDConfigure
This agent is used to bring up resources. This is very similar to what the init script/the systemd service
file usually does for DRBD resources. If you can use the init script/service file, use it and ignore this
agent. We provide it for cases where it is not possible to use the init script/service file. This might for
example include cases where the backing devices of your resources are Veritas cluster resources themselves.

## DRBDPrimary
This agent switches a resource to Primary/Secondary.

# Agent Setup
- Copy `DRBDConfigure`, `DRBDPrimary`, and `drbd_inc.sh` to `$VCS_HOME/bin` (e.g., /opt/VRTSvcs/bin/)
- Change directory to these directories and create the necessary symlinks:
```
cd $VCS_HOME/bin/DRBDConfigure && ln -s ${VCS_HOME}/bin/Script51Agent DRBDConfigureAgent
cd $VCS_HOME/bin/DRBDPrimary && ln -s ${VCS_HOME}/bin/Script51Agent DRBDPrimaryAgent
```
- Make the types known to VCS:
```
mkdir -p /etc/VRTSagents/ha/conf/DRBD{Configure,Primary}
cp DRBDConfigureTypes.cf /etc/VRTSagents/ha/conf/DRBDConfigure
cp DRBDPrimaryTypes.cf /etc/VRTSagents/ha/conf/DRBDPrimary
```
- Check if the types are known (`hatype -list`). If not:
```
hatype -add DRBDConfigure
hatype -modify DRBDConfigure ArgList ResName
haattr -add DRBDConfigure ResName -string
hatype -add DRBDPrimary
hatype -modify DRBDPrimary ArgList ResName
haattr -add DRBDPrimary ResName -string
```

# Cluster Setup
In the following we show a simple example where the cluster consists of two nodes, and where the backing
devices already exist. We configure two groups (`configure`, and `primary`), that act on the DRBD resource `r0`:

## The configure Group
After the following steps the resource should be up and connected on both nodes:

```
# haconf -makerw
# hagrp -add configure
VCS NOTICE V-16-1-10136 Group added; populating SystemList and setting the Parallel attribute recommended before adding resources
# hagrp -modify configure SystemList infoscale01 0 infoscale02 1
# hagrp -modify configure Parallel 1
# hagrp -modify configure AutoStartList infoscale01 infoscale02
# hares -add r0conf DRBDConfigure configure
VCS NOTICE V-16-1-10242 Resource added. Enabled attribute must be set before agent monitors
# hares -modify r0conf ResName r0
# hares -modify r0conf Enabled 1
```

## The primary Group
After this step the resource should be Primary on one node:

```
# haconf -makerw
# hagrp -add primary
VCS NOTICE V-16-1-10136 Group added; populating SystemList and setting the Parallel attribute recommended before adding resources
# hagrp -modify primary SystemList infoscale01 0 infoscale02 1
# hagrp -modify primary AutoStartList infoscale01 infoscale02
# hares -add r0primary DRBDPrimary primary
VCS NOTICE V-16-1-10242 Resource added. Enabled attribute must be set before agent monitors
# hares -modify r0primary ResName r0
# hares -modify r0primary Enabled 1
```

## Final Steps
Finally we create a dependency between these groups:
`hagrp -link primary configure online global`

The output then should look like this:

```
[root@infoscale01 DRBDConfigure]# hastatus -sum

-- SYSTEM STATE
-- System               State                Frozen

A  infoscale01          RUNNING              0
A  infoscale02          RUNNING              0

-- GROUP STATE
-- Group           System               Probed     AutoDisabled    State

B  configure       infoscale01          Y          N               ONLINE
B  configure       infoscale02          Y          N               ONLINE
B  primary         infoscale01          Y          N               ONLINE
B  primary         infoscale02          Y          N               OFFLINE
```
