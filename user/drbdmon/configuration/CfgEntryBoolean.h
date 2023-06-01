#ifndef CFGENTRYBOOLEAN_H
#define CFGENTRYBOOLEAN_H

#include <configuration/CfgEntry.h>
#include <configuration/IoException.h>
#include <istream>
#include <ostream>

class CfgEntryBoolean : public CfgEntry
{
  public:
    using CfgEntry::CfgEntry;
    CfgEntryBoolean();
    CfgEntryBoolean(const std::string& entry_key, const bool value);
    virtual ~CfgEntryBoolean() noexcept;

    virtual uint32_t get_type() const;
    const bool get_value() const;

    void serialize(std::ostream& data_out) const;
    void deserialize(std::istream& data_in);

  private:
    bool    flag    {false};
};

#endif /* CFGENTRYBOOLEAN_H */
