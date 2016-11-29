#include <Args.h>

Args::Args(int argc, char* argv[]):
    arg_count(argc),
    arg_values(argv)
{
}

Args::~Args() noexcept
{
}

bool Args::has_next() const
{
    bool rc = index < arg_count;
    return rc;
}

char* Args::next()
{
    char* arg = nullptr;
    if (index < arg_count)
    {
        arg = arg_values[index];
        ++index;
    }
    return arg;
}
