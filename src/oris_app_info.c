#include <stdlib.h>
#include <string.h>

#ifndef _WINDOWS
#include <signal.h>
#else
#include <unistd.h>
#endif

#include <event2/event.h>
#include <event2/dns.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/sha.h>

#include "oris_log.h"
#include "oris_util.h"
#include "oris_app_info.h"
#include "oris_socket_connection.h"

#ifndef _WIN32
/* list from https://golang.org/src/crypto/x509/root_linux.go + FreeBSD location*/
static const char* cert_locations[] = {
	"/etc/ssl/certs/ca-certificates.crt",
	"/etc/ssl/ca-bundle.pem",
	"/etc/ssl/cert.pem",
	"/etc/pki/tls/certs/ca-bundle.crt",
	"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
	"/etc/pki/tls/cacert.pem",
};
#elif defined(_WIN32)
/* noop */
#else
#error Unsupporting operating system
#endif

#ifdef _WIN32
#define snprintf _snprintf
#endif

/* forwards */
static void oris_targets_clear(oris_http_target_t* targets, int *count);
static void oris_set_http_target_auth_header(oris_http_target_t* target);

/* definitons */
static void oris_libevent_log_cb(int severity, const char* msg)
{
	int sev;

	switch (severity) {
		case _EVENT_LOG_DEBUG:
			sev = LOG_DEBUG;
			break;
		case _EVENT_LOG_MSG:
			sev = LOG_INFO;
			break;
		case _EVENT_LOG_WARN:
			sev = LOG_WARNING;
			break;
		case _EVENT_LOG_ERR:
			sev = LOG_ERR;
			break;
		default:
			sev = LOG_WARNING;
	}

	oris_log_f(sev, "[libevent] %s", msg);
}

static void oris_sigint_cb(evutil_socket_t fd, short type, void *arg)
{
	struct event_base *base = arg;

	oris_logs(LOG_INFO, "Received SIGINT. Cleaning up and exiting.\n");
	fflush(stdout);

	event_base_loopbreak(base);

	/* keep compiler happy */
	(void) fd;
	(void) type;
}

static bool oris_init_libevent(struct oris_application_info *info)
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	(void) WSAStartup(wVersionRequested, &wsaData);
#endif
	info->libevent_info.base = event_base_new();
	info->libevent_info.dns_base = evdns_base_new(info->libevent_info.base, true);

	if (info->libevent_info.base == NULL || info->libevent_info.dns_base == NULL) {
		oris_log_f(LOG_CRIT, "could not init libevent");
		return false;
	} else {
		info->sigint_event = evsignal_new(info->libevent_info.base, SIGINT, oris_sigint_cb,
			info->libevent_info.base);
		event_set_log_callback(oris_libevent_log_cb);
		event_add(info->sigint_event, NULL);

		oris_log_f(LOG_DEBUG, "using libevent %s", event_get_version());

		return true;
	}
}

static bool oris_init_ssl(struct oris_application_info* info)
{
#if (OPENSSL_VERSION_NUMBER >= 0x101000000L)
	OPENSSL_init_ssl()
#else
	SSL_load_error_strings();
	SSL_library_init();
#endif

	OpenSSL_add_all_algorithms();

#if (OPENSSL_VERSION_NUMBER >=  0x10100000L)
	info->ssl_ctx = SSL_CTX_new(TLS_method());
#else
	info->ssl_ctx = SSL_CTX_new(TLSv1_2_method());
#endif

	if (!info->ssl_ctx) {
		oris_log_f(LOG_CRIT, "could not create SSL context");
		oris_log_ssl_error(LOG_CRIT);

		return false;
	}

	SSL_CTX_set_options(info->ssl_ctx, SSL_OP_NO_TLSv1);
	SSL_CTX_set_options(info->ssl_ctx, SSL_OP_NO_TLSv1_1);

#ifndef _WIN32
	bool ssl_loaded = false;

	for (size_t i = 0; i < sizeof(cert_locations)/sizeof(*cert_locations); i++) {
		oris_log_f(LOG_DEBUG, "trying to load SSL certificates from %s", cert_locations[i]);
		ssl_loaded = SSL_CTX_load_verify_locations(info->ssl_ctx, cert_locations[i], NULL) == 1;
		if (!ssl_loaded) {
			oris_log_f(LOG_DEBUG, "could not load certificates");
			oris_log_ssl_error(LOG_DEBUG);
		} else {
			break;
		}
	}

	if (!ssl_loaded) {
		return false;
	}
#endif

	if (info->cert_fn != NULL) {
		if (SSL_CTX_use_certificate_file(info->ssl_ctx,
			info->cert_fn, SSL_FILETYPE_PEM) == 1) {
			oris_log_ssl_error(LOG_CRIT);
		}
	}
	/* no certificate verification ATM */
/*	SSL_CTX_set_verify(info->ssl_ctx, SSL_VERIFY_PEER, NULL);*/
/*	FIXME: SSL_CTX_set_cert_verify_callback(info->ssl_ctx, */

	return true;
}

bool oris_app_info_init(oris_application_info_t* info)
{
	info->targets.items = NULL;
	info->targets.count = 0;

	return oris_init_libevent(info) && oris_init_ssl(info);
}

