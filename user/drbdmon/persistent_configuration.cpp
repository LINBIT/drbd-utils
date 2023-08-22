#include <default_types.h>
#include <persistent_configuration.h>
#include <configuration/IoException.h>
#include <configuration/CfgEntryStore.h>
#include <configuration/CfgEntryBoolean.h>
#include <configuration/CfgEntryIntegerTypes.h>
#include <exceptions.h>
#include <string_transformations.h>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <dsaext.h>
#include <integerparse.h>

namespace configuration
{
    const size_t    KEY_WIDTH       = 44;
    const size_t    VALUE_WIDTH     = 24;
    const size_t    TYPE_WIDTH      = 12;

    static void no_entry_for_key(const std::string& entry_key);
    static void throw_invalid_value(const std::string& config_value);

    void initialize_cfg_entry_store(CfgEntryStore& config_store)
    {
        // Initialize the configuration store using a temporary default configuration
        {
            std::unique_ptr<Configuration> default_config(new Configuration());
            default_config->save_to(config_store);
        }
    }

    void load_configuration(
        const std::string   file_path,
        Configuration&      config,
        CfgEntryStore&      config_store,
        SystemApi&          sys_api
    )
    {
        std::ifstream data_in(file_path);
        config_store.load_from(data_in);
        config.load_from(config_store);
    }

    void save_configuration(
        const std::string   file_path,
        Configuration&      config,
        CfgEntryStore&      config_store,
        SystemApi&          sys_api
    )
    {
        std::ofstream data_out(file_path);
        config.save_to(config_store);
        config_store.save_to(data_out);
    }

    void display_configuration(CfgEntryStore& config_store)
    {
        CfgEntryStore::CfgMap::ValuesIterator cfg_iter = config_store.iterator();
        while (cfg_iter.has_next())
        {
            const CfgEntry* const entry = cfg_iter.next();
            display_entry(entry);
        }
    }

    void display_entry(const CfgEntry* const entry)
    {
        const std::string entry_key = entry->get_key();
        const uint32_t entry_type = entry->get_type();

        std::cout << std::setw(KEY_WIDTH) << std::left << entry_key;
        if (entry_type == CfgEntry::TYPE_BOOLEAN)
        {
            const CfgEntryBoolean* const typed_entry = dynamic_cast<const CfgEntryBoolean*> (entry);
            const bool entry_flag = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "Boolean";
            std::cout << "[";
            std::cout << (entry_flag ? "True" : "False");
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_UNSGINT8)
        {
            const CfgEntryUnsgInt8* const typed_entry = dynamic_cast<const CfgEntryUnsgInt8*> (entry);
            const uint8_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "UnsgInt8";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<unsigned int> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_UNSGINT16)
        {
            const CfgEntryUnsgInt16* const typed_entry = dynamic_cast<const CfgEntryUnsgInt16*> (entry);
            const uint16_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "UnsgInt16";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<unsigned int> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_UNSGINT32)
        {
            const CfgEntryUnsgInt32* const typed_entry = dynamic_cast<const CfgEntryUnsgInt32*> (entry);
            const uint32_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "UnsgInt32";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<unsigned long> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_UNSGINT64)
        {
            const CfgEntryUnsgInt64* const typed_entry = dynamic_cast<const CfgEntryUnsgInt64*> (entry);
            const uint64_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "UnsgInt64";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right <<
                static_cast<unsigned long long> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_INT8)
        {
            const CfgEntryInt8* const typed_entry = dynamic_cast<const CfgEntryInt8*> (entry);
            const int8_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "Int8";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<int> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_INT16)
        {
            const CfgEntryInt16* const typed_entry = dynamic_cast<const CfgEntryInt16*> (entry);
            const int16_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "Int16";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<int> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_INT32)
        {
            const CfgEntryInt32* const typed_entry = dynamic_cast<const CfgEntryInt32*> (entry);
            const int32_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "Int32";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<long> (entry_value);
            std::cout << "]";
        }
        else
        if (entry_type == CfgEntry::TYPE_INT64)
        {
            const CfgEntryInt64* const typed_entry = dynamic_cast<const CfgEntryInt64*> (entry);
            const int64_t entry_value = typed_entry->get_value();
            std::cout << std::setw(TYPE_WIDTH) << std::left << "Int64";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::right << static_cast<long long> (entry_value);
            std::cout << "]";
        }
        else
        {
            std::cout << std::setw(TYPE_WIDTH) << std::left << "UNKNOWN";
            std::cout << "[";
            std::cout << std::setw(VALUE_WIDTH) << std::left << "UNAVAILABLE";
            std::cout << "]";
        }
        std::cout << std::endl;
    }

    void get_configuration_entry(
        CfgEntryStore&      config_store,
        const std::string&  config_key
    )
    {
        const CfgEntry* const entry = config_store.get_entry(config_key);
        if (entry != nullptr)
        {
            display_entry(entry);
        }
        else
        {
            no_entry_for_key(config_key);
        }
    }

