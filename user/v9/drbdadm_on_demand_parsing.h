#include <stdbool.h>

struct parser_file_state {
	// Canonical path of the configuration file
	const char *canon_path;
	// Indicates that this configuration file was loaded, e.g.
	// due to an explicit include line for that file.
	// Used to avoid duplicate loading of the same configuration file.
	bool is_loaded;
	// Indicates that this configuration file should be ignored (never loaded)
	bool prevent_load;
};

struct parser_res_state {
	// DRBD resource name (may be assumed based on the config file name)
	const char *res_name;
	// List of canonical paths to configurations files that may contain the associated resource configuration
	void *parser_file_state_map;
	// Indicates that an attempt to load all configuration file candidates for this
	// resource was made already.
	// If is_loaded == false, it makes sense to load all configuration file candidates
	// for this resource only first and lookup the resource again, before falling back
	// to loading all configuration files (for all resources).
	bool is_loaded;
};

void debug_on_demand_parsing(void);

const char *create_res_name_from_path(const char *const path);
struct parser_file_state *add_cfgfile_by_path(const char *const path);
