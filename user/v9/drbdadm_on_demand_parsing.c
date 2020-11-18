#include "drbdadm_on_demand_parsing.h"
#include "shared_main.h"
#include "shared_tool.h"
#include "shared_parser.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>

static void load_resource_file_state(const void *const node, const VISIT phase, const int depth);
static void load_all_resources_res_state(const void *const node, const VISIT phase, const int depth);
static ENTRY *get_or_create_res_map_entry(const char *const path);
static ENTRY *get_or_create_res_map_entry(const char *const path);
static struct parser_file_state *create_parser_file_state(const char *const canon_path,
                                                          const bool is_loaded,
                                                          const bool prevent_load);
static struct parser_res_state *create_parser_res_state(const char *const res_name, const bool is_loaded);
static ENTRY *create_entry(const char* const key, const void *const value);
static void mark_res_state_loaded(const void *const node, const VISIT phase, const int depth);
static void file_state_check(const void *const node, const VISIT phase, const int depth);
static void debug_list_global_res_map(void);
static void debug_list_global_path_map(void);
static void debug_list_res_state_node(const void *const node, const VISIT phase, const int depth);
static void debug_list_file_state_node(const void *const node, const VISIT phase, const int depth);
static void debug_short_list_res_state_node(const void *const node, const VISIT phase, const int depth);

bool defer_load = true;
bool res_map_changed = false;

/*
 * Map of canonical configuration file path to parser_res_state
 * Set of ENTRY with key = canonical config file path, value = struct parser_res_state
 */
void *global_path_map = NULL;

/*
 * Map of (presumed) resource name to parser_res_state
 * Set of ENTRY with key = resource name, value = struct parser_res_state
 */
void *global_res_map = NULL;

/**
 * Flag used for determining, during a tree walk, whether all configuration files
 * that are associated with a resource are loaded. This should normally be a local
 * variable, but the tree walks require a callback method that does not support
 * any additional context (like a pointer to a local variable).
 */
bool all_files_loaded = false;

/**
 * Guesses the name of the DRBD resource based on the path of
 * the configuration file, e.g. "/etc/drbd.d/archive.res" will
 * result in a resource name of "archive".
 * Memory for the resource name is allocated and must be released by the caller.
 *
 * Like other drbdadm functions, this function exits the program if memory allocation fails.
 *
 * @returns: Newly allocated resource name string or NULL if path is NULL
 */
const char *create_res_name_from_path(const char *const path)
{
	char *res_name = NULL;
	if (path != NULL) {
		// Isolate file name from path
		const char *separator = strrchr(path, '/');
		const char *const file_name = (separator == NULL ? path : &separator[1]);

		// Remove file name suffixes
		separator = strchr(file_name, '.');
		if (separator == NULL) {
			// No suffix, copy the entire file name
			res_name = strdup(file_name);
			if (res_name == NULL) {
				err("Cannot allocate memory for a resource name: %m");
				exit(E_THINKO);
			}
		} else {
			// Copy the part of the file name before the suffixes to a new string
			const size_t res_name_length = (size_t) (separator - file_name);
			res_name = malloc(res_name_length + 1);
			if (res_name != NULL) {
				memcpy(res_name, file_name, res_name_length);
				res_name[res_name_length] = '\0';
			} else {
				err("Cannot allocate memory for a resource name: %m");
				exit(E_THINKO);
			}
		}
	}
	return res_name;
}

struct parser_file_state *add_cfgfile_by_path(const char *const path)
{
	struct parser_file_state *file_state = NULL;
	if (path == NULL) {
		return file_state;
	}

	const char *const canon_path = canonicalize_path(path);
	if (canon_path == NULL) {
		err("Out of memory: Cannot allocate space required to canonicalize path");
		exit(E_THINKO);
	}

