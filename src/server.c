/* server.c - This file is part of the bitu program
 *
 * Copyright (C) 2010  Lincoln de Sousa <lincoln@comum.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <taningia/taningia.h>
#include <bitu/server.h>
#include <bitu/util.h>

#include "server-priv.h"
#include "hashtable.h"
#include "hashtable-utils.h"

#define LISTEN_BACKLOG 1


/* Client data type */


bitu_client_t *
bitu_client_new (int socket)
{
  bitu_client_t *client;
  if ((client = malloc (sizeof (bitu_client_t))) == NULL)
    return NULL;
  client->id = bitu_util_uuid4 ();
  client->socket = socket;
  return client;
}


void
bitu_client_free (bitu_client_t *client)
{
  free (client->id);
  free (client);
}


const char *
bitu_client_get_id (bitu_client_t *client)
{
  return (const char *) client->id;
}


int
bitu_client_get_socket (bitu_client_t *client)
{
  return client->socket;
}


/* Public API */


bitu_server_t *
bitu_server_new (const char *sock_path, bitu_server_callbacks_t callbacks)
{
  bitu_server_t *server;
  if ((server = malloc (sizeof (bitu_server_t))) == NULL)
    return NULL;
  server->logger = ta_log_new ("bitu-server");
  server->sock_path = strdup (sock_path);
  server->clients = hashtable_create (hash_string, string_equal,
                                      NULL, /* No need to free keys */
                                      (free_fn) bitu_client_free);
  server->can_run = 1;
  server->callbacks = callbacks;
  return server;
}


void
bitu_server_free (bitu_server_t *server)
{
  ta_log_info (server->logger, "Gracefully exiting, see you!");
  hashtable_destroy (server->clients);
  server->can_run = 0;
  close (server->sock);
  unlink (server->sock_path);
  free (server->sock_path);
  free (server);
}


void
bitu_server_set_data (bitu_server_t *server, void *data)
{
  server->data = data;
}


void *
bitu_server_get_data (bitu_server_t *server)
{
  return (void *) server->data;
}


int
bitu_server_connect (bitu_server_t *server)
{
  int len;
  struct sockaddr_un local;

  if ((server->sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
      ta_log_critical (server->logger, "Unable to open socket server");
      return TA_ERROR;
    }

  local.sun_family = AF_UNIX;
  strcpy (local.sun_path, server->sock_path);
  unlink (local.sun_path);

  len = strlen (local.sun_path) + sizeof (local.sun_family);
  if (bind (server->sock, (struct sockaddr *) &local, len) == -1)
    {
      ta_log_critical (server->logger,
                       "Error when binding to socket %s: %s",
                       server->sock_path,
                       strerror (errno));
      return TA_ERROR;
    }
  if (listen (server->sock, LISTEN_BACKLOG) == -1)
    {
      ta_log_critical (server->logger,
                       "Error to listen to connections: %s",
                       strerror (errno));
      return TA_ERROR;
    }

  ta_log_info (server->logger,
               "Local server bound to the socket in %s",
               server->sock_path);
  return TA_OK;
}


int
bitu_server_recv (bitu_server_t *server, int sock,
                  char *buf, size_t bufsize, int timeout)
{
  int n, r;
  fd_set fds;
  struct timeval tv, *tvptr;

  FD_ZERO (&fds);
  FD_SET (sock, &fds);

  if (timeout == -1)
    tvptr = NULL;
  else
    {
      tv.tv_sec = 0;
      tv.tv_usec = timeout;
      tvptr = &tv;
    }

  r = select (sock + 1, &fds, NULL, NULL, tvptr);
  if (r == -1 && (errno == EAGAIN || errno == EINTR))
    return 0;
  else if (r == -1)
    {
      ta_log_error (server->logger, "Error in select(): %s", strerror (errno));
      return -1;
    }
  if (FD_ISSET (sock, &fds))
    {
      do
        n = recv (sock, buf, bufsize, 0);
      while (n == -1 && (errno == EAGAIN));
      if (n < 0)
        {
          ta_log_error (server->logger, "Error in recv(): %s", strerror (errno));
          return -1;
        }
      else
        return n;
    }
  return 0;
}

int
bitu_server_send (bitu_server_t *server, const char *msg, const char *to)
{
  bitu_client_t * client;
  int n;
  int sock;
  size_t sent = 0;
  size_t bufsize = strlen (msg);

  if ((client = hashtable_get (server->clients, to)) == NULL)
    return TA_ERROR;

  sock = bitu_client_get_socket (client);

  while (sent < bufsize)
    {
      n = send (sock, msg, bufsize, 0);
      if (n == -1 && errno == EAGAIN)
        continue;
      if (n == -1)
        {
          ta_log_error (server->logger, "Error in send(): %s", strerror (errno));
          return TA_ERROR;
        }
      sent += n;
    }
  return TA_OK;
}


int
bitu_server_run (bitu_server_t *server)
{
  int sock;
  struct sockaddr_un remote;

  while (server->can_run)
    {
      bitu_client_t *client;
      const char *client_id;
      socklen_t len;
      char *str = NULL;
      int full_len, allocated;
      int timeout;

      len = sizeof (struct sockaddr_un);

      /* EINTR means the the resource is not ready, so we should try
       * agian */
      do
        sock = accept (server->sock, (struct sockaddr *) &remote, &len);
      while (sock == -1 && errno == EINTR);

      /* Unlucky, some thing is wrong. Report and fail */
      if (sock == -1)
        {
          ta_log_error (server->logger, "Error in accept(): %s", strerror (errno));
          continue;
        }
      ta_log_info (server->logger, "Client connected");

      /* Let's save the client with a uniq identifier to make it
       * possible to find the correct client when we need to send a
       * messages to this client in the future */
      client = bitu_client_new (sock);
      client_id = bitu_client_get_id (client);
      hashtable_set (server->clients, (void *) client_id, client);

      while (1)
        {
          /* Initializing variables that are going to be used when
           * allocating space to store the whole command received via
           * socket. */
          full_len = allocated = 0;
          str = NULL;
          timeout = -1;

          while (1)
            {
              int n, bufsize = 128;
              char buf[bufsize];
              n = bitu_server_recv (server, sock, buf, bufsize, timeout);
              ta_log_debug (server->logger, "recv() returned: %d", n);
              if (n == 0)
                {
                  ta_log_info (server->logger, "End of client stream");
                  break;
                }
              if (n < 0)
                break;
              buf[n] = '\0';
              if ((full_len + n) > allocated)
                {
                  char *tmp;
                  while (allocated < (full_len + n))
                    allocated += bufsize;
                  if ((tmp = realloc (str, allocated)) == NULL)
                    {
                      if (str)
                        free (str);
                      break;
                    }
                  else
                    str = tmp;
                }
              memcpy (str + full_len, buf, n);
              full_len += n;
              timeout = 0;
            }
          if (str)
            str[full_len] = '\0';

          /* Client is saying good bye, it means that no command
           * should be run. It is time to get out */
          if ((str == NULL) ||
              (str && strcmp (str, "exit") == 0) ||
              (str && strlen (str) < 2))
            {
              hashtable_del (server->clients, client_id);
              close (sock);
              break;
            }

          /* We just received a message, let's fire the callback */
          if (server->callbacks.message_received)
            server->callbacks.message_received (server,
                                                "message-received",
                                                client_id,
                                                str);
        }
    }
  return TA_OK;
}
