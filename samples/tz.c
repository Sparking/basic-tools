#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tzset.h"

int settimezone(const char *tzstring)
{
	int ret;
	const char *e;
	char buf[256];

	if ((ret = parse_posix_tzstring(tzstring, "./tz", buf, sizeof(buf))) != 0) {
		e = parse_posix_tzstring_err2str(ret);
		printf("error: %s\n", e);
		return -1;
	}
	printf("timezone has been set as: %s\n", buf);

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc <= 1) {
		return -1;
	}

	return settimezone(argv[1]);
}
