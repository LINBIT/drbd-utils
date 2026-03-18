global {
	disable-ip-verification;
}

resource r0 {
	proxy {
		memlimit 128M;
		plugin {
			zstd level 4;
		}
	}

	on undertest {
		node-id 0;
		volume 0 {
			device /dev/drbd1;
			disk /dev/sda1;
			meta-disk internal;
		}
	}

	on peer {
		node-id 1;
		volume 0 {
			device /dev/drbd1;
			disk /dev/sda1;
			meta-disk internal;
		}
	}

	connection {
		host undertest address 10.0.0.1:7789 via proxy on undertest {
			inside 10.0.1.1:7789;
			outside 10.0.2.1:7789;
		}
		host peer address 10.0.0.2:7789 via proxy on peer {
			inside 10.0.1.2:7789;
			outside 10.0.2.2:7789;
		}
		net { protocol A; }
	}
}
