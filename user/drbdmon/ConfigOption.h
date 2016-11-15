#ifndef CONFIGOPTION_H
#define	CONFIGOPTION_H

#include <stdexcept>
#include <string>

#include <Configurable.h>

class ConfigOption
{
  public:
    ConfigOption(bool make_flag, const std::string& key_ref);

    ConfigOption(const ConfigOption& orig) = delete;
    ConfigOption& operator=(const ConfigOption& orig) = delete;
    ConfigOption(ConfigOption&& orig) = default;
    ConfigOption& operator=(ConfigOption&& orig) = default;

    virtual ~ConfigOption() noexcept;

    const bool is_flag;
    const std::string& key;
};

#endif	/* CONFIGOPTION_H */
