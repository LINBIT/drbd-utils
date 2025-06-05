global {
       disable-ip-verification;
}

resource r0 {
	disk {
		disk-flushes no;
		disk-barrier no;
		md-flushes no;
	}
	volume 1 {
		device minor 0;
		disk /dev/vdb;
		meta-disk internal;
	}

	on undertest {
		address	192.168.122.173:7000;
	}
	on rocky-8.9_b {
		address	192.168.122.174:7000;
	}
}

resource r1 {
	volume 1 {
		device minor 1;
		disk /dev/vdc;
		meta-disk internal;
	}

	on undertest {
		address	192.168.122.173:7001;
	}
	on rocky-8.9_b {
		address	192.168.122.174:7001;
	}
}

resource r2 {
	volume 1 {
		device minor 2;
		disk /dev/vdd;
		meta-disk internal;
	}

	on undertest {
		address	192.168.122.173:7002;
	}
	on rocky-8.9_b {
		address	192.168.122.174:7002;
	}
}
