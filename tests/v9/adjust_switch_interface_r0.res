resource r0 {
	options {
		quorum majority;
		on-no-quorum suspend-io;
		on-no-data-accessible suspend-io;
		on-suspended-primary-outdated force-secondary;
	}

	disk {
		disk-flushes no;
		md-flushes no;
		block-size 4096;
	}
	volume 0 {
		device minor 10;
		disk /dev/scratch/r0_0;
		meta-disk internal;
	}
	net {
		after-sb-0pri discard-zero-changes;
		rr-conflict retry-connect;
		verify-alg md5;
	}

	on n1.ryzen9.home { node-id 0; }
	on n2.ryzen9.home { node-id 1; }
	on undertest {
		node-id 2;
		volume 0 {
		       disk none;
		}
	}

	connection {
		skip {
			host n1 address 192.168.122.47:7000;
			host n2 address 192.168.122.48:7000;
		}
		path {
			host n1 address 192.168.123.1:7001;
			host n2 address 192.168.123.2:7001;
		}
	}


	connection {
		skip {
			host n1 address 192.168.122.47:7000;
			host undertest address 192.168.122.49:7000;
		}
		path {
			host n1 address 192.168.123.1:7001;
			host undertest address 192.168.123.3:7001;
		}
	}

	connection {
		skip {
			host n2 address 192.168.122.48:7000;
			host undertest address 192.168.122.49:7000;
		}
		path {
			host n2 address 192.168.123.2:7001;
			host undertest address 192.168.123.3:7001;
		}
	}
}
