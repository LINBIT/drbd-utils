#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int assign_drive_letter(int minor, const char *drive)
{
	printf("Assigning drive letter %s to minor %d\n", drive, minor);

	return 0;
}

void usage_and_exit(void)
{
	fprintf(stderr, "Usage: windrbd assign-drive-letter <minor> <drive-letter>\n");
	exit(1);
}

int main(int argc, char ** argv)
{
	const char *op;

	if (argc < 2) {
		usage_and_exit();
	}
	op = argv[1];
	if (strcmp(op, "assign-drive-letter") == 0) {
		if (argc != 4) {
			usage_and_exit();
		}
		int minor = atoi(argv[2]);
		const char *drive = argv[3];

		return assign_drive_letter(minor, drive);
	}

	usage_and_exit();
	return 0;
}
