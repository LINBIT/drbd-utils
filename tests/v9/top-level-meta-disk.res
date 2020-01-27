resource drbd_testqm {
    device /dev/drbd1;
    meta-disk /dev/mqmvg/MD-testqm;
    syncer {
        verify-alg sha1;
    }
    disk {
        disk-flushes no;
        md-flushes no;
        disable-write-same yes;
        resync-rate 184320;
        c-fill-target 1048576;
        c-max-rate  4194304;
        c-min-rate   0;
    }
    net {
        max-buffers 131072;
        sndbuf-size 10485760;
        rcvbuf-size 10485760;
    }
    on ADDRLeft {
        disk /dev/mqmvg/QM-testqm;
        address 192.168.45.122:7789;
    }
    on ADDRRight {
        disk /dev/mqmvg/QM-testqm;
        address 192.168.45.121:7789;
    }
# discarded some stuff
}
