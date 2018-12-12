global { disable-ip-verification; }
resource r0 {
	 net {
			protocol C;
			cram-hmac-alg sha1;
			shared-secret "FooFunFactory";
	 }
	 disk {
			resync-rate 10M;
	 }
	 on undertest {
			volume 0 {
				  device    minor 1;
				  disk      /dev/sda7;
				  meta-disk internal;
			}
			address   10.1.1.31:7789;
	 }
	 on bob {
			volume 0 {
				  device    minor 1;
				  disk      /dev/sda7;
				  meta-disk internal;
			}
			address   10.1.1.32:7789;
	 }
}
