resource r0 {
	volume 0 {
		device minor 10;
		disk /dev/scratch/r0_0;
		meta-disk internal;
	}
	net {
		verify-alg crc32c;
	}

	on n1.ryzen9.home { node-id 0; address 192.168.1.10:7000; }
	on n2.ryzen9.home { node-id 1; address 192.168.1.11:7000; }
	on undertest {
		node-id 2;
		volume 0 {
		       disk none;
		}
		address 192.168.1.12:7000;
	}

	connection-mesh { hosts n1 n2 undertest; }
}
