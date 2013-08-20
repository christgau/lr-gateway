#ifndef __ORIS_LOG_H
#define __ORIS_LOG_H

#include <stdarg.h>

#ifndef _WIN32
#include <syslog.h>
#else
#define LOG_DEBUG 10
#define LOG_INFO 20
#define LOG_NOTICE 30
#define LOG_WARN 40
#define LOG_WARNING LOG_WARN
#define LOG_ERR 50
#define LOG_ALERT 70
#define LOG_CRIT 80
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
 * oris_finalize_log()
 */
void oris_finalize_log(void);


#endif /* __ORIS_LOG_H */
