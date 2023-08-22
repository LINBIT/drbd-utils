#ifndef GLOBALCOMMANDS_H
#define GLOBALCOMMANDS_H

#include <default_types.h>
#include <StringTokenizer.h>
#include <string>

class GlobalCommands
{
  public:
    virtual ~GlobalCommands() noexcept
    {
    }

    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer) = 0;
    virtual bool complete_command(const std::string& prefix, std::string& completion) = 0;
};

#endif /* GLOBALCOMMANDS_H */
