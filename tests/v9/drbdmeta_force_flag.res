resource r0 {
    on undertest {
         node-id 1;
	 address ipv4 10.1.1.1:7006;
	 volume 0 {
	     device minor 1;
	     disk /dev/foo/fun/0;
	 }
    }
    on other {
         node-id 2;
	 address ipv4 10.1.1.2:7006;
	 volume 0 {
	     device minor 1;
	     disk /dev/foo/fun/0;
	 }
    }
}
