#include <stdarg.h>
#include <stdio.h>
#include "shared_main.h"

int call_windrbd(char *res_name, char *path, ...)
{
        char *argv[40];
	int argc = 0;
	va_list ap;
	char *arg;

	argv[argc++] = path;
	va_start(ap, path);
	while (argc < sizeof(argv)/sizeof(*argv)-1 && 
	       (arg = va_arg(ap, char*)) != NULL)
		argv[argc++] = arg;

	if (argc >= sizeof(argv)/sizeof(*argv)-1) {
		printf("Warning: argument list too long (%d elements)\n", argc);
		va_end(ap);
		return E_THINKO;
	}

	argv[argc++] = NULL;
	va_end(ap);

        return m_system_ex(argv, SLEEPS_SHORT, res_name);
}
