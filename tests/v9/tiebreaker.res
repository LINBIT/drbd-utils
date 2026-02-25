resource "tiebreaker-test" {
	volume 0 {
		device minor 43;
		disk "/dev/foo/bar42";
		meta-disk "internal";
	}
	on "undertest" {
		node-id 0;
		address 192.168.1.10:7000;
	}
	on "other" {
		node-id 1;
		address 192.168.1.11:7000;
	}
	on "witness" {
		node-id 2;
		address 192.168.1.12:7000;
		volume 0 {
			disk none;
		}
	}
	on "dless" {
		node-id 3;
		address 192.168.1.13:7000;
		volume 0 {
			disk none;
			tiebreaker no;
		}
	}
	connection-mesh { hosts undertest other witness dless; }
}
