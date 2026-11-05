global { cmd-timeout-short 10; cmd-timeout-medium 600; cmd-timeout-long 0; }
resource r0 {
	volume 0 {
		device    /dev/drbd1;
		disk      /dev/sda7;
		meta-disk internal;
	}
	on undertest {
		node-id   0;
		address   127.0.0.1:7000;
	}
	on bob {
		node-id   1;
		address   10.1.1.32:7000;
	}
}
