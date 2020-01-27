/*
   drbdadm_dump.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2003-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2003-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2003-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "drbdadm.h"
#include "drbdtool_common.h"

static int indent = 0;
#define INDENT_WIDTH 4
#define BFMT  "%s;\n"
#define IPV4FMT "%-16s %s %s:%s%s"
#define IPV6FMT "%-16s %s [%s]:%s%s"
#define MDISK "%-16s %s;\n"
#define MDISKI "%-16s %s [%s];\n"
#define printI(fmt, args... ) printf("%*s" fmt,INDENT_WIDTH * indent,"" , ## args )
#define printA(name, val ) \
	printf("%*s%*s %3s;\n", \
	  INDENT_WIDTH * indent,"" , \
	  -24+INDENT_WIDTH * indent, \
	  name, val )

static void dump_options(char *name, struct options *options);

static void __dump_options(struct options *options)
{
	struct d_option *option;

	STAILQ_FOREACH(option, options, link) {
		if (option->value)
			printA(option->name,
			       option->is_escaped ? option->value : esc(option->
									value));
		else
			printI(BFMT, option->name);
	}
}

static void dump_options2(char *name, struct options *options,
			  void(*within)(struct options *), struct options *ctx)
{

	if (STAILQ_EMPTY(options) && (!ctx || (ctx && STAILQ_EMPTY(ctx))))
		return;

	printI("%s {\n", name);
	++indent;
	__dump_options(options);
	if (within)
		within(ctx);
	--indent;
	printI("}\n");
}

static void dump_peer_device_options(struct options *options)
{
	if (!STAILQ_EMPTY(options)) {
		/* printI("# peer device options:\n"); */
		__dump_options(options);
	}
}

static void dump_options(char *name, struct options *options)
{
	dump_options2(name, options, NULL, NULL);
}

static void dump_proxy_plugins(struct options *options)
{
	dump_options("plugin", options);
}

void dump_global_info()
{
	static const char * const yes_no_ask[] = {
		[UC_YES] = "yes",
		[UC_NO] = "no",
		[UC_ASK] = "ask",
	};
	if (!global_options.minor_count
	    && !global_options.disable_ip_verification
	    && global_options.dialog_refresh == 1
	    && global_options.usage_count == UC_ASK
	    && !verbose)
		return;
	printI("global {\n");
	++indent;
	if (global_options.disable_ip_verification)
		printI("disable-ip-verification;\n");
	if (global_options.minor_count)
		printI("minor-count %i;\n", global_options.minor_count);
	if (global_options.dialog_refresh != 1)
		printI("dialog-refresh %i;\n", global_options.dialog_refresh);
	if (global_options.usage_count != UC_ASK)
		printI("usage-count %s;\n", yes_no_ask[global_options.usage_count]);
	--indent;
	printI("}\n\n");
}

static void fake_startup_options(struct d_resource *res);

static void dump_common_info()
{
	if (!common)
		return;
	printI("common {\n");
	++indent;

	fake_startup_options(common);
	dump_options("options", &common->res_options);
	dump_options("net", &common->net_options);
	dump_options2("disk", &common->disk_options,
		      dump_peer_device_options, &common->pd_options);
	dump_options("startup", &common->startup_options);
	dump_options2("proxy", &common->proxy_options,
			dump_proxy_plugins, &common->proxy_plugins);
	dump_options("handlers", &common->handlers);
	--indent;
	printf("}\n\n");
}

static void dump_address(char *name, struct d_address *address, char *postfix)
{
	if (!strcmp(address->af, "ipv6"))
		printI(IPV6FMT, name, address->af, address->addr, address->port, postfix);
	else
		printI(IPV4FMT, name, address->af, address->addr, address->port, postfix);
}

