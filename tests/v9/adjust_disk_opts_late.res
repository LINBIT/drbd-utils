resource r0 {
	on undertest {
		node-id 0;
		volume 0 {
			device /dev/drbd1;
			disk /dev/sda1;
			meta-disk internal;
		}
	}

	on peer1 {
		node-id 1;
		volume 0 {
			device /dev/drbd1;
			disk /dev/sda1;
			meta-disk internal;
		}
	}

	connection {
		path {
			host undertest address 10.0.0.1:7789;
			host peer1 address 10.0.0.2:7789;
		}
	}
}
