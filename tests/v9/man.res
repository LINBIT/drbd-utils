global { disable-ip-verification; }
resource r0 {
	  net {
			 cram-hmac-alg sha1;
			 shared-secret "FooFunFactory";
	  }
	  volume 0 {
			 device    /dev/drbd1;
			 disk      /dev/sda7;
			 meta-disk internal;
	  }
	  on undertest {
			 node-id   0;
			 address   10.1.1.31:7000;
	  }
	  on bob {
			 node-id   1;
			 address   10.1.1.32:7000;
	  }
	  connection {
			 host      undertest  port 7000;
			 host      bob    port 7000;
			 net {
				  protocol C;
			 }
	  }
}
