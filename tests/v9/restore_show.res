resource "lb-tcp-20251117-171820" {
    _this_host {
        node-id                 0;
        volume 0 {
            device                      minor 1;
            disk                        "/dev/scratch/lb-tcp-20251117-171820-disk0";
            meta-disk                   internal;
            disk {
                disk-flushes            no;
                md-flushes              no;
            }
        }
    }
    connection {
        _peer_node_id 1;
        path {
            _this_host ipv4 192.168.122.35:7789;
            _remote_host ipv4 192.168.122.36:7789;
        }
        path {
            _this_host ipv4 10.255.0.254:7789;
            _remote_host ipv4 10.255.0.162:7789;
        }
        _cstate Connecting;
        net {
            load-balance-paths  yes;
            _name               "f43-2";
        }
    }
}
