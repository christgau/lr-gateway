#ifndef __ORIS_LOG_H
#define __ORIS_LOG_H

#include <stdarg.h>
#include <syslog.h>

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


/**
 * oris_finalize_log()
 */
void oris_finalize_log(void);


#endif /* __ORIS_LOG_H */
