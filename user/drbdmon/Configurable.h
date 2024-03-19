#ifndef CONFIGURABLE_H
#define	CONFIGURABLE_H

#include <default_types.h>
#include <string>

class Configurator;

class Configurable
{
  public:
    virtual ~Configurable() noexcept
    {
    }

    // @throws std::bad_alloc
    virtual void announce_options(Configurator& collector) = 0;

    virtual void options_help() noexcept = 0;

    // @throws std::bad_alloc, ConfigurationException
    virtual void set_flag(const std::string& key) = 0;

    // @throws std::bad_alloc, ConfigurationException
    virtual void set_option(const std::string& key, const std::string& value) = 0;
};

#endif	/* CONFIGURABLE_H */
