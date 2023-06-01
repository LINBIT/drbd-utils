#ifndef CFGENTRYSTORE_H
#define CFGENTRYSTORE_H

#include <QTree.h>
#include <configuration/CfgEntry.h>
#include <string>
#include <new>
#include <memory>
#include <istream>
#include <ostream>

class CfgEntryStore
{
  public:
    using CfgMap = QTree<std::string, CfgEntry>;

    static const std::string    CES_UUID;
    static const uint32_t       CES_VERSION;

    CfgEntryStore();
    virtual ~CfgEntryStore() noexcept;

    virtual void set_entry(CfgEntry* const entry);
    virtual const CfgEntry* get_entry(const std::string& entry_key) const;
    virtual void delete_entry(const std::string& entry_key);

    virtual bool get_boolean(const std::string& entry_key, const bool default_value) const;
    virtual uint8_t get_unsgint8(const std::string& entry_key, const uint8_t default_value) const;
    virtual uint16_t get_unsgint16(const std::string& entry_key, const uint16_t default_value) const;
    virtual uint32_t get_unsgint32(const std::string& entry_key, const uint32_t default_value) const;
    virtual uint64_t get_unsgint64(const std::string& entry_key, const uint64_t default_value) const;
    virtual int8_t get_int8(const std::string& entry_key, const int8_t default_value) const;
    virtual int16_t get_int16(const std::string& entry_key, const int16_t default_value) const;
    virtual int32_t get_int32(const std::string& entry_key, const int32_t default_value) const;
    virtual int64_t get_int64(const std::string& entry_key, const int64_t default_value) const;

    virtual CfgMap::ValuesIterator iterator();

    virtual void load_from(std::istream& data_in);
    virtual void save_to(std::ostream& data_out) const;

  private:
    static constexpr size_t ENTRY_TYPE_SIZE = 4;

    std::unique_ptr<CfgMap> config_mgr;
    CfgMap*                 config;

    void clear_impl() noexcept;

    template<typename T, typename E>
    T get_numeric_entry(const std::string& entry_key, const T default_value, const uint32_t id) const
    {
        T value = default_value;
        const CfgEntry* const entry = get_entry(entry_key);
        if (entry != nullptr)
        {
            if (entry->get_type() == id)
            {
                const E* const typed_entry = dynamic_cast<const E*> (entry);
                value = typed_entry->get_value();
            }
        }
        return value;
    }
};

#endif /* CFGENTRYSTORE_H */
