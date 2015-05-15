#include <stdio.h>
#include <errno.h>

#include "oris_libevent.h"
#include <antlr3treeparser.h>
#include <event2/http.h>

#include "oris_connection.h"
#include "oris_configuration.h"
#include "oris_log.h"

#include "grammars/configLexer.h"
#include "grammars/configParser.h"
#include "grammars/configTree.h"

#ifdef _WIN32
#include <stdlib.h>
#include <string.h>
#define PATH_DELIMITER '\\'
#define CONFIG_SUBPATH ""
#define snprintf _snprintf
#else
#define PATH_DELIMITER '/'
#define CONFIG_SUBPATH "/etc"
#endif /* _WINDOWS */

const char* ORIS_GW_CFG_DEFAULT_FILENAME = "automation.script";

#ifndef MAX_PATH
#define MAX_PATH 256
#endif /* MAX_PATH */

typedef struct {
	oris_automation_event_t event;
	pANTLR3_BASE_TREE tree;
	pANTLR3_COMMON_TREE_NODE_STREAM node_stream;
} oris_automation_action_t;

void oris_get_config_filename(char* buffer, size_t bufsize)
{
#ifdef _WIN32
	char *s;
	if (_get_pgmptr(&s) == 0) {
		strncpy(buffer, s, bufsize);
		s = strrchr(buffer, PATH_DELIMITER);
		if (s) {
			*s = '\0';
		}
	} else {
		*buffer = '\0';
	}
#else
	*buffer = '\0';
	strncpy(buffer, getenv("HOME"), bufsize);
#endif
	bufsize -= strlen(buffer);
	snprintf(buffer + strlen(buffer), bufsize, "%s%c%s", CONFIG_SUBPATH, PATH_DELIMITER, ORIS_GW_CFG_DEFAULT_FILENAME);
}

static pANTLR3_BASE_TREE get_subtree_by_type(pANTLR3_BASE_TREE tree, ANTLR3_UINT32 type);
static pANTLR3_LIST get_nodes_of_type(pANTLR3_BASE_TREE tree, ANTLR3_UINT32 type, pANTLR3_LIST result);
static void collect_automation_nodes(pANTLR3_BASE_TREE root_tree);

static size_t automation_events_count;
static oris_automation_action_t* automation_events;

static struct {
	pANTLR3_INPUT_STREAM input;
	pconfigLexer lexer;
	pconfigParser parser;
	pANTLR3_COMMON_TOKEN_STREAM token_stream;
	pANTLR3_COMMON_TREE_NODE_STREAM node_stream;
	configParser_configuration_return parseTree;
	pANTLR3_BASE_TREE automationTree;
    pANTLR3_BASE_TREE request_defs_node;
    pANTLR3_LIST templates;
} parsing_state;

bool oris_load_configuration(oris_application_info_t* info)
{
	char fname[MAX_PATH];
	pANTLR3_BASE_TREE configTree, tree;
	pconfigTree walker;

	memset(fname, 0, sizeof(fname));
	oris_get_config_filename(fname, MAX_PATH - 1);
	oris_log_f(LOG_DEBUG, "loading configuration from: %s.",
		(info->config_fn ? info->config_fn : fname));

	parsing_state.input = antlr3FileStreamNew((pANTLR3_UINT8)
		(info->config_fn ? info->config_fn : (char*) &fname), ANTLR3_ENC_UTF8);
	if (parsing_state.input == NULL) {
		return false;
	}

	parsing_state.lexer = configLexerNew(parsing_state.input);
	if (parsing_state.lexer == NULL) {
		parsing_state.input->close(parsing_state.input);
		return false;
	}

	parsing_state.token_stream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(parsing_state.lexer));
	if (parsing_state.token_stream == NULL) {
		parsing_state.input->close(parsing_state.input);
		parsing_state.lexer->free(parsing_state.lexer);
		return false;
	}

	parsing_state.parser = configParserNew(parsing_state.token_stream);
	if (parsing_state.parser == NULL) {
		parsing_state.input->close(parsing_state.input);
		parsing_state.lexer->free(parsing_state.lexer);
		parsing_state.token_stream->free(parsing_state.token_stream);
		return false;
	}

	parsing_state.parseTree = parsing_state.parser->configuration(parsing_state.parser);
    if (parsing_state.parser->pParser->rec->state->errorCount > 0) {
		return false;
	}

	configTree = get_subtree_by_type(parsing_state.parseTree.tree, CONFIG);
    parsing_state.request_defs_node = get_subtree_by_type(parsing_state.parseTree.tree, REQUESTS);
	parsing_state.automationTree = get_subtree_by_type(parsing_state.parseTree.tree, AUTOMATION);

    parsing_state.templates = antlr3ListNew(32); /* we expect no more than 32 templates */
    tree = get_subtree_by_type(parsing_state.parseTree.tree, TEMPLATES);
    if (tree) {
        parsing_state.templates = get_nodes_of_type(tree, TEMPLATE, parsing_state.templates);
    }

	/* create tree parser for configuration */
	parsing_state.node_stream = antlr3CommonTreeNodeStreamNewTree(configTree, ANTLR3_SIZE_HINT);
	walker = configTreeNew(parsing_state.node_stream);

	/* walk the configuration node (parses connections and targets) */
	walker->configuration(walker, info);
	walker->free(walker);

	/* recreate token stream for re-use on automation interpretation */
	parsing_state.node_stream->free(parsing_state.node_stream);
	parsing_state.node_stream = antlr3CommonTreeNodeStreamNewTree(parsing_state.automationTree, ANTLR3_SIZE_HINT);

	automation_events = NULL;
	collect_automation_nodes(parsing_state.automationTree);

	return true;
}