static void dump_proxy_info(const char *prefix, struct d_proxy_info *pi)
{
	printI("%sproxy on %s {\n", prefix, names_to_str(&pi->on_hosts));
	++indent;
	dump_address("inside", &pi->inside, ";\n");
	dump_address("outside", &pi->outside, ";\n");
	dump_options2("options", &pi->options, dump_proxy_plugins, &pi->plugins);
	--indent;
	printI("}\n");
}

static void dump_volume(int has_lower, struct d_volume *vol)
{
	if (!vol->implicit) {
		printI("volume %d {\n", vol->vnr);
		++indent;
	}

	/* Handle volume of '_remote_host' */
	if (vol->device_minor == -1 && !vol->device
	&& !vol->disk && !vol->meta_disk && !vol->meta_index)
		goto out;

	dump_options2("disk", &vol->disk_options,
		      dump_peer_device_options, &vol->pd_options);

	if (vol->parsed_device || verbose) {
		printI("device%*s", -19 + INDENT_WIDTH * indent, "");
		if (vol->device)
			printf("%s ", esc(vol->device));
		printf("minor %d;\n", vol->device_minor);
	}

	if (!has_lower && (vol->parsed_disk || verbose))
		printA("disk", esc(vol->disk ? vol->disk : "none"));

	if (!has_lower && (vol->parsed_meta_disk || verbose) && vol->meta_disk) {
		if (!strcmp(vol->meta_index, "flexible"))
			printI(MDISK, "meta-disk", esc(vol->meta_disk));
		else if (!strcmp(vol->meta_index, "internal"))
			printA("meta-disk", "internal");
		else
			printI(MDISKI, "meta-disk", esc(vol->meta_disk),
			       vol->meta_index);
	}

	if (!vol->implicit) {
	out:
		--indent;
		printI("}\n");
	}
}

static void dump_host_info(struct d_host_info *hi)
{
	struct d_volume *vol;

	if (!hi) {
		printI("  # No host section data available.\n");
		return;
	}

	if (hi->implicit && !verbose)
		return;

	if (hi->lower) {
		printI("stacked-on-top-of %s {\n", esc(hi->lower->name));
		++indent;
		printI("# on %s \n", names_to_str(&hi->on_hosts));
	} else if (hi->by_address) {
		dump_address("floating", &hi->address, " {\n");
		++indent;
	} else {
		printI("on %s {\n", names_to_str(&hi->on_hosts));
		++indent;
	}
	printI("node-id %s;\n", hi->node_id);

	dump_options("options", &hi->res_options);

	for_each_volume(vol, &hi->volumes) {
		if (vol->parsed_device || vol->parsed_disk || vol->parsed_meta_disk || verbose)
			dump_volume(!!hi->lower, vol);
	}

	if (!hi->by_address && hi->address.addr)
		dump_address("address", &hi->address, ";\n");
	if (hi->proxy_compat_only && !verbose)
		dump_proxy_info("", hi->proxy_compat_only);
	--indent;
	printI("}\n");
}

static void dump_hname_address_pairs(struct hname_address_pairs *hname_address_pairs)
{
	struct hname_address *ha;

	STAILQ_FOREACH(ha, hname_address_pairs, link) {
		if (ha->by_address || ha->faked_hostname) {
			dump_address("address", &ha->address,
				ssprintf("%s# on %s\n", ha->proxy ? " " : "; ", ha->name));
		} else {
			printI("host %s", ha->name);
			if (ha->parsed_address || (verbose && ha->address.addr))
				dump_address(" address", &ha->address, "");
			else if (ha->parsed_port)
				printf(" port %s", ha->address.port);
		}

		if (ha->proxy)
			dump_proxy_info(" via ", ha->proxy);
		else
			printf(";\n");
	}
}

