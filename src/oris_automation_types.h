#ifndef __ORIS_AUTOMATION_TYPES_H
#define __ORIS_AUTOMATION_TYPES_H

typedef enum {EVT_CONNECTION, EVT_TABLE, EVT_COMMAND} oris_event_type_t;

typedef struct {
	oris_event_type_t type;
	char* name;
} oris_automation_event_t;

bool oris_is_same_automation_event(oris_automation_event_t* a, oris_automation_event_t* b);

void oris_init_automation_event(oris_automation_event_t* event,
		oris_event_type_t type, const char* name);
void oris_free_automation_event(oris_automation_event_t* event);

#endif /* __ORIS_AUTOMATION_TYPES_H */
