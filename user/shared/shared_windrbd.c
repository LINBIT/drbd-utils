#include <string.h>
#include <ctype.h>

int is_guid(const char *arg)
{
        int i;
#define GUID_MASK "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

        for (i=0;arg[i] != '\0';i++) {
                if (GUID_MASK[i] == 'x' && !isxdigit(arg[i]))
                        return 0;
                if (GUID_MASK[i] == '-' && arg[i] != '-')
                        return 0;
        }
        return i == strlen(GUID_MASK) && arg[i] == '\0';
#undef GUID_MASK
}


