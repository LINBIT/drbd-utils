global {
  disable-ip-verification;
  usage-count no;
}
common {
  protocol C;
  syncer {
     rate 25M;
     al-extents 379;
     csums-alg md5;
     verify-alg crc32c;
     c-min-rate 20M;
     c-max-rate 500M;
  }
  handlers {
    outdate-peer "/opt/root-scripts/bin/fence-peer";
    local-io-error "/opt/root-scripts/bin/handle-io-error";
    pri-on-incon-degr "/opt/root-scripts/bin/handle-io-error";
  }
  net {
    timeout 50;
    connect-int 10;
    ping-int 5;
    ping-timeout 50;
    cram-hmac-alg md5;
    csums-after-crash-only;
    shared-secret "gaeWoor7dawei3Oo";
    ko-count 0;
  }
}

resource dbdata_resource {
  startup {
   wfc-timeout     1;
     }
  disk {
    no-disk-flushes;
    no-md-flushes;
    fencing resource-and-stonith;
    on-io-error call-local-io-error;
    disk-timeout 0;
  }
  device    /dev/drbd1;
  disk      /dev/dbdata01/lvdbdata01;
  meta-disk internal;

  on undertest {
    address   172.16.6.211:1120;
  }
  on peer-host {
    address   172.16.0.249:1120;
  }
}