	// Check whether the configuration file path is already registered
	{
		const ENTRY path_search_key = {(char *) canon_path, NULL};
		void *path_entry_node = tfind(&path_search_key, &global_path_map, &btree_key_cmp);
		// If there is no entry for this path, create a new one
		if (path_entry_node == NULL) {
			ENTRY *const res_entry = get_or_create_res_map_entry(canon_path);
			struct parser_res_state *const res_state = res_entry->data;
			file_state = create_parser_file_state(canon_path, false, false);
			ENTRY *path_entry = create_entry(canon_path, res_state);

			// Create an entry for the configuration file path in the associated resource's
			// map of configuration file paths
			{
				ENTRY *const file_entry = create_entry(canon_path, file_state);
				void *const ins_entry_node = tsearch(file_entry,
				                                     &(res_state->parser_file_state_map),
				                                     &btree_key_cmp);
				if (ins_entry_node == NULL) {
					err("Tree insertion into the file state map for a resource failed: Out of memory");
					exit(E_THINKO);
				}
			}

			// Create an entry for the configuration file path in the global path map
			{
				void *const ins_entry_node = tsearch(path_entry,
				                                     &global_path_map,
				                                     &btree_key_cmp);
				if (ins_entry_node == NULL) {
					err("Tree insertion into the path map failed: Out of memory");
					exit(E_THINKO);
				}
			}
		} else {
			const ENTRY *const path_entry = *((ENTRY **) path_entry_node);
			struct parser_res_state *const res_state = path_entry->data;
			void *const file_entry_node = tfind(&path_search_key,
			                                    &(res_state->parser_file_state_map),
			                                    &btree_key_cmp);
			if (file_entry_node == NULL) {
				err("Implementation error: Parser file state information not found for a registered configuration file path\n");
				exit(E_THINKO);
			}
			const ENTRY *const file_entry = *((ENTRY **) file_entry_node);
			file_state = file_entry->data;
			free((void *) canon_path);
		}

		return file_state;
	}
}

/**
 * Loads all configuration files that are associated with the specified resource name
 */
void load_resource(const char *const res_name)
{
	res_map_changed = false;
	do {
		const ENTRY res_search_key = {(char *) res_name, NULL};
		void *const res_entry_node = tfind(&res_search_key, &global_res_map, &btree_key_cmp);
		if (res_entry_node != NULL) {
			const ENTRY *const res_entry = *((ENTRY **) res_entry_node);
			struct parser_res_state *const res_state = res_entry->data;
			if (!res_state->is_loaded) {
				twalk(res_state->parser_file_state_map, &load_resource_file_state);
			}
		}
	} while (res_map_changed);
	mark_resources_loaded();
}

/**
 * Loads all configuration files that are not loaded yet
 */
void load_all_resources(void)
{
	res_map_changed = false;
	do {
		twalk(global_res_map, &load_all_resources_res_state);
	} while (res_map_changed);
	mark_resources_loaded();
}

/**
 * Per-node handler function called by load_all_resources
 */
static void load_all_resources_res_state(const void *const node, const VISIT phase, const int depth)
{
	const ENTRY *const res_entry = *((ENTRY **) node);
	struct parser_res_state *const res_state = res_entry->data;
	if (!res_state->is_loaded) {
		twalk(res_state->parser_file_state_map, &load_resource_file_state);
	}
}

/**
 * Per-node handler function called by load_resources and load_all_resources
 * (through its load_all_resoures_res_state per-node handler function)
 *
 * Initiates loading of the configuration file specified by an entry in the file state map
 * associated with a resource. This function is called for each entry in the resource's
 * file state map.
 */
static void load_resource_file_state(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const file_entry = *((ENTRY **) node);
		struct parser_file_state *const file_state = file_entry->data;
		if (!file_state->is_loaded && !file_state->prevent_load) {
			if (load_config_file(file_state->canon_path)) {
				file_state->is_loaded = true;
			}
		}
	}
}

static ENTRY *get_or_create_res_map_entry(const char *const path) {
	// Derive (guess) the name of the resource that this configuration file probably describes
	const char *const res_name = create_res_name_from_path(path);

	ENTRY *res_entry = NULL;
	// Look for an existing entry for the resource
	const ENTRY res_search_key = {(char *) res_name, NULL};
	void *res_entry_node = tfind(&res_search_key, &global_res_map, &btree_key_cmp);
	// If there is no resource entry, create a new one with an empty map
	// of associated configuration file paths
	if (res_entry_node == NULL) {
		struct parser_res_state *const res_state = create_parser_res_state(res_name, false);
		res_entry = create_entry(res_name, res_state);
		const ENTRY *const ins_entry = tsearch(res_entry, &global_res_map, &btree_key_cmp);
		if (ins_entry == NULL) {
			err("Tree insertion into the resource map failed: Out of memory");
			exit(E_THINKO);
		}
	} else {
		res_entry = *((ENTRY **) res_entry_node);
		free((void *) res_name);
	}
	return res_entry;
}

static struct parser_file_state *create_parser_file_state(const char *const canon_path,
                                                   const bool is_loaded,
                                                   const bool prevent_load) {
	struct parser_file_state *const new_state = malloc(sizeof (*new_state));
	if (new_state == NULL) {
		err("Cannot allocate a new configuration file state entry: %m");
		exit(E_THINKO);
	}
	new_state->canon_path = canon_path;
	new_state->is_loaded = is_loaded;
	new_state->prevent_load = prevent_load;
	return new_state;
}

