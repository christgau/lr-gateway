#ifndef __ORIS_LIB_EVENT_H
#define __ORIS_LIB_EVENT_H

/* libevent related global application stuff */
typedef struct oris_libevent_base_info {
	struct event_base *base;
	struct evdns_base *dns_base;
} oris_libevent_base_info_t;

#endif /* __ORIS_LIB_EVENT_H */
