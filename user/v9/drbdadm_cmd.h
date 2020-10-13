#ifndef DRBDADM_CMD_H
#define DRBDADM_CMD_H

#include <stddef.h>
#include <stdbool.h>

struct drbdadm_cmd {
	char        **resource_names;
	size_t      resource_count;
	bool        is_dump;
	bool        is_dump_xml;
	bool        is_adjust;
	bool        is_proxy_cmd;
	bool        is_all_resources_cmd;
};

#endif // DRBDADM_CMD_H
