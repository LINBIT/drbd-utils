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

# You can use handlers with PowerShell scripts
#       handlers {
#		before-resync-target "echo return error ; exit 55";
#		after-resync-target "echo return ok ; exit 0";
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
# Note that only newer Linux drbd-utils (newer than 9.5.0) understand the
# WinDRBD device name syntax (they will ignore device names that are not
# on the local host), so either upgrade your drbd-utils or patch the
# file on your Linux hosts to use /dev/drbdN syntax.
#
			device		"F:" minor 1;
			meta-disk	internal;
#
# Meta disk can be internal or external
#			meta-disk	"G:";
# Again, we recommend not to use a drive letter:
#			meta-disk       "3e56b893-10bf-11e8-aedd-080027421234";
		}
	}
}
