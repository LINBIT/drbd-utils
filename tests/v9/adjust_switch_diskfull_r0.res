resource r0 {
	options {
		quorum majority;
	}
	volume 0 {
		device minor 10;
		disk /dev/scratch/r0_0;
		meta-disk internal;
	}
	on undertest { node-id 1; volume 0 { disk none; } }
	on i2 { node-id 2; }
	on i3 { node-id 3; }

	net { load-balance-paths yes; }

	skip {
		path {
			host undertest address 192.168.122.11:7000;
			host i2 address 192.168.122.12:7000;
		}
		path {
			host undertest address 192.168.122.11:7001;
			host i2 address 192.168.122.12:7001;
		}
	}
	connection {
		path {
			host i2 address 192.168.122.12:7000;
			host i3 address 192.168.122.13:7000;
		}
		path {
			host i2 address 192.168.122.12:7001;
			host i3 address 192.168.122.13:7001;
		}
	}
	connection {
		path {
			host undertest address 192.168.122.11:7000;
			host i3 address 192.168.122.13:7000;
		}
		path {
			host undertest address 192.168.122.11:7001;
			host i3 address 192.168.122.13:7001;
		}
	}
}

