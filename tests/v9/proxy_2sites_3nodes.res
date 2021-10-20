global {
       disable-ip-verification;
}

resource proxy_2sites_3nodes {
	volume 0 {
	       device minor 19;
	       disk /dev/foo/bar;
	       meta-disk internal;
	}

	on alpha {
		node-id 0;
		address 192.168.31.1:7800;
	}
	on bravo {
		node-id 1;
		address 192.168.31.2:7800;
	}
	on charlie {
		node-id 2;
		address 192.168.31.3:7800;
	}

	connection {
		host alpha;
		host bravo;
		net { protocol C; }
	}

	connection {
		host alpha address 192.168.31.1:7800 via proxy on site1-pxy {
			inside 192.168.31.4:7800;
			outside 192.168.31.4:7801;
		}
		host charlie address 192.168.31.3:7800 via proxy on charlie-pxy {
			inside 192.168.31.5:7800;
			outside 192.168.31.5:7801;
		}
		net { protocol A; }

		volume 0 {
			disk {
				resync-rate 10M;
				c-plan-ahead 20;
				c-delay-target 10;
				c-fill-target 100;
				c-min-rate 10;
				c-max-rate 100M;
			}
		}
	}

	connection {
		host bravo address 192.168.31.2:7800 via proxy on site1-pxy {
			inside 192.168.31.4:7802;
			outside 192.168.31.4:7803;
		}
		host charlie address 192.168.31.3:7800 via proxy on charlie-pxy {
			inside 192.168.31.5:7802;
			outside 192.168.31.5:7803;
		}
		net { protocol A; }
	}
}
