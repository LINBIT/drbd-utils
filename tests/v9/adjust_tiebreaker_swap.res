resource r0 {
	options {
		quorum majority;
	}
	volume 0 {
		device minor 10;
		disk /dev/scratch/r0_0;
		meta-disk internal;
	}
	on undertest { node-id 1; }
	on peer2 { node-id 2; }
	on peer3 { node-id 3; }

	connection {
		path {
			host undertest address 10.36.50.13:7790;
			host peer2 address 10.36.50.10:7790;
		}
	}
	connection {
		path {
			host undertest address 10.36.50.13:7791;
			host peer3 address 10.36.50.11:7791;
		}
	}
}