static void dump_connection(struct connection *conn)
{
	struct peer_device *pd;
	struct path *path;

	if (conn->implicit && !verbose)
		return;

	printI("connection");
	if (conn->name)
		printf(" %s", esc(conn->name));
	printf(" {\n");
	++indent;

	path = STAILQ_FIRST(&conn->paths);
	if (path->implicit && !verbose) {
		dump_hname_address_pairs(&path->hname_address_pairs);
	} else {
		for_each_path(path, &conn->paths) {
			printI("path {\n");
			++indent;
			dump_hname_address_pairs(&path->hname_address_pairs);
			--indent;
			printI("}\n");
		}
	}

	dump_options("net", &conn->net_options);
	dump_options("disk", &conn->pd_options);
	STAILQ_FOREACH(pd, &conn->peer_devices, connection_link) {
		if (pd->implicit && !verbose)
			continue;
		printI("volume %d {\n", pd->vnr);
		++indent;
		dump_options("disk", &pd->pd_options);
		--indent;
		printI("}\n");
	}
	--indent;
	printI("}\n");
}

static void __dump_options_xml(struct options *options)
{
	struct d_option *option;

	STAILQ_FOREACH(option, options, link) {
		if (option->value)
			printI("<option name=\"%s\" value=\"%s\"/>\n",
			       option->name,
			       option->is_escaped ? option->value : esc_xml(option->
									    value));
		else
			printI("<option name=\"%s\"/>\n", option->name);
	}
}

static void dump_options_xml2(char *name, struct options *options,
			      void(*within)(struct options *), struct options *ctx)
{
	if (STAILQ_EMPTY(options) && (!ctx || (ctx && STAILQ_EMPTY(ctx))))
		return;

	printI("<section name=\"%s\">\n", name);
	++indent;
	__dump_options_xml(options);
	if (within)
		within(ctx);
	--indent;
	printI("</section>\n");
}

static void dump_peer_device_options_xml(struct options *options)
{
	if (!STAILQ_EMPTY(options))
		__dump_options_xml(options);
}

static void dump_options_xml(char *name, struct options *options)
{
	dump_options_xml2(name, options, NULL, NULL);
}

static void dump_proxy_plugins_xml(struct options *options)
{
	dump_options_xml("plugin", options);
}

static void dump_global_info_xml()
{
	if (!global_options.minor_count
	    && !global_options.disable_ip_verification
	    && global_options.dialog_refresh == 1)
		return;
	printI("<global>\n");
	++indent;
	if (global_options.disable_ip_verification)
		printI("<disable-ip-verification/>\n");
	if (global_options.minor_count)
		printI("<minor-count count=\"%i\"/>\n",
		       global_options.minor_count);
	if (global_options.dialog_refresh != 1)
		printI("<dialog-refresh refresh=\"%i\"/>\n",
		       global_options.dialog_refresh);
	--indent;
	printI("</global>\n");
}

static void dump_common_info_xml()
{
	if (!common)
		return;
	printI("<common>\n");
	++indent;
	fake_startup_options(common);
	dump_options_xml("options", &common->res_options);
	dump_options_xml("net", &common->net_options);
	dump_options_xml2("disk", &common->disk_options,
			  dump_peer_device_options_xml, &common->pd_options);
	dump_options_xml("startup", &common->startup_options);
	dump_options_xml2("proxy", &common->proxy_options,
			  dump_proxy_plugins, &common->proxy_plugins);
	dump_options_xml("handlers", &common->handlers);
	--indent;
	printI("</common>\n");
}

static void dump_proxy_info_xml(struct d_proxy_info *pi)
{
	printI("<proxy hostname=\"%s\">\n", names_to_str(&pi->on_hosts));
	++indent;
	printI("<inside family=\"%s\" port=\"%s\">%s</inside>\n", pi->inside.af,
	       pi->inside.port, pi->inside.addr);
	printI("<outside family=\"%s\" port=\"%s\">%s</outside>\n",
	       pi->outside.af, pi->outside.port, pi->outside.addr);
	dump_options_xml2("options", &pi->options, dump_proxy_plugins_xml, &pi->plugins);
	--indent;
	printI("</proxy>\n");
}

