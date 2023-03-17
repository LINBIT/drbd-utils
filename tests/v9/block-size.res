resource res {
          disk {
               disk-flushes no;
               md-flushes no;
               block-size 4096;
          }

          on undertest.ryzen9.home {
               node-id 0;
               volume 0 {
                    device /dev/drbd1;
                    disk none;
               }

          }

          on u2.ryzen9.home {
               node-id 1;
               volume 0 {
                    device /dev/drbd1;
                    disk /dev/mapper/diskless-logical-block-size-20230320-100658-disk0-ebs;
                    meta-disk internal;
               }

          }

          on u3.ryzen9.home {
               node-id 2;
               volume 0 {
                    device /dev/drbd1;
                    disk /dev/mapper/diskless-logical-block-size-20230320-100658-disk0-ebs;
                    meta-disk internal;
               }

          }

          connection {
               net {
               }

               path {
                    host undertest address 192.168.123.51:7789;
                    host u2 address 192.168.123.52:7789;
               }

          }

          connection {
               net {
               }

               path {
                    host undertest address 192.168.123.51:7789;
                    host u3 address 192.168.123.53:7789;
               }

          }

          connection {
               net {
               }

               path {
                    host u2 address 192.168.123.52:7789;
                    host u3 address 192.168.123.53:7789;
               }

          }

}

