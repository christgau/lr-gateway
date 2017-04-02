#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <openssl/err.h>
#include <event2/util.h>

#include "oris_log.h"

static FILE* logFile = NULL;
static int logLevel;

void oris_init_log(const char* logfilename, int desiredLogLevel)
{
	if (logfilename) {
		if (logFile && logFile != stdout) {
			fclose(logFile);
		}
#ifndef _WIN32
		logFile = fopen(logfilename, "a");
#else
		fopen_s(&logFile, logfilename, "a");
#endif
	} else {
		logFile = stdout;
	}

	logLevel = desiredLogLevel;
}

static void get_localtime(struct tm* t, struct timeval *tv)
{
	time_t rawtime;

	evutil_gettimeofday(tv, NULL);
	rawtime = tv->tv_sec;
	time(&rawtime);
#ifndef _WIN32
	localtime_r(&rawtime, t);
#else
	localtime_s(t, &rawtime);
#endif
}

static void oris_log_time(void)
{
	struct tm t;
	struct timeval now;
	static struct timeval last_log = { 0, 0 };
	int64_t diff;

	get_localtime(&t, &now);
	fprintf(logFile, "%4d-%02d-%02d %02d:%02d:%02d",
		1900 + t.tm_year, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec);

	if (last_log.tv_sec != 0) {
		diff = (int64_t)
			(((now.tv_sec * 1E6 + now.tv_usec) -
			 (last_log.tv_sec * 1E6 + last_log.tv_usec)) / 1E3);
		fprintf(logFile, " [+%0ld.%03ld]: ", diff / 1000, diff % 1000);
	} else {
		fprintf(logFile, ": ");
	}

	last_log = now;
}

static void oris_log_severity(int severity)
{
	char* msg;

	switch (severity) {
	case LOG_DEBUG:
		msg = "DEBUG";
		break;
	case LOG_INFO:
		msg = "INFO";
		break;
	case LOG_WARNING:
		msg = "WARN";
		break;
	case LOG_NOTICE:
		msg = "NOTICE";
		break;
	case LOG_ERR:
		msg = "ERROR";
		break;
	case LOG_ALERT:
		msg = "ALERT";
		break;
	case LOG_CRIT:
		msg = "CRITICAL";
		break;
	default:
		msg = "";
	}

	fprintf(logFile, "%s ", msg);
}

void oris_log_f(int severity, char* fmt, ...)
{
	va_list arglist;

	if (logFile && severity <= logLevel) {
		oris_log_time();
		oris_log_severity(severity);
        	va_start(arglist, fmt);
		vfprintf(logFile, fmt, arglist);
		va_end(arglist);
		fprintf(logFile," \n");
	}
}

void oris_logs(int severity, const char* s)
{
	if (logFile && severity <= logLevel) {
		oris_log_time();
		oris_log_severity(severity);
		fputs(s, logFile);
	}
}


void oris_log_ssl_error(int severity)
{
	if (logFile && severity <= logLevel) {
		oris_log_time();
		oris_log_severity(severity);
		ERR_print_errors_fp(logFile);
	}
}

int oris_get_log_level(void)
{
	return logLevel;
}

void oris_set_log_level(int level)
{
	logLevel = level;
}

void oris_finalize_log(void)
{
	if (logFile && logFile != stderr) {
		fclose(logFile);
	}
}
