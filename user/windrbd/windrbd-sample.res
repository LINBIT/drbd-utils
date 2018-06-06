# This is a sample configuration for windrbd with 2 nodes
# For commercial support please contact office@linbit.com

include "global_common.conf";

resource "windrbd-sample" {
	protocol	A;

	net {
		use-rle no;
	}

# Use this for faster sync speed:
#	disk {
#		c-max-rate 4048000;
#		c-fill-target 1048000;
#	}

	on linuxhost {
		address		192.168.0.2:7600;
		node-id 1;
		volume 1 {
# For Linux use /dev notation
			disk		/dev/sdb1;
			device		/dev/drbd1;
			flexible-meta-disk	internal;
		}
	}
	on windowshost {
		address		192.168.0.3:7600;
		node-id 2;
		volume 1 {
# The backing device of the DRBD volume
#			disk		"E:";
#
# However we strongly recommend not to assign drive letters to
# backing devices and use GUID's to address Windows volumes instead
# You can find them with the mountvol utility.
#
			disk            "3e56b893-10bf-11e8-aedd-0800274289ab";
#
# Drive letter of the windrbd device as well as a unique minor (for this host)
# The data is accessible under this drive letter (F: in that case) once
# the windrbd resource is primary (do drbdadm up <res> / drbdadm primary <res>)
#
			device		"F:" minor 1;
			meta-disk	internal;
#
# Meta disk can be internal or external
#			meta-disk	"G:";
# Again, we recommend not to use a drive letter:
#			meta-disk       "3e56b893-10bf-11e8-aedd-080027421234";
#
# Please note that as of 06/2018 Linux drbd-utils do not understand
# WinDRBD syntax of block devices (yet), so you need to replace the
# disk, device and meta-disk windows block devices with dummy values
# (/dev/drbdX, /dev/sdxx) if you want to use the same file on Linux.
# This will change in upcoming Linux drbd-utils versions.
#
		}
	}
}
