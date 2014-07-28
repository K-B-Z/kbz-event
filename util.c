
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "util.h"

static const char *ban[128];
static int ban_nr;
static int ban_all = 0;

void log_ban(const char *file, const char *func) {
	ban[ban_nr*2] = file;
	ban[ban_nr*2+1] = func;
	ban_nr++;
}

static int log_level = LOG_WARN;

void log_set_level(int level) {
	log_level = level;
}

void _log(
	int level,
	const char *func, const char *file, int line,
	char *fmt, ...
) {
	va_list ap;
	char buf[1024];

	if (level < log_level)
		return;

	if (ban_all)
		return;

	int i;
	for (i = 0; i < ban_nr; i++) {
		if (!strcmp(ban[i*2], file)) {
			if (ban[i*2+1] == NULL)
				return;
		 	if (!strcmp(ban[i*2+1], func))
				return;
		}
	}

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	char ds;
	switch (level) {
	case LOG_DEBUG: ds = 'D'; break;
	case LOG_INFO:  ds = 'I'; break;
	case LOG_WARN:  ds = 'W'; break;
	case LOG_ERROR: ds = 'E'; break;
	default: ds = 'U'; break;
	}
	fprintf(stderr, "[%8.3f] %c [%s:%d:%s] %s\n", now(), ds, file, line, func, buf);
}

void log_init() {
	if (getenv("LOG") && !strcmp(getenv("LOG"), "0"))
		ban_all = 1;
}

float now() {
	struct timeval tv;
	static time_t sec_start;

	gettimeofday(&tv, NULL);
	if (!sec_start)
		sec_start = tv.tv_sec;

	return (tv.tv_sec - sec_start) + tv.tv_usec / 1e6;
}

