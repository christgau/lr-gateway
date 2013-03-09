#ifndef __ORIS_PROTOCOL_CTRL_H
#define __ORIS_PROTOCOL_CTRL_H

#include <event2/bufferevent.h>

#include "oris_util.h"

typedef struct oris_ctrl_protocol_data {
	oris_application_info_t* info;
} oris_ctrl_protocol_data_t;

void oris_protocol_ctrl_read_cb(struct bufferevent *bev, void *ctx);

#endif /* __ORIS_PROTOCOL_CTRL_H */
