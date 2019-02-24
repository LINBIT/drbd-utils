global {
  disable-ip-verification;
}

resource r0 {
  net {
    protocol C;
  }

  startup {
        wfc-timeout             60;
        degr-wfc-timeout        60;
  }

  on undertest {
    device     /dev/drbd0;
    disk       /dev/sdb1;
    address    10.56.84.138:7788;
    meta-disk  internal;
  }

  on node_b {
    device    /dev/drbd0;
    disk      /dev/sdb1;
    address   10.56.84.139:7788;
    meta-disk internal;
  }
}

resource r0-U {
  net {
    protocol B;
  }

  stacked-on-top-of r0 {
    device     /dev/drbd10;
    address    10.56.84.142:7788;
  }

  on node_c {
    device     /dev/drbd10;
    disk       /dev/sdb1;
    address    10.56.85.140:7788;
    meta-disk  internal;
  }
}
