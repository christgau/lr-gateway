#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/err.h>

#include "oris_log.h"

static FILE* logFile = NULL;
static int logLevel;

void oris_init_log(const char* logfilename, int desiredLogLevel)
{
	if (logfilename) {
#ifndef WIN32
		logFile = fopen(logfilename, "a");
#else
		fopen_s(&logFile, logfilename, "a");
#endif
	} else {
		logFile = stderr;
	}

	logLevel = desiredLogLevel;
}

static void oris_log_time(void)
{
	time_t rawtime;
	struct tm *i;

	time(&rawtime);
#ifndef WIN32
	i = localtime(&rawtime);
#else
	localtime_s(i, &rawtime);
#endif
	fprintf(logFile, "%4d-%02d-%02d %02d:%02d:%02d: ", 1900 + i->tm_year, i->tm_mon + 1, i->tm_mday, i->tm_hour, i->tm_min, i->tm_sec);
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
		fputs(s, logFile);
	}
}


void oris_log_ssl_error(int severity)
{
	if (logFile && severity <= logLevel) {
		oris_log_time();
		ERR_print_errors_fp(logFile);
	}
}


void oris_finalize_log(void)
{
	if (logFile && logFile != stderr) {
		fclose(logFile);
	}
}