static void dump_volume_xml(struct d_volume *vol)
{
	printI("<volume vnr=\"%d\">\n", vol->vnr);
	++indent;

	dump_options_xml2("disk", &vol->disk_options,
			  dump_peer_device_options_xml, &vol->pd_options);
	printI("<device minor=\"%d\">%s</device>\n", vol->device_minor,
	       esc_xml(vol->device));
	printI("<disk>%s</disk>\n", esc_xml(vol->disk));

	if (vol->meta_index) {
		/* Stacked do not have this... */
		if (!strcmp(vol->meta_index, "flexible"))
			printI("<meta-disk>%s</meta-disk>\n",
			       esc_xml(vol->meta_disk));
		else if (!strcmp(vol->meta_index, "internal"))
			printI("<meta-disk>internal</meta-disk>\n");
		else {
			printI("<meta-disk index=\"%s\">%s</meta-disk>\n",
			       vol->meta_index, esc_xml(vol->meta_disk));
		}
	}
	--indent;
	printI("</volume>\n");
}

static void dump_host_info_xml(struct d_host_info *hi)
{
	struct d_volume *vol;

	if (!hi) {
		printI("<!-- No host section data available. -->\n");
		return;
	}

	if (hi->by_address)
		printI("<host floating=\"1\">\n");
	else
		printI("<host name=\"%s\">\n", names_to_str(&hi->on_hosts));

	++indent;

	dump_options_xml("options", &hi->res_options);
	for_each_volume(vol, &hi->volumes)
		dump_volume_xml(vol);

	printI("<address family=\"%s\" port=\"%s\">%s</address>\n",
	       hi->address.af, hi->address.port, hi->address.addr);
	--indent;
	printI("</host>\n");
}

static void dump_hname_address_pairs_xml(struct hname_address_pairs *hname_address_pairs)
{
	struct hname_address *ha;

	STAILQ_FOREACH(ha, hname_address_pairs, link) {
		printI("<host name=\"%s\">", ha->name);
		if (ha->proxy) {
			printf("\n");
			++indent;
			printI("");
		}
		if (ha->address.addr)
			printf("<address family=\"%s\" port=\"%s\">%s</address>",
			       ha->address.af, ha->address.port, ha->address.addr);
		else
			printf("<address family=\"%s\" port=\"%s\">%s</address>",
			       ha->host_info->address.af, ha->host_info->address.port,
			       ha->host_info->address.addr);
		if (ha->proxy) {
			printf("\n");
			dump_proxy_info_xml(ha->proxy);
			--indent;
			printI("</host>\n");
		} else {
			printf("</host>\n");
		}
	}
}

static void dump_connection_xml(struct connection *conn)
{
	struct peer_device *pd;
	struct path *path;


	if (conn->name)
		printI("<connection name=\"%s\">\n", esc_xml(conn->name));
	else
		printI("<connection>\n");
	++indent;

	path = STAILQ_FIRST(&conn->paths);
	if (path->implicit && !verbose) {
		dump_hname_address_pairs_xml(&path->hname_address_pairs);
	} else {
		for_each_path(path, &conn->paths) {
			printI("<path>\n");
			++indent;
			dump_hname_address_pairs_xml(&path->hname_address_pairs);
			--indent;
			printI("</path>\n");
		}
	}

	dump_options_xml("net", &conn->net_options);
	dump_options_xml("disk", &conn->pd_options);
	STAILQ_FOREACH(pd, &conn->peer_devices, connection_link) {
		if (pd->implicit && !verbose)
			continue;
		printI("<volume vnr=\"%d\">\n", pd->vnr);
		++indent;
		dump_options_xml("disk", &pd->pd_options);
		--indent;
		printI("</volume>\n");
	}
	--indent;
	printI("</connection>\n");
}

