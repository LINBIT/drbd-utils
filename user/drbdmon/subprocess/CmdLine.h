#ifndef CMDLINE_H
#define CMDLINE_H

#include <default_types.h>
#include <memory>
#include <string>
#include <VList.h>

class CmdLine
{
  public:
    using StringList = VList<std::string>;

    CmdLine();
    CmdLine(const std::string& description);
    virtual ~CmdLine() noexcept;

    virtual size_t get_argument_count() const;
    virtual StringList::ValuesIterator get_argument_iterator() const;

    virtual const std::string& get_description() const;

    // @throws std::bad_alloc
    virtual void set_description(const std::string& description);

    // @throws std::bad_alloc
    virtual void add_argument(const std::string& arg);

  private:
    std::unique_ptr<StringList> arg_list;
    std::string                 cmd_description;
};

#endif /* CMDLINE_H */
