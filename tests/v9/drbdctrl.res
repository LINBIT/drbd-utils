global { disable-ip-verification; }
resource r0 {
    net {
        cram-hmac-alg       sha256;
        shared-secret       "Uwni5ZRVCvbqk3AwHD4K";
        allow-two-primaries no;
    }
    volume 0 {
        device      minor 0;
        disk        /dev/drbdpool/.drbdctrl_0;
        meta-disk   internal;
    }
    volume 1 {
        device      minor 1;
        disk        /dev/drbdpool/.drbdctrl_1;
        meta-disk   internal;
    }
    on undertest {
        node-id     0;
        address     ipv4 10.43.70.115:6999;
    }
    on rckdebb {
        node-id     1;
        address     ipv4 10.43.70.116:6999;
    }
    on rckdebd {
        node-id     2;
        address     ipv4 10.43.70.118:6999;
    }
    connection-mesh {
        hosts undertest rckdebb rckdebd;
        net {
            protocol C;
        }
    }
}