static struct parser_res_state *create_parser_res_state(const char *const res_name, const bool is_loaded) {
	struct parser_res_state *const new_state = malloc(sizeof (*new_state));
	if (new_state == NULL) {
		err("Cannot allocate a new resource state entry: %m");
		exit(E_THINKO);
	}
	new_state->res_name = res_name;
	new_state->is_loaded = is_loaded;
	new_state->parser_file_state_map = NULL;
	return new_state;
}

static ENTRY *create_entry(const char *const key, const void* const value) {
	ENTRY *const new_entry = malloc(sizeof (*new_entry));
	if (new_entry == NULL) {
		err("Cannot allocate a new map entry: %m");
		exit(E_THINKO);
	}
	new_entry->key = (char *) key;
	new_entry->data = (void *) value;
	return new_entry;
}

bool is_defer_exempt(const char *const path)
{
	const char *file_name = strrchr(path, '/');
	if (file_name != NULL) {
		++file_name;
	} else {
		file_name = path;
	}
	return strcmp(file_name, "linstor-resources.res") == 0;
}

void mark_resources_loaded(void)
{
	twalk(global_res_map, &mark_res_state_loaded);
}

static void mark_res_state_loaded(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const map_entry = *((ENTRY **) node);
		struct parser_res_state *const res_state = map_entry->data;
		all_files_loaded = true;
		twalk(res_state->parser_file_state_map, &file_state_check);
		res_state->is_loaded = all_files_loaded;
	}
}

static void file_state_check(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const map_entry = *((ENTRY **) node);
		const struct parser_file_state *const file_state = map_entry->data;
		if (!file_state->is_loaded) {
			all_files_loaded = false;
		}
	}
}

void debug_on_demand_parsing(void)
{
	debug_list_global_res_map();
	debug_list_global_path_map();
}

void debug_loaded_resources(void)
{
	fputs("\x1b[38;5;196mDEBUG\x1B[0m \x1b[38;5;226mMappings of resource name to parser state:\x1b[0m\n", stderr);
	twalk(global_res_map, &debug_short_list_res_state_node);
	fputc('\n', stderr);
}

static void debug_list_global_res_map(void)
{
	fputs("\x1b[38;5;196mDEBUG\x1B[0m \x1b[38;5;226mMappings of resource name to parser state:\x1b[0m\n", stderr);
	twalk(global_res_map, &debug_list_res_state_node);
	fputc('\n', stderr);
}

static void debug_list_global_path_map(void)
{
	fputs("\x1b[38;5;196mDEBUG\x1B[0m \x1b[38;5;226mMappings of file path to parser state:\x1b[0m\n", stderr);
	twalk(global_path_map, &debug_list_res_state_node);
	fputc('\n', stderr);
}

static void debug_list_res_state_node(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const map_entry = *((ENTRY **) node);
		const struct parser_res_state *const res_state = map_entry->data;

		fprintf(stderr, "    Key:    \"\x1B[38;5;40m%s\x1B[0m\"\n", map_entry->key);
		fputs("    Value:\n", stderr);
		fprintf(stderr, "        res_name:       \x1B[38;5;40m%s\x1B[0m\n", (res_state->res_name));
		fprintf(stderr, "        is_loaded:      \x1B[38;5;40m%s\x1B[0m\n", (res_state->is_loaded ? "true" : "false"));
		fputs("        Mapped paths:\n", stderr);
		twalk(res_state->parser_file_state_map, &debug_list_file_state_node);
	}
}

static void debug_list_file_state_node(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const map_entry = *((ENTRY **) node);
		const struct parser_file_state *const file_state = map_entry->data;

		fprintf(stderr, "            Key:    \"\x1B[38;5;40m%s\x1B[0m\"\n", map_entry->key);
		fputs("            Value:\n", stderr);
		fprintf(stderr, "                Path:           \"\x1B[38;5;40m%s\x1B[0m\"\n", file_state->canon_path);
		fprintf(stderr, "                is_loaded:      \x1B[38;5;40m%s\x1B[0m\n", (file_state->is_loaded ? "true" : "false"));
		fprintf(stderr, "                prevent_load:   \x1B[38;5;40m%s\x1B[0m\n", (file_state->prevent_load ? "true" : "false"));
	}
}

static void debug_short_list_res_state_node(const void *const node, const VISIT phase, const int depth)
{
	if (phase == postorder || phase == leaf) {
		const ENTRY *const map_entry = *((ENTRY **) node);
		const struct parser_res_state *const res_state = map_entry->data;

		fprintf(stderr, "Resource:   \x1B[38;5;40m%-48s\x1B[0m %s\n",
		        res_state->res_name, (res_state->is_loaded ? "\x1B[38;5;40mLOADED\x1B[0m" : "\x1B[38;5;20mnot loaded\x1B[0m"));
	}
}
