resource "nat-address" {
	volume 0 {
		device minor 99;
		disk "/dev/foo/bar4";
		meta-disk "internal";
	}
	on "undertest" {
		node-id 0;
	}
	on "other" {
		node-id 1;
	}
	connection {
		host "undertest" address 10.43.10.50:7000 via outside-address 192.168.17.19:7000;
		host "other" address 10.43.10.51:7000 via outside-address 192.168.17.20:7000;
	}
}