static void oris_finalize_ssl(oris_application_info_t* info)
{
	SSL_CTX_free(info->ssl_ctx);

#if (OPENSSL_VERSION_NUMBER <=  0x10100000L)
	ERR_remove_state(0);
#endif
	ENGINE_cleanup();
	CONF_modules_unload(1);

	CONF_modules_free();
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
}

void oris_app_info_finalize(oris_application_info_t* info)
{
	oris_tables_finalize(&info->data_tables);
	oris_targets_clear(info->targets.items, &info->targets.count);

	free(info->targets.items);
	info->targets.items = NULL;

	oris_free_connections(&info->connections);

	event_free(info->sigint_event);
	evdns_base_free(info->libevent_info.dns_base, 0);
	event_base_free(info->libevent_info.base);

	oris_free_and_null(info->cert_fn);

	oris_finalize_ssl(info);
}

static void oris_set_http_target_auth_header(oris_http_target_t* target)
{
	const char* userinfo = evhttp_uri_get_userinfo(target->uri);
	BIO *bio, *base64;
	BUF_MEM *buffer;
	char* auth_value;

	if (userinfo == NULL) {
		return;
	}

	base64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	bio = BIO_push(base64, bio);

	BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
	BIO_write(bio, userinfo, strlen(userinfo));
	BIO_flush(bio);
	BIO_get_mem_ptr(bio, &buffer);

	target->auth_header_value = (char*) calloc(6 + 2 +
		buffer->length, sizeof(*auth_value));

	evutil_snprintf(target->auth_header_value,
		6 + 1 + buffer->length, "Basic %s", buffer->data); //buffer->data);

	BIO_free_all(bio);
}

void oris_config_add_target(oris_application_info_t* config, const char* name, const char* uri)
{
	oris_http_target_t* items;
	oris_http_target_t* target;
	struct evhttp_uri *evuri;
	int port;
	bool use_ssl;

	evuri = evhttp_uri_parse(uri);
	if (evuri) {
		items = realloc(config->targets.items, (config->targets.count + 1) * sizeof(*items));

		if (items) {
			use_ssl = strcasecmp(evhttp_uri_get_scheme(evuri), "https") == 0;
			port = evhttp_uri_get_port(evuri);
			if (port == -1) {
				evhttp_uri_set_port(evuri, use_ssl ? 443 : 80);
			}

			config->targets.items = items;
			target = config->targets.items + config->targets.count;
			target->name = strdup(name);
			target->uri = evuri;
			target->enabled = true;
			target->compress = config->compress_http;
			target->auth_header_value = NULL;

			oris_set_http_target_auth_header(target);

			/* TODO: plain http (no ssl) works fine, but when the https (!)
			 * connection is closed by the server side we end up in "bad file descriptor"
			 * messages.... so https is not working properbly ATM :-(
			 * BUT: maybe http is also affected, as we close the buffereevent_Sockets when
			 * a close comes in! */
			if (!use_ssl) {
				target->ssl = NULL;
				target->bev = bufferevent_socket_new(config->libevent_info.base,
					-1, BEV_OPT_CLOSE_ON_FREE);
			} else {
				target->ssl = SSL_new(config->ssl_ctx);
				target->bev = bufferevent_openssl_socket_new(
					config->libevent_info.base, -1, target->ssl,
					BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE
					| BEV_OPT_DEFER_CALLBACKS);
				if (!target->bev) {
					oris_log_f(LOG_ERR, "could not create SSL socket for %s", name);
				}
				bufferevent_openssl_set_allow_dirty_shutdown(target->bev, 1);
			}

			target->libevent_info = &config->libevent_info;
			target->connection = evhttp_connection_base_bufferevent_new(
				config->libevent_info.base, config->libevent_info.dns_base,
				target->bev, evhttp_uri_get_host(evuri),
				(unsigned short) evhttp_uri_get_port(evuri));

			config->targets.count++;
		}
		oris_log_f(LOG_DEBUG, "new target %s: %s", name, uri);
	} else {
		oris_log_f(LOG_WARNING, "invalid URI for target %s: %s. Skipping.", name, uri);
	}
}

void oris_targets_clear(oris_http_target_t* targets, int *count)
{
	int i;

	for (i = 0; i < *count; i++) {
		if (targets[i].uri) {
			evhttp_uri_free(targets[i].uri);
			targets[i].uri = NULL;
		}

		if (targets[i].name) {
			free(targets[i].name);
			targets[i].name = NULL;
		}

		if (targets[i].connection) {
			evhttp_connection_free(targets[i].connection);
			targets[i].connection = NULL;
		}

		oris_free_and_null(targets[i].auth_header_value);
	}

	*count = 0;
}

void oris_create_connection(oris_application_info_t* info, const char* name, oris_parse_expr_t* e)
{
	char* v_str = oris_expr_as_string(e);
	struct evhttp_uri *uri = evhttp_uri_parse((const char*) v_str);

	if (uri) {
		oris_connection_t* c = oris_create_connection_from_uri(uri, name, (void*) info);
		if (c) {
			oris_log_f(LOG_DEBUG, "new connection %s -> %s", name, v_str);
			oris_connections_add(&info->connections, c);
			/* uri is freed by connection */
		} else {
			evhttp_uri_free(uri);
		}
	} else {
		oris_log_f(LOG_ERR, "invalid uri for connection %s: %s", name, v_str);
	}

	free(v_str);
}
