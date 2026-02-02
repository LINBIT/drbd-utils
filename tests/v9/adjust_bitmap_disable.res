resource r0 {
	disk {
		bitmap no;
	}

	on undertest {
		node-id 0;
		volume 0 {
			device /dev/drbd1;
			disk /dev/sda1;
			meta-disk internal;
		}
	}
}
