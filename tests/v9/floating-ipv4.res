resource r0 {
    volume 0 { device minor 1; disk /dev/foo/fun/0; }
    floating 127.0.0.1:7706 { node-id 1; } # undertest
    floating 127.1.2.3:7706 { node-id 2; } # other
}
