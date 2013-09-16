#include <errno.h>

#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>

#include "oris_log.h"
#include "oris_util.h"
#include "oris_http.h"

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4706 )
#endif

static void http_request_done_cb(struct evhttp_request *req, void *ctx);
static void http_connection_close(struct evhttp_connection *con, void *ctx);
static const char* oris_get_http_method_string(const enum evhttp_cmd_type method);


/* taken from libevent https-client sample */
static void http_request_done_cb(struct evhttp_request *req, void *ctx)
{
	int status, nread;
	size_t length;
	struct evbuffer* response;
	oris_http_target_t* target = (oris_http_target_t*) ctx;
	char buffer[256];

	if (req == NULL) {
		bool printed_err = false;
		int errcode = EVUTIL_SOCKET_ERROR();
		unsigned long oslerr;

		while ((oslerr = bufferevent_get_openssl_error(target->bev))) {
			ERR_error_string_n(oslerr, buffer, sizeof(buffer));
			oris_log_f(LOG_ERR, "SSL error %s", buffer);
			printed_err = true;
		}

		/* no ssl error, so maybe socket error */
		if (!printed_err) {
			oris_log_f(LOG_ERR, "socket error: %s (%d)",
				evutil_socket_error_to_string(errcode),	errcode);
		}

		return;
	}

	response = evhttp_request_get_input_buffer(req);
	length = evbuffer_get_length(response);

	status = evhttp_request_get_response_code(req);
	oris_log_f(status / 100 != 2 ? LOG_ERR : LOG_INFO,
		"http response %d %s (%d bytes body)",
		evhttp_request_get_response_code(req),
		evhttp_request_get_response_code_line(req),
		length);

	if (oris_get_log_level() == LOG_DEBUG) {
		while ((nread = evbuffer_remove(response, buffer, sizeof(buffer))) > 0) {
			fwrite(buffer, nread, 1, stderr);
		}
	} else {
		evbuffer_drain(response, length);
	}
}

static void http_connection_close(struct evhttp_connection *con, void *ctx)
{
	oris_log_f(LOG_INFO, "http on %s connection closed", ((oris_http_target_t*) ctx)->name);

	con = con; /* keep compiler happy */
}

bool oris_str_to_http_method(const char* str, enum evhttp_cmd_type* method)
{
	if (strcasecmp(str, "GET") == 0) {
		*method = EVHTTP_REQ_GET;
	} else if (strcasecmp(str, "POST") == 0) {
		*method = EVHTTP_REQ_POST;
	} else if (strcasecmp(str, "PUT") == 0) {
		*method = EVHTTP_REQ_PUT;
	} else if (strcasecmp(str, "DELETE") == 0) {
		*method = EVHTTP_REQ_DELETE;
	} else {
		return false;
	}

	return true;
}

static const char* oris_get_http_method_string(const enum evhttp_cmd_type method)
{
	switch (method) {
		case EVHTTP_REQ_GET:
			return "GET";
		case EVHTTP_REQ_POST:
			return "POST";
		case EVHTTP_REQ_PUT:
			return "PUT";
		case EVHTTP_REQ_DELETE:
			return "DELETE";
		default:
			return "?";
	}
}

#define MAX_URL_SIZE 256

void oris_perform_http_on_targets(oris_http_target_t* targets, int target_count,
	const enum evhttp_cmd_type method, const char* uri, struct evbuffer* body)
{
	char url_buf[MAX_URL_SIZE] = { 0 };
	struct evhttp_request *request;
	struct evkeyvalq *output_headers;
	struct evbuffer* output_buffer;
	int i;

	oris_log_f(LOG_INFO, "http %s %s (%lu bytes body) ", oris_get_http_method_string(method),
			uri, evbuffer_get_length(body));

	for (i = 0; i < target_count; i++) {
		request = evhttp_request_new(http_request_done_cb, &targets[i]);
		if (request) {
			/* todo: place this at a better position */
			evhttp_connection_set_closecb(targets[i].connection, http_connection_close, targets + i);

			output_headers = evhttp_request_get_output_headers(request);
			evhttp_add_header(output_headers, "Host", evhttp_uri_get_host(targets[i].uri));
			evhttp_add_header(output_headers, "User-Agent", ORIS_USER_AGENT);
			evhttp_add_header(output_headers, "Accept", "application/json, text/plain");
			evhttp_add_header(output_headers, "Accept-Charset", "utf-8");
			if (evbuffer_get_length(body) > 0) {
				evhttp_add_header(output_headers, "Content-Type", "application/json");
			}
			evutil_snprintf(url_buf, MAX_URL_SIZE - 1, "%s%s", evhttp_uri_get_path(
					targets[i].uri), uri);

			if (method == EVHTTP_REQ_PUT || method == EVHTTP_REQ_POST) {
				output_buffer = evhttp_request_get_output_buffer(request);
				evbuffer_add_buffer_reference(output_buffer, body);
			}

			oris_log_f(LOG_DEBUG, "sending request %s to '%s'", url_buf, targets[i].name);
			if (evhttp_make_request(targets[i].connection, request, method, url_buf) != 0) {
				oris_log_f(LOG_ERR, "error making http request");
			}
		} else {
			oris_log_f(LOG_ERR, "could not send request %s to target %s. target's state may be undefined!", uri,
					targets[i].name);
		}
	}
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
