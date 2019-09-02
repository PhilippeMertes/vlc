/*****************************************************************************
 * tls.c
 *****************************************************************************
 * Copyright © 2004-2016 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * @ingroup tls
 * @file
 * Transport Layer Session protocol API.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL
# include <poll.h>
#endif
#include <assert.h>
#include <errno.h>
#include <time.h>
#ifndef SOL_TCP
# define SOL_TCP IPPROTO_TCP
#endif
#include <libpvd.h>
#include <regex.h>

#include <vlc_common.h>
#include "libvlc.h"

#include <vlc_tls.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>
#include <vlc_network.h>
#include <vlc_fs.h>

/*** TLS credentials ***/

static int tls_server_load(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_tls_server_t *, const char *, const char *) = func;
    vlc_tls_server_t *crd = va_arg(ap, vlc_tls_server_t *);
    const char *cert = va_arg (ap, const char *);
    const char *key = va_arg (ap, const char *);

    int ret = activate (crd, cert, key);
    if (ret)
        vlc_objres_clear(VLC_OBJECT(crd));
    (void) forced;
    return ret;
}

static int tls_client_load(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_tls_client_t *) = func;
    vlc_tls_client_t *crd = va_arg(ap, vlc_tls_client_t *);

    int ret = activate (crd);
    if (ret)
        vlc_objres_clear(VLC_OBJECT(crd));
    (void) forced;
    return ret;
}

vlc_tls_server_t *
vlc_tls_ServerCreate (vlc_object_t *obj, const char *cert_path,
                      const char *key_path)
{
    vlc_tls_server_t *srv = vlc_custom_create(obj, sizeof (*srv),
                                              "tls server");
    if (unlikely(srv == NULL))
        return NULL;

    if (key_path == NULL)
        key_path = cert_path;

    if (vlc_module_load(srv, "tls server", NULL, false,
                        tls_server_load, srv, cert_path, key_path) == NULL)
    {
        msg_Err (srv, "TLS server plugin not available");
        vlc_object_delete(srv);
        return NULL;
    }

    return srv;
}

void vlc_tls_ServerDelete(vlc_tls_server_t *crd)
{
    if (crd == NULL)
        return;

    crd->ops->destroy(crd);
    vlc_objres_clear(VLC_OBJECT(crd));
    vlc_object_delete(crd);
}

