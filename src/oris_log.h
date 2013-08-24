#ifndef __ORIS_LOG_H
#define __ORIS_LOG_H

#include <stdarg.h>

#ifndef _WIN32
#include <syslog.h>
#else
#define LOG_DEBUG 7
#define LOG_INFO 6
#define LOG_NOTICE 5
#define LOG_WARN 4
#define LOG_WARNING LOG_WARN
#define LOG_ERR 3
#define LOG_CRIT 2
#define LOG_ALERT 1
#endif

/**
 * oris_init_log
 *
 * initiate the loggign infrastructure
 */ 
void oris_init_log(const char* logfilename, int desiredLogLevel);

/**
 * oris_log
 * 
 * log a message with given severity (either a plain string for printf stuff)
 */
void oris_log_f(int severity, char* fmt, ...);
void oris_logs(int severity, const char* s);

void oris_log_ssl_error(int severity);

/**
 * get/set log level
 */
int oris_get_log_level(void);
void oris_set_log_level(int level);

/**
 * oris_finalize_log()
 */
void oris_finalize_log(void);


#endif /* __ORIS_LOG_H */
