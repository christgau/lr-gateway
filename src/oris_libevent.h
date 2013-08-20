#ifndef __ORIS_LIB_EVENT_H
#define __ORIS_LIB_EVENT_H

#include <event2/event-config.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef EVENT__HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef EVENT__HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <event.h>
#include <event2/event.h>
#include <event2/dns.h>

/* libevent related global application stuff */
typedef struct oris_libevent_base_info {
	struct event_base *base;
	struct evdns_base *dns_base;
} oris_libevent_base_info_t;

#endif /* __ORIS_LIB_EVENT_H */
