global {
       disable-ip-verification;
}

resource site1 {
	net {
		cram-hmac-alg "sha1";
		shared-secret "Gei6mahcui4Ai0Oh1";
	}

	volume 0 {
		device minor 0;
		disk /dev/foo;
		meta-disk /dev/bar;
	}
	on alfa		{ node-id 1; address 192.168.1.17:7000; }
	on bravo	{ node-id 2; address 192.168.2.17:7000; }
	on charlie	{ node-id 3; address 192.168.3.17:7000; }
	connection-mesh { hosts alfa bravo charlie; }
}

resource site2 {
	net {
		cram-hmac-alg "sha1";
		shared-secret "Gei6mahcui4Ai0Oh2";
	}

	volume 0 {
		device minor 0;
		disk /dev/foo;
		meta-disk /dev/bar;
	}
	on delta	{ node-id 4; address 192.168.4.17:7000; }
	on echo		{ node-id 5; address 192.168.5.17:7000; }
	on fox		{ node-id 6; address 192.168.6.17:7000; }
	connection-mesh { hosts delta echo fox; }
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

	volume 0 { device minor 10; }

	stacked-on-top-of site1 { node-id 0; }
	stacked-on-top-of site2 { node-id 1; }

	connection { # site1 - site2
		path {
			host alfa	address 192.168.1.17:7100;
			host delta	address 192.168.4.17:7100;
		}
		path {
			host bravo	address 192.168.2.17:7100;
			host delta	address 192.168.4.17:7100;
		}
		path {
			host charlie	address 192.168.3.17:7100;
			host delta	address 192.168.4.17:7100;
		}
		path {
			host alfa	address 192.168.1.17:7100;
			host echo	address 192.168.5.17:7100;
		}
		path {
			host bravo	address 192.168.2.17:7100;
			host echo	address 192.168.5.17:7100;
		}
		path {
			host charlie	address 192.168.3.17:7100;
			host echo	address 192.168.5.17:7100;
		}
		path {
			host alfa	address 192.168.1.17:7100;
			host fox	address 192.168.6.17:7100;
		}
		path {
			host bravo	address 192.168.2.17:7100;
			host fox	address 192.168.6.17:7100;
		}
		path {
			host charlie	address 192.168.3.17:7100;
			host fox	address 192.168.6.17:7100;
		}
	}
}
