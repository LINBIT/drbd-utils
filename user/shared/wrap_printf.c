#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

__attribute__((format(printf, 2, 0)))
int wrap_vprintf(int indent, const char *format, va_list ap1)
{
	static int columns, col;
	va_list ap2;
	int n;
	const char *nl;

	if (columns == 0) {
		struct winsize ws = { };

		ioctl(1, TIOCGWINSZ, &ws);
		columns = ws.ws_col;
		if (columns <= 0)
			columns = 80;
	}

	va_copy(ap2, ap1);
	n = vsnprintf(NULL, 0, format, ap1);
	if (col + n > columns) {
		putchar('\n');
		if (*format == '\n')
			format++;
		col = 0;
	}
	if (col == 0) {
		while (*format == ' ')
			format++;
		col += indent;
		while (indent--)
			putchar(' ');
	}
	n = vprintf(format, ap2);
	va_end(ap2);
	if (n > 0)
		col += n;

	nl = strrchr(format, '\n');
	if (nl && nl[1] == 0)
		col = 0;

	return n;
}

__attribute__((format(printf, 2, 3)))
int wrap_printf(int indent, const char *format, ...)
{
    int rc;
    va_list ap;
    va_start(ap, format);
    rc = wrap_vprintf(indent, format, ap);
    va_end(ap);
    return rc;
}

int wrap_printf_wordwise(int indent, const char *str)
{
	int n = 0;

	do {
		const char *fmt = "%.*s", *s;
		int m;

		if (*str == ' ') {
			fmt = " %.*s";
			while (*str == ' ')
				str++;
		}
		for (s = str; *s && *s != ' '; s++)
			/* nothing */ ;
		m = wrap_printf(indent, fmt, s - str, str);
		if (m < 0)
			return m;
		n += m;
		str = s;
	} while (*str);

	return n;
}