static void fake_startup_options(struct d_resource *res)
{
	struct d_option *opt;

	if (res->stacked_timeouts) {
		opt = new_opt(strdup("stacked-timeouts"), NULL);
		insert_tail(&res->startup_options, opt);
	}
}

static void dump_mesh(struct d_resource *res)
{
	struct mesh *mesh;

	STAILQ_FOREACH(mesh, &res->meshes, link) {
		struct d_name *h;

		printI("connection-mesh {\n");
		++indent;
		printI("hosts");
		STAILQ_FOREACH(h, &mesh->hosts, link)
			printf(" %s", h->name);
		printf(";\n");
		dump_options("net", &mesh->net_options);
		--indent;
		printI("}\n");
	}
}

int adm_dump(const struct cfg_ctx *ctx)
{
	struct d_host_info *host;
	struct d_resource *res = ctx->res;
	struct connection *conn;
	struct d_volume *vol;

	if (!res) {
		printI("# no resources configured\n");
		return 0;
	}

	printI("# resource %s on %s: %s, %s\n",
	       esc(res->name), hostname,
	       res->ignore ? "ignored" : "not ignored",
	       res->stacked ? "stacked" : "not stacked");
	printI("# defined at %s:%u\n", res->config_file, res->start_line);
	printI("resource %s {\n", esc(res->name));
	++indent;

	if (res->template)
		printI("template-file %s;\n", esc(res->template->config_file));

	for_each_volume(vol, &res->volumes) {
		if (!verbose && (vol->parsed_device || vol->parsed_disk || vol->parsed_meta_disk))
			dump_volume(res->stacked, vol);
	}

	for_each_host(host, &res->all_hosts)
		dump_host_info(host);

	for_each_connection(conn, &res->connections)
		dump_connection(conn);

	if (!verbose)
		dump_mesh(res);

	fake_startup_options(res);
	dump_options("options", &res->res_options);
	dump_options("net", &res->net_options);
	dump_options2("disk", &res->disk_options,
		      dump_peer_device_options, &res->pd_options);
	dump_options("startup", &res->startup_options);
	dump_options2("proxy", &res->proxy_options,
			dump_proxy_plugins, &res->proxy_plugins);
	dump_options("handlers", &res->handlers);
	--indent;
	printf("}\n\n");

	return 0;
}

int adm_dump_xml(const struct cfg_ctx *ctx)
{
	struct d_host_info *host;
	struct d_resource *res = ctx->res;
	struct connection *conn;

	if (!res) {
		printI("<!-- No resources configured -->\n");
		return 0;
	}

	printI("<resource name=\"%s\" conf-file-line=\"%s:%u\">\n",
		esc_xml(res->name),
		esc_xml(res->config_file), res->start_line);
	++indent;

	if (res->template)
		printI("<template file=\"%s\">\n", esc_xml(res->template->config_file));

	// else if (common && common->protocol) printA("# common protocol", common->protocol);
	for_each_host(host, &res->all_hosts)
		dump_host_info_xml(host);
	for_each_connection(conn, &res->connections)
		dump_connection_xml(conn);
	fake_startup_options(res);
	dump_options_xml("options", &res->res_options);
	dump_options_xml("net", &res->net_options);
	dump_options_xml2("disk", &res->disk_options,
			  dump_peer_device_options_xml, &res->pd_options);
	dump_options_xml("startup", &res->startup_options);
	dump_options_xml2("proxy", &res->proxy_options,
			dump_proxy_plugins_xml, &res->proxy_plugins);
	dump_options_xml("handlers", &res->handlers);
	--indent;
	printI("</resource>\n");

	return 0;
}

void print_dump_xml_header(void)
{
	printf("<config file=\"%s\">\n", config_save);
	++indent;
	dump_global_info_xml();
	dump_common_info_xml();
}

void print_dump_header(void)
{
	printf("# %s\n", config_save);
	dump_global_info();
	dump_common_info();
}
