#ifndef CFGENTRY_H
#define CFGENTRY_H

#include <cstdint>
#include <new>
#include <memory>
#include <string>
#include <istream>
#include <ostream>

class CfgEntry
{
  public:
    static const size_t     MAX_KEY_LENGTH;

    static const uint32_t   TYPE_CFG_VERSION;
    static const uint32_t   TYPE_BOOLEAN;
    static const uint32_t   TYPE_UNSGINT8;
    static const uint32_t   TYPE_UNSGINT16;
    static const uint32_t   TYPE_UNSGINT32;
    static const uint32_t   TYPE_UNSGINT64;
    static const uint32_t   TYPE_INT8;
    static const uint32_t   TYPE_INT16;
    static const uint32_t   TYPE_INT32;
    static const uint32_t   TYPE_INT64;
    static const uint32_t   TYPE_BINARY;

    CfgEntry();
    CfgEntry(const std::string& entry_key);
    virtual ~CfgEntry() noexcept;

    virtual const std::string& get_key() const;
    virtual uint32_t get_type() const = 0;
    virtual void serialize(std::ostream& data_out) const = 0;
    virtual void deserialize(std::istream& data_in) = 0;

  protected:
    virtual void serialize_key(std::ostream& data_out) const;
    virtual void deserialize_key(std::istream& data_in);

    std::string key;
};

#endif /* CFGENTRY_H */
