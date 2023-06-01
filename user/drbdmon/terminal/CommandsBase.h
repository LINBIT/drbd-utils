#ifndef COMMANDSBASE_H_
#define COMMANDSBASE_H_

#include <string>
#include <new>
#include <memory>
#include <QTree.h>
#include <StringTokenizer.h>
#include <comparators.h>
#include <string_transformations.h>

template<class T>
class CommandsBase
{
  protected:
    typedef bool (T::*cmd_func_type)(const std::string& command, StringTokenizer& tokenizer);

    class Entry
    {
      public:
        Entry(const std::string* const key_ref, const cmd_func_type cmd_func_ref):
            key(key_ref),
            cmd_func(cmd_func_ref)
        {
        }

        virtual ~Entry() noexcept
        {
        }

        const std::string*  key         {nullptr};
        const cmd_func_type cmd_func    {nullptr};
    };

    using CommandMap = QTree<std::string, Entry>;

    std::unique_ptr<CommandMap> cmd_map;

  public:
    CommandsBase()
    {
        cmd_map = std::unique_ptr<CommandMap>(new CommandMap(&comparators::compare_string));
    }

    virtual ~CommandsBase() noexcept
    {
        cmd_map->clear();
    }

    virtual void add_command(const Entry& command_entry)
    {
        try
        {
            cmd_map->insert(command_entry.key, &command_entry);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            // Duplicate inserts ignored
        }
    }

    virtual bool complete_command(const std::string& prefix, std::string& completion)
    {
        bool unique = false;
        const std::string prefix_upper = string_transformations::uppercase_copy_of(prefix);
        typename CommandMap::Node* cmd_node = cmd_map->get_ceiling_node(&prefix_upper);
        if (cmd_node != nullptr)
        {
            const std::string* cmd_key_ptr = cmd_node->get_key();
            if (prefix_upper == *cmd_key_ptr)
            {
                unique = true;
                completion = *cmd_key_ptr;
            }
            else
            {
                if (cmd_key_ptr->find(prefix_upper) == 0)
                {
                    size_t completion_count = 1;

                    std::string common_completion = *cmd_key_ptr;
                    size_t common_length = common_completion.length();
                    const char* const common_chars = common_completion.c_str();

                    const size_t prefix_length = prefix_upper.length();

                    typename CommandMap::KeysIterator cmd_key_iter(*cmd_map, *cmd_node);
                    // Skip the start node
                    cmd_key_iter.next();
                    while (cmd_key_iter.has_next() && common_length > prefix_length)
                    {
                        cmd_key_ptr = cmd_key_iter.next();

                        if (cmd_key_ptr->find(prefix_upper) == 0)
                        {
                            ++completion_count;

                            const char* const cmd_key_chars = cmd_key_ptr->c_str();
                            size_t compare_idx = prefix_length;
                            while (compare_idx < common_length)
                            {
                                if (cmd_key_chars[compare_idx] != common_chars[compare_idx])
                                {
                                    break;
                                }
                                ++compare_idx;
                            }
                            if (compare_idx < common_length)
                            {
                                common_length = compare_idx;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }

                    completion = common_completion.substr(0, common_length);
                    unique = completion_count == 1;
                }
                else
                {
                    // No match
                    completion = "";
                }
            }
        }
        else
        {
            completion = "";
        }
        return unique;
    }
};

#endif /* TERMINAL_COMMANDSBASE_H_ */
