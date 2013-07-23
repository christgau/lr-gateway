#ifndef __ORIS_AUTOMATION_TYPES_H
#define __ORIS_AUTOMATION_TYPES_H

typedef enum {EVT_CONNECTION, EVT_TABLE, EVT_COMMAND} oris_event_type_t;

typedef struct {
	oris_event_type_t type;
	char* name;
} oris_automation_event_t;

#endif /* __ORIS_AUTOMATION_TYPES_H */
