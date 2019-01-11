global {
       disable-ip-verification;
}

resource site1 {
	net {
		cram-hmac-alg "sha1";
		shared-secret "Gei6mahcui4Ai0Oh1";
	}

	on undertest {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.23.21:7000;
	}
	on bravo {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.23.22:7000;
	}
}

resource site2 {
	net {
		cram-hmac-alg "sha1";
		shared-secret "Gei6mahcui4Ai0Oh2";
	}

	on charlie {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.24.21:7000;
	}
	on delta {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.24.22:7000;
	}
}

resource site3 {
	net {
		cram-hmac-alg "sha1";
		shared-secret "Gei6mahcui4Ai0Oh3";
	}

	on echo {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.25.21:7000;
	}
	on foxtrott {
		volume 0 {
			device minor 0;
			disk /dev/foo;
			meta-disk /dev/bar;
		}
		address	192.168.25.22:7000;
	}
}

resource stacked_multi_path {
	net {
		protocol A;

		on-congestion pull-ahead;
		congestion-fill 400M;
		congestion-extents 1000;
	}

	disk {
		c-fill-target 10M;
	}

	volume 0 {
		device minor 10;
	}

	stacked-on-top-of site1 {
		node-id 0;
	}
	stacked-on-top-of site2 {
		node-id 1;
	}
	stacked-on-top-of site3 {
		node-id 2;
	}

	connection { # site1 - site2
		path {
			host undertest address 192.168.23.21:7100;
			host charlie address 192.168.24.21:7100;
		}
		path {
			host bravo address 192.168.23.22:7100;
			host delta address 192.168.24.22:7100;
		}
		path {
			host undertest address 192.168.23.21:7100;
			host delta address 192.168.24.22:7100;
		}
		path {
			host bravo address 192.168.23.22:7100;
			host charlie address 192.168.24.21:7100;
		}
	}

	connection {
		path {
			host undertest address 192.168.23.21:7100;
			host echo address 192.168.25.21:7100;
		}
		path {
			host bravo address 192.168.23.22:7100;
			host foxtrott address 192.168.25.22:7100;
		}
		path {
			host undertest address 192.168.23.21:7100;
			host foxtrott address 192.168.25.22:7100;
		}
		path {
			host bravo address 192.168.23.22:7100;
			host echo address 192.168.25.21:7100;
		}

	}

	connection {
		path {
			host charlie address 192.168.24.21:7100;
			host echo address 192.168.25.21:7100;
		}
		path {
			host delta address 192.168.24.22:7100;
			host foxtrott address 192.168.25.22:7100;
		}
		path {
			host charlie address 192.168.24.21:7100;
			host foxtrott address 192.168.25.22:7100;
		}
		path {
			host delta address 192.168.24.22:7100;
			host echo address 192.168.25.21:7100;
		}
	}
}
