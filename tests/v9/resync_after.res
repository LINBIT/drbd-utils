resource res0 {
	on swiftfox {
		volume 0 {
			disk /dev/ssdpool/res0_0;
			disk {
				discard-zeroes-if-aligned yes;
				rs-discard-granularity 65536;
			}
			device minor 1039;
		}
		node-id 1;
	}
	on undertest {
		volume 0 {
			disk /dev/ssdpool/res0_0;
			disk {
				discard-zeroes-if-aligned yes;
				rs-discard-granularity 65536;
			}
			device minor 1039;
		}
		node-id 2;
	}
	connection {
		disk {
			c-fill-target 1048576;
		}
		host swiftfox address ipv4 10.43.241.3:7039;
		host undertest address ipv4 10.43.241.4:7039;
	}
}

resource res1 {
	on swiftfox {
		volume 0 {
			disk /dev/ssdpool/res0_0;
			disk {
				discard-zeroes-if-aligned yes;
				resync-after "testing_that_this_is_accepted_although_not_defined_in_here/0";
				rs-discard-granularity 65536;
			}
			device minor 1041;
		}
		node-id 1;

	}
	on undertest {
		volume 0 {
			disk /dev/ssdpool/res0_0;
			disk {
				discard-zeroes-if-aligned yes;
				resync-after "res0/0";
				rs-discard-granularity 65536;
			}
			device minor 1041;
		}
		node-id 2;
	}
	connection {
		disk {
			c-fill-target 1048576;
		}
		host swiftfox address ipv4 10.43.241.3:7041;
		host undertest address ipv4 10.43.241.4:7041;
	}
}
