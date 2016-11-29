#include <ConfigOption.h>

ConfigOption::ConfigOption(bool make_flag, const std::string& key_ref):
    is_flag(make_flag),
    key(key_ref)
{
}

ConfigOption::~ConfigOption() noexcept
{
}