int vlc_tls_pvd_parse_list(vlc_tls_client_t *crd, vlc_array_t *pvds,
                          regex_t re_pvd, char *pvd_list)
{
    regmatch_t rm[2];
    int ret = regexec(&re_pvd, pvd_list, 2, &rm, 0);
    char matchstr[128];
    char errbuf[128];
    int start;
    int end;

    while (!ret) {
        start = (int) rm[1].rm_so;
        if (start == -1) {
            msg_Err(crd, "unable to parse PvDs from config file");
            return VLC_EGENERIC;
        }
        end = (int) rm[1].rm_eo;

        memcpy(matchstr, pvd_list+start, end-start);
        // add dot at the end if not specified
        if (matchstr[end-start-1] != '.') {
            matchstr[end-start] = '.';
            matchstr[end-start+1] = '\0';
        }
        else
            matchstr[end-start] = '\0';
        vlc_array_append(pvds, strdup(matchstr));

        // strip matched bit and try finding next matches
        strcpy(pvd_list, &pvd_list[end]);
        ret = regexec(&re_pvd, pvd_list, 2, &rm, 0);
    }

    if (ret != REG_NOMATCH) {
        regerror(ret, &re_pvd, errbuf, sizeof(errbuf));
        msg_Err(crd, "regexec error while parsing PvDs: %s", errbuf);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


int vlc_tls_pvd_parse_line(vlc_tls_client_t *crd, vlc_dictionary_t *url_pvds,
                          regex_t re_line, regex_t re_pvd, const char* line)
{
    regmatch_t rm[3];
    int ret = regexec(&re_line, line, 3, &rm, 0);
    char matchstr[1024];
    char errbuf[128];
    int start;
    int end;
    if (!ret) {
        // parse URL
        start = (int) rm[1].rm_so;
        if (start == -1) {
            msg_Err(crd, "Wrong syntax in PvD config file.\n"
                         "Usage: \"url\": [\"pvd1\", \"pvd2\", ...]");
            return VLC_EGENERIC;
        }
        end = (int) rm[1].rm_eo;
        memcpy(matchstr, line+start, end-start);
        matchstr[end-start] = '\0';
        char *url = strdup(matchstr);

        // retrieve PvDs
        start = (int) rm[2].rm_so;
        if (start == -1) {
            msg_Err(crd, "Wrong syntax in PvD config file.\n"
                         "Usage: \"url\": [\"pvd1\", \"pvd2\", ...]");
            free(url);
            return VLC_EGENERIC;
        }
        end = (int) rm[2].rm_eo;
        memcpy(matchstr, line+start, end-start);
        matchstr[end-start] = '\0';

        // initialize dynamic array containing PvDs
        vlc_array_t *pvds = malloc(sizeof(vlc_array_t));
        if (!pvds) {
            msg_Err(crd, "Unable to allocate memory to create a dynamic array.");
            free(url);
            return VLC_EGENERIC;
        }
        vlc_array_init(pvds);

        // parse PvDs
        if (vlc_tls_pvd_parse_list(crd, pvds, re_pvd, strdup(matchstr)) != VLC_SUCCESS) {
            msg_Err(crd, "Unable to parse PvDs from config file.\n"
                         "Please only use valid PvD domain names.");
            free(url);
            return VLC_EGENERIC;
        }

        vlc_dictionary_insert(url_pvds, url, pvds);
        msg_Dbg(crd, "PvDs matching entry added for URL: %s", url);
        free(url);
    }
    else if (ret == REG_NOMATCH) {
        msg_Dbg(crd, "regex: no match in line: %s", line);
    }
    else {
        regerror(ret, &re_line, errbuf, sizeof(errbuf));
        msg_Err(crd, "regexec error while line parsing: %s", errbuf);
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

int vlc_tls_pvd_parse_config(vlc_tls_client_t *crd, vlc_dictionary_t *url_pvds,
                            const char *config_file) {
    vlc_dictionary_init(url_pvds, 0);
    if (config_file) {
        FILE *pvd_file = vlc_fopen(config_file, "r");
        if (!pvd_file) {
            msg_Err(crd, "Unable to open PvD config file");
            return VLC_EGENERIC;
        }

        char *line = NULL;
        size_t len = 1024;
        // compile regular expressions
        regex_t re_line;
        regex_t re_pvd;
        if (regcomp(&re_line, "\"([^\"]+)\"\\s*:\\s*(\\[?\"[^\\n]+\"\\]?)", REG_EXTENDED) ||
            regcomp(&re_pvd, "\"([a-zA-Z0-9\\.]+)\"", REG_EXTENDED)) {
            msg_Err(crd, "Could not compile one of the regular expressions.");
            return VLC_EGENERIC;
        }

        while (getline(&line, &len, pvd_file) != -1) {
            if (vlc_tls_pvd_parse_line(crd, url_pvds, re_line, re_pvd, line) != VLC_SUCCESS) {
                return VLC_EGENERIC;
            }
        }
        regfree(&re_line);
        free(line);
        fclose(pvd_file);
        msg_Dbg(crd, "Number of keys: %d", vlc_dictionary_keys_count(url_pvds));
    }
    return VLC_SUCCESS;
}

static void vlc_tls_clear_array(void *p_item, void *p_obj)
{
    VLC_UNUSED(p_obj);
    vlc_array_clear(p_item);
}

vlc_tls_client_t *vlc_tls_ClientCreate(vlc_object_t *obj)
{
    vlc_tls_client_t *crd = vlc_custom_create(obj, sizeof (*crd),
                                              "tls client");
    if (unlikely(crd == NULL))
        return NULL;

    if (vlc_module_load(crd, "tls client", NULL, false,
                        tls_client_load, crd) == NULL)
    {
        msg_Err (crd, "TLS client plugin not available");
        vlc_object_delete(crd);
        return NULL;
    }

    /// open and parse PvD config file
    char *pvd_config = var_InheritString(crd, "pvd-config");
    // dictionary mapping urls to PvD names
    vlc_dictionary_t *url_pvds = malloc(sizeof(vlc_dictionary_t));
    if (!url_pvds) {
        msg_Err(crd, "Unable to allocate memory for a dictionary structure"
                     "mapping URLs to PvDs.");
        return NULL;
    }

    int ret = vlc_tls_pvd_parse_config(crd, url_pvds, pvd_config);
    if (ret != VLC_SUCCESS) {
        vlc_dictionary_clear(url_pvds, vlc_tls_clear_array, NULL);
        free(url_pvds);
        return NULL;
    }

    crd->url_pvds = url_pvds;

    return crd;
}

void vlc_tls_ClientDelete(vlc_tls_client_t *crd)
{
    if (crd == NULL)
        return;

    crd->ops->destroy(crd);
    vlc_objres_clear(VLC_OBJECT(crd));
    vlc_dictionary_clear(crd->url_pvds, vlc_tls_clear_array, NULL);
    free(crd->url_pvds);
    vlc_object_delete(crd);
}


/*** TLS  session ***/

void vlc_tls_SessionDelete (vlc_tls_t *session)
{
    int canc = vlc_savecancel();
    session->ops->close(session);
    vlc_restorecancel(canc);
}

static void cleanup_tls(void *data)
{
    vlc_tls_t *session = data;

    vlc_tls_SessionDelete (session);
}

vlc_tls_t *vlc_tls_ClientSessionCreate(vlc_tls_client_t *crd, vlc_tls_t *sock,
                                       const char *host, const char *service,
                                       const char *const *alpn, char **alp)
{

    int val;
    int canc = vlc_savecancel();
    vlc_tls_t *session = crd->ops->open(crd, sock, host, alpn);
    vlc_restorecancel(canc);

    if (session == NULL)
        return NULL;
    printf("session fd = %d\n", session->ops->get_fd(session, NULL));

    session->p = sock;

    canc = vlc_savecancel();
    vlc_tick_t deadline = vlc_tick_now ();
    deadline += VLC_TICK_FROM_MS( var_InheritInteger (crd, "ipv4-timeout") );

    vlc_cleanup_push (cleanup_tls, session);
    while ((val = crd->ops->handshake(session, host, service, alp)) != 0)
    {
        printf("while loop\n");
        struct pollfd ufd[1];

        //printf ("ClientSessionCreate: vlc_BindToPvd return: %d\n", vlc_BindToPvd("test.example.com."));
        if (val < 0 || vlc_killed() )
        {
            if (val < 0)
                msg_Err(crd, "TLS session handshake error");
error:
            vlc_tls_SessionDelete (session);
            session = NULL;
            break;
        }

        vlc_tick_t now = vlc_tick_now ();
        if (now > deadline)
           now = deadline;

        assert (val <= 2);

        ufd[0].events = (val == 1) ? POLLIN : POLLOUT;
        ufd[0].fd = vlc_tls_GetPollFD(sock, &ufd->events);

        vlc_restorecancel(canc);
        val = vlc_poll_i11e(ufd, 1, MS_FROM_VLC_TICK(deadline - now));
        canc = vlc_savecancel();
        if (val == 0)
        {
            msg_Err(crd, "TLS session handshake timeout");
            goto error;
        }
    }
    vlc_cleanup_pop();
    vlc_restorecancel(canc);
    return session;
}

vlc_tls_t *vlc_tls_ServerSessionCreate(vlc_tls_server_t *crd,
                                       vlc_tls_t *sock,
                                       const char *const *alpn)
{
    int canc = vlc_savecancel();
    vlc_tls_t *session = crd->ops->open(crd, sock, alpn);
    vlc_restorecancel(canc);
    if (session != NULL)
        session->p = sock;
    return session;
}

// variable holding preferred provisioning domain
static char *pref_pvd = NULL;

vlc_tls_t *vlc_tls_SocketOpenTLS(vlc_tls_client_t *creds, const char *name,
                                 unsigned port, const char *service,
                                 const char *const *alpn, char **alp)
{
    struct addrinfo hints =
    {
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    }, *res;

    //printf ("SocketOpenTLS: vlc_BindToPvd return: %d\n", vlc_BindToPvd("test.example.com."));

    /*if (creds->url_pvds && !pref_pvd) {
        msg_Dbg(creds, "trying to bind to preconfigured PvD");

        // check for preferred default PvD
        char *def_pvd = NULL;
        vlc_array_t *pvd_arr = NULL;
        if (vlc_dictionary_has_key(creds->url_pvds, "default")) {
            pvd_arr = vlc_dictionary_value_for_key(creds->url_pvds, "default");
            def_pvd = vlc_array_item_at_index(pvd_arr, 0);
        }

        // traverse different URL keys
        char **keys = vlc_dictionary_all_keys(creds->url_pvds);
        for (int i = 0; i < vlc_dictionary_keys_count(creds->url_pvds); ++i) {
            if (strstr(name, keys[i])) {
                pvd_arr = vlc_dictionary_value_for_key(creds->url_pvds, keys[i]);
                char *pvd = NULL;

                // check if default preferred PvD in array associated to URL
                bool use_def_pvd = 0;
                if (def_pvd) {
                    for (size_t j = 0; j < vlc_array_count(pvd_arr); ++j) {
                        if (strcmp(def_pvd, vlc_array_item_at_index(pvd_arr, j)) == 0) {
                            pvd = def_pvd;
                            use_def_pvd = 1;
                            break;
                        }
                    }
                }

                // if no default PvD used, take one randomly
                if (!use_def_pvd) {
                    srand(time(NULL));
                    int index = rand();
                    index = index % vlc_array_count(pvd_arr);
                    printf("vlc_array_count: %ld\nindex: %d\n", vlc_array_count(pvd_arr), index);
                    pvd = vlc_array_item_at_index(pvd_arr, index);
                }

                // binding process to PvD
                if (vlc_BindToPvd(pvd))
                    msg_Dbg(creds, "Unable to bind process to PvD \"%s\"", pvd);
                else
                    msg_Dbg(creds, "Process bound to PvD: %s", pvd);
                break;
            }
        }
    }*/

    msg_Dbg(creds, "resolving %s ...", name);
    printf("resolving %s ..., port=%u\n", name, port);

    int val = vlc_getaddrinfo_i11e(name, port, &hints, &res);
    printf("passed dns check: port=%u, val=%d\n", port, val);
    if (val != 0)
    {   /* TODO: C locale for gai_strerror() */
        msg_Err(creds, "cannot resolve %s port %u: %s", name, port,
                gai_strerror(val));
        return NULL;
    }

    for (const struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        vlc_tls_t *tcp = vlc_tls_SocketOpenAddrInfo(p, true);
        if (tcp == NULL)
        {
            msg_Err(creds, "socket error: %s", vlc_strerror_c(errno));
            continue;
        }
        printf("tcp != NULL\n");

        vlc_tls_t *tls = vlc_tls_ClientSessionCreate(creds, tcp, name, service,
                                                     alpn, alp);
        printf("ClientSessionCreate passed\n");
        if (tls != NULL)
        {   /* Success! */
            printf("tls != NULL\n");
            freeaddrinfo(res);
            return tls;
        }
        printf ("tls == NULL\n");

        msg_Err(creds, "connection error: %s", vlc_strerror_c(errno));
        vlc_tls_SessionDelete(tcp);
    }

    /* Failure! */
    freeaddrinfo(res);
    return NULL;
}


void vlc_tls_SetPreferredPvd(const char *pvdname) {
    free(pref_pvd);
    pref_pvd = (pvdname) ? strdup(pvdname) : NULL;
}