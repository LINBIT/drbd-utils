resource r0 {
    volume 0 { device minor 1; disk /dev/foo/fun/0; }
    # undertest, 127.0.0.1 used to identify "self",
    # we can not rely on ::1%lo to be present in all CI pipelines
    floating 127.0.0.1:7706 { node-id 1; }
    floating ipv6 [fe80::1022:53ff:feb7:614f%vethX]:7706 { node-id 2; } # other
}