    void set_configuration_entry(
        CfgEntryStore&      config_store,
        const std::string&  config_key,
        const std::string&  config_value,
        MonitorEnvironment& mon_env
    )
    {
        const CfgEntry* entry = config_store.get_entry(config_key);
        if (entry != nullptr)
        {
            const uint32_t entry_type = entry->get_type();
            try
            {
                std::unique_ptr<CfgEntry> new_entry;
                if (entry_type == CfgEntry::TYPE_BOOLEAN)
                {
                    std::string value = string_transformations::uppercase_copy_of(config_value);
                    bool flag = false;
                    if (value == "TRUE")
                    {
                        flag = true;
                    }
                    else
                    if (value != "FALSE")
                    {
                        throw_invalid_value(config_value);
                    }
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryBoolean(config_key, flag));
                }
                else
                if (entry_type == CfgEntry::TYPE_UNSGINT8)
                {
                    const uint8_t value = dsaext::parse_unsigned_int8(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryUnsgInt8(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_UNSGINT16)
                {
                    const uint16_t value = dsaext::parse_unsigned_int16(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryUnsgInt16(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_UNSGINT32)
                {
                    const uint32_t value = dsaext::parse_unsigned_int32(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryUnsgInt32(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_UNSGINT64)
                {
                    const uint64_t value = dsaext::parse_unsigned_int64(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryUnsgInt64(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_INT8)
                {
                    const int8_t value = dsaext::parse_signed_int8(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryInt8(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_INT16)
                {
                    const int16_t value = dsaext::parse_signed_int16(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryInt16(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_INT32)
                {
                    const int32_t value = dsaext::parse_signed_int32(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryInt32(config_key, value));
                }
                else
                if (entry_type == CfgEntry::TYPE_INT64)
                {
                    const int64_t value = dsaext::parse_signed_int64(config_value);
                    new_entry = std::unique_ptr<CfgEntry>(new CfgEntryInt64(config_key, value));
                }
                else
                {
                    std::string error_message;
                    error_message = "Cannot change entry \"";
                    error_message += config_key;
                    error_message += "\": Unknown entry type";
                    throw ConfigurationException(error_message);
                }

                if (new_entry != nullptr)
                {
                    config_store.set_entry(new_entry.get());
                    entry = nullptr;

                    std::ofstream data_out(mon_env.config_file_path);
                    config_store.save_to(data_out);
                    CfgEntry* const changed_entry = new_entry.release();

                    std::cout << "Changed configuration entry:\n";
                    display_entry(changed_entry);
                }
            }
            catch (dsaext::NumberFormatException&)
            {
                throw_invalid_value(config_value);
            }
        }
        else
        {
            no_entry_for_key(config_key);
        }
    }

    void configuration_command(
        int                             argc,
        char*                           argv[],
        DrbdMonConsts::run_action_type  action,
        MonitorEnvironment&             mon_env
    )
    {
        std::unique_ptr<CfgEntryStore> config_store(new CfgEntryStore());
        initialize_cfg_entry_store(*config_store);
        try
        {
            if (mon_env.sys_api->is_file_accessible(mon_env.config_file_path.c_str()))
            {
                configuration::load_configuration(
                    mon_env.config_file_path,
                    *(mon_env.config),
                    *config_store,
                    *(mon_env.sys_api)
                );
            }
            else
            {
                std::cout << "** " << DrbdMonConsts::PROGRAM_NAME <<
                    ": Configuration file not present or not accessible\n";
                std::cout << "File path: " << mon_env.config_file_path << "\n";
                std::cout << "Using default configuration.\n\n" << std::flush;
            }

            if (action == DrbdMonConsts::run_action_type::DM_DSP_CONF)
            {
                display_configuration(*config_store);
            }
            else
            if (action == DrbdMonConsts::run_action_type::DM_GET_CONF)
            {
                const std::string config_key = get_config_key(argc, argv);
                get_configuration_entry(*config_store, config_key);
            }
            else
            if (action == DrbdMonConsts::run_action_type::DM_SET_CONF)
            {
                const std::string config_key = get_config_key(argc, argv);
                const std::string config_value = get_config_value(argc, argv);
                set_configuration_entry(*config_store, config_key, config_value, mon_env);
            }
        }
        catch (IoException& io_exc)
        {
            std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << " v" << DrbdMonConsts::UTILS_VERSION << '\n';
            std::cout << "Cannot access the " << DrbdMonConsts::PROGRAM_NAME  << " configuration store: I/O error\n";
            std::cout << "File path: " << mon_env.config_file_path << "\n";
        }
        std::cout << std::endl;
    }


    std::string get_config_key(int argc, char* argv[])
    {
        std::string key;
        if (argc >= 3)
        {
            key = argv[2];
        }
        else
        {
            std::string error_message;
            error_message += "Missing command line parameter: Configuration key";
            throw ConfigurationException(error_message);
        }
        return key;
    }

    std::string get_config_value(int argc, char* argv[])
    {
        std::string value;
        if (argc >= 4)
        {
            value = argv[3];
        }
        else
        {
            std::string error_message;
            error_message += "Missing command line parameter: Configuration value";
            throw ConfigurationException(error_message);
        }
        return value;
    }

    static void no_entry_for_key(const std::string& config_key)
    {
        std::cout << "No configuration entry with key \"" << config_key << "\"" << std::endl;
    }

    static void throw_invalid_value(const std::string& config_value)
    {
        std::string error_message;
        error_message = "Invalid configuration value \"";
        error_message += config_value;
        error_message += "\"";
        throw ConfigurationException(error_message);
    }
}
