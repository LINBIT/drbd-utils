resource r0 {
          disk {
               disk-flushes no;
               md-flushes no;
          }

          on undertest {
               node-id 0;
               volume 0 {
                    device /dev/drbd1;
                    disk /dev/scratch/day0-add-disk-20250701-105205-disk0;
                    meta-disk internal;
               }

          }

          on lbtest-vm-75.test {
               node-id 1;
               volume 0 {
                    device /dev/drbd1;
                    disk none;
               }

          }

          connection {
               path {
                    host undertest address 10.224.10.29:7789;
                    host lbtest-vm-75.test address 10.224.10.37:7789;
               }

          }
}

