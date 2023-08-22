#ifndef CONFIGURATOR_H
#define	CONFIGURATOR_H

#include <default_types.h>

class Configurable;
class ConfigOption;

class Configurator
{
  public:
    virtual ~Configurator() noexcept
    {
    }

    // @throws std::bad_alloc
    virtual void add_config_option(Configurable& owner, const ConfigOption& option) = 0;
};

#endif	/* CONFIGURATOR_H */