void oris_configuration_init(void)
{

}

void oris_configuration_finalize(void)
{
	size_t i;

	parsing_state.node_stream->free(parsing_state.node_stream);

	parsing_state.input->close(parsing_state.input);
	parsing_state.lexer->free(parsing_state.lexer);
	parsing_state.token_stream->free(parsing_state.token_stream); /* ! */
	parsing_state.parser->free(parsing_state.parser);
    parsing_state.templates->free(parsing_state.templates);

	if (automation_events) {
		for (i = 0; i < automation_events_count; i++) {
//			automation_events[i].tree->free(automation_events[i].tree);
			automation_events[i].node_stream->free(automation_events[i].node_stream);
		}
		free(automation_events);
	}
}

static pANTLR3_BASE_TREE get_subtree_by_type(pANTLR3_BASE_TREE tree, ANTLR3_UINT32 type)
{
	pANTLR3_BASE_TREE child;
	ANTLR3_UINT32 i, child_count = tree->getChildCount(tree);

	if (tree->getType(tree) == type) {
		return tree;
	}

	for (i = 0; i < child_count; i++) {
		child = tree->getChild(tree, i);
		if (child->getType(child) == type) {
			return child;
		}
	}

	for (i = 0; i < child_count; i++) {
		child = tree->getChild(tree, i);
		child = get_subtree_by_type(child, i);
		if (child) {
			return child;
		}
	}

	return NULL;
}

static pANTLR3_LIST get_nodes_of_type(pANTLR3_BASE_TREE tree, ANTLR3_UINT32 type, pANTLR3_LIST result)
{
	pANTLR3_BASE_TREE child;
	ANTLR3_UINT32 i, child_count = tree->getChildCount(tree);
    bool found = false;

    for (i = 0; i < child_count; i++) {
		child = tree->getChild(tree, i);
        if (child->getType(child) == type) {
            found = true;
            result->add(result, child, NULL);
        }

        if (!found) {
            /* dive deeper */
            get_nodes_of_type(child, type, result);
        }
    }

    return result;
}

pANTLR3_BASE_TREE oris_get_request_parse_tree(const char *name)
{
    ANTLR3_UINT32 i, child_count;
    pANTLR3_BASE_TREE child;
    pANTLR3_COMMON_TOKEN token;
    pANTLR3_STRING token_name;

	if (!parsing_state.request_defs_node) {
		oris_log_f(LOG_WARNING, "no requests defined");
		return NULL;
	}

    child_count = parsing_state.request_defs_node->getChildCount(parsing_state.request_defs_node);
    for (i = 0; i < child_count; i += 2) {
        child = parsing_state.request_defs_node->getChild(parsing_state.request_defs_node, i);
        token = child->getToken(child);
        token_name = token->getText(token);
        if (token_name->compare(token_name, name) == 0) {
            /* this is an expression node */
            child = parsing_state.request_defs_node->getChild(parsing_state.request_defs_node, i + 1);

            return child;
        }
    }

    oris_log_f(LOG_WARNING, "request %s does not exist", name);
    return NULL;
}

pANTLR3_BASE_TREE oris_get_template_by_name(pANTLR3_STRING name)
{
	ANTLR3_UINT32 i;
	pANTLR3_BASE_TREE tmpl, inode;
	pANTLR3_STRING iname;

	for (i = 1; i < parsing_state.templates->size(parsing_state.templates) + 1; i++) {
		tmpl = parsing_state.templates->get(parsing_state.templates, i);
		inode = tmpl->getChild(tmpl, 0);
		iname = inode->getText(inode);
		if (inode->getType(inode) == IDENTIFIER && iname->compareS(iname, name) == 0) {
			return tmpl;
		}
	}

    oris_log_f(LOG_WARNING, "template %s does not exist", name->chars);
	return NULL;
}

static oris_automation_event_t parse_automation_object_node(pANTLR3_BASE_TREE object)
{
	pANTLR3_COMMON_TREE_NODE_STREAM node_stream;
	pconfigTree walker;
	oris_automation_event_t retval;

	node_stream = antlr3CommonTreeNodeStreamNewTree(object, ANTLR3_SIZE_HINT);
	walker = configTreeNew(node_stream);

	retval = walker->object(walker);

	node_stream->free(node_stream);
	walker->free(walker);

	return retval;
}

static void collect_automation_nodes(pANTLR3_BASE_TREE root_tree)
{
    pANTLR3_BASE_TREE event, object;
	ANTLR3_UINT32 i;

    automation_events_count = root_tree->getChildCount(root_tree);
	automation_events = calloc(automation_events_count, sizeof(*automation_events));
    for (i = 0; i < automation_events_count; i++) {
		event = root_tree->getChild(root_tree, i);
		/* we rely on the parse tree to look like: (EVENT (object) OPERATIONS (...)) */
		if (event->getChildCount(event) == 0) continue;

		object = event->getChild(event, 0);
		automation_events[i].event = parse_automation_object_node(object);
		automation_events[i].tree = event->getChild(event, 1);
		automation_events[i].node_stream = antlr3CommonTreeNodeStreamNewTree(
			automation_events[i].tree, ANTLR3_SIZE_HINT);
	}
}

bool oris_get_automation_parse_tree(oris_automation_event_t ev,
	pANTLR3_BASE_TREE* tree, pANTLR3_COMMON_TREE_NODE_STREAM* stream)
{
	size_t i;

	for (i = 0; i < automation_events_count; i++) {
		if (oris_is_same_automation_event(&automation_events[i].event, &ev)) {
			*tree = automation_events[i].tree;
			*stream = automation_events[i].node_stream;
			return true;
		}
	}

	*stream = NULL;
	*tree = NULL;
	return false;
}

