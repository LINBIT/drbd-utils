#include <subprocess/CmdLine.h>

#include <new>
#include <comparators.h>

CmdLine::CmdLine()
{
    arg_list = std::unique_ptr<StringList>(new StringList(&comparators::compare_string));
}

CmdLine::CmdLine(const std::string& description):
    CmdLine::CmdLine()
{
    cmd_description = description;
}

CmdLine::~CmdLine() noexcept
{
    StringList::ValuesIterator arg_iter(*arg_list);
    while (arg_iter.has_next())
    {
        std::string* const cur_arg = arg_iter.next();
        delete cur_arg;
    }
    arg_list->clear();

    if (env_list != nullptr)
    {
        StringList::ValuesIterator env_iter(*env_list);
        while (env_iter.has_next())
        {
            std::string* const env_entry = env_iter.next();
            delete env_entry;
        }
        env_list->clear();
    }
}

size_t CmdLine::get_argument_count() const
{
    return arg_list->get_size();
}

CmdLine::StringList::ValuesIterator CmdLine::get_argument_iterator() const
{
    StringList::ValuesIterator arg_iter(*arg_list);
    return arg_iter;
}

size_t CmdLine::get_environment_entry_count() const
{
    return env_list == nullptr ? 0 : env_list->get_size();
}

CmdLine::StringList::ValuesIterator CmdLine::get_environment_entry_iterator() const
{
    if (env_list == nullptr)
    {
        env_list = std::unique_ptr<StringList>(new StringList(&comparators::compare_string));
    }
    StringList::ValuesIterator env_iter(*env_list);
    return env_iter;
}

const std::string& CmdLine::get_description() const
{
    return cmd_description;
}

void CmdLine::set_description(const std::string& description)
{
    cmd_description = description;
}

// @throws std::bad_alloc
void CmdLine::add_argument(const std::string& arg)
{
    std::unique_ptr<std::string> new_arg(new std::string(arg));
    arg_list->append(new_arg.get());
    new_arg.release();
}

// @throws std::bad_alloc
void CmdLine::add_environment_entry(const std::string& entry)
{
    if (env_list == nullptr)
    {
        env_list = std::unique_ptr<StringList>(new StringList(&comparators::compare_string));
    }
    std::unique_ptr<std::string> new_entry(new std::string(entry));
    env_list->append(new_entry.get());
    new_entry.release();
}
