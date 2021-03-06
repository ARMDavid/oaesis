/*
** srv_get_sockets.c
**
** Copyright 1999 - 2001 Christer Gustavsson <cg@nocrew.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** Read the file COPYING for more information.
*/

#define DEBUGLEVEL 0

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debug.h"
#include "srv_get.h"
#include "srv_interface.h"
#include "srv_sockets.h"

#define BACKLOG 256

/*
** Module type definitions
*/
typedef struct comm_handle_s {
  int         fd;
  COMM_HANDLE next;
} COMM_HANDLE_S;

/*
** Module global variables
*/
static int sockfd;  /* Server socket descriptor */

/* List of handles that are not reported to have data waiting */
static COMM_HANDLE selectable_handles  = COMM_HANDLE_NIL;

/* List of handles that are waiting to be passed to the server */
static COMM_HANDLE first_queued_handle = COMM_HANDLE_NIL;
static COMM_HANDLE last_queued_handle  = COMM_HANDLE_NIL;


#define QUEUE_EMPTY (first_queued_handle == COMM_HANDLE_NIL)

/*
** Description
** Insert a handle last in the queue
*/
static
inline
void
insert_last (COMM_HANDLE handle) {
  if (first_queued_handle == COMM_HANDLE_NIL) {
    /* Previously no handles queued */
    first_queued_handle = last_queued_handle = handle;
  } else {
    last_queued_handle->next = handle;
    last_queued_handle = handle;
  }

  handle->next = COMM_HANDLE_NIL;
}


/*
** Description
** Pop the first queued handle
*/
static
inline
COMM_HANDLE
pop_first (void) {
  COMM_HANDLE handle;

  if (first_queued_handle == COMM_HANDLE_NIL) {
    handle = COMM_HANDLE_NIL;
  } else {
    handle = first_queued_handle;

    if (first_queued_handle == last_queued_handle) {
      first_queued_handle = last_queued_handle = COMM_HANDLE_NIL;
    } else {
      first_queued_handle = first_queued_handle->next;
    }
  }

  return handle;
}


/*
** Description
** Open the server connection
**
** 1998-09-25 CG
** 1999-02-03 CG
** 1999-08-22 CG
*/
void
Srv_open (void) {
  struct sockaddr_in my_addr;  /* Address information for server */

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    DEBUG1 ("oaesis: srv_get_sockets.c: Srv_open: socket: %s: ",
	    strerror (errno));
    return;
  }
  
  my_addr.sin_family = AF_INET;         /* host byte order */
  my_addr.sin_port = htons(MYPORT);     /* short, network byte order */
  my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
  memset (&(my_addr.sin_zero), 0, 8);   /* zero the rest of the struct */
  
  if (bind(sockfd,
           (struct sockaddr *)&my_addr,
           sizeof(struct sockaddr)) == -1) {
    DEBUG1 ("oaesis: Srv_open: bind: %s", strerror (errno));
    return;
  }

  if (listen(sockfd, BACKLOG) == -1) {
    DEBUG1 ("oaesis: Srv_open: listen: %s", strerror (errno));
    return;
  }
}


/*
** Description
** Wait for a message from a client
*/
COMM_HANDLE
Srv_get (void * in,
         int    max_bytes_in)
{
  int                   sin_size = sizeof (struct sockaddr_in);
  struct sockaddr_in    their_addr; /* Client address information */
  fd_set                handle_set;
  COMM_HANDLE           handle_walk;
  COMM_HANDLE *         handle_ref_walk;
  int                   new_fd;
  int                   highest_fd;
  int                   err;
  static struct timeval timeout = {0, 0};
  /*  static int count = 0;*/

  FD_ZERO (&handle_set);

  if (QUEUE_EMPTY)
  {
    /* Timeout after 50 ms */
    timeout.tv_usec = 50000;
  }
  else
  {
    timeout.tv_usec = 0;
  }

  /* Specify which handles to wait for */
  FD_SET (sockfd, &handle_set);
  highest_fd = sockfd;

  handle_walk = selectable_handles;
  while (handle_walk)
  {
    FD_SET (handle_walk->fd, &handle_set);
    if (handle_walk->fd > highest_fd) {
      highest_fd = handle_walk->fd;
    }
    handle_walk = handle_walk->next;
  }

  /* Wait for input */
  err = select (highest_fd + 1,
		&handle_set,
		NULL,
		NULL,
		&timeout);

  if(err == -1)
  {
    return NULL;
  }

  /* We got a timeout */
  if (QUEUE_EMPTY && (err == 0))
  { 
    return NULL;
  }

  if (FD_ISSET (sockfd, &handle_set))
  {
    COMM_HANDLE new_handle;

    /* A new application has called the server*/

    if ((new_fd = accept (sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
      DEBUG1 ("oaesis: Srv_get: accept: %s", strerror (errno));

      return NULL;
    }

    /* Allocate a new handle and put it in the queue */
    new_handle = (COMM_HANDLE)malloc (sizeof (COMM_HANDLE_S));
    new_handle->fd = new_fd;
    insert_last (new_handle);
  }

  /* Check for applications that have sent data and queue them */
  handle_ref_walk = &selectable_handles;
  
  while (*handle_ref_walk != NULL)
  {
    if (FD_ISSET ((*handle_ref_walk)->fd, &handle_set)) {
      COMM_HANDLE data_handle;

      /* A handle with data has been found */
      data_handle = *handle_ref_walk;
      *handle_ref_walk = (*handle_ref_walk)->next;

      insert_last (data_handle);
    } else {
      handle_ref_walk = &(*handle_ref_walk)->next;
    }
  }

  handle_walk = pop_first ();
  /*  DB_printf ("popped %p", handle_walk);*/

  /* Something strange has happened */
  if(handle_walk == NULL)
  {
    return NULL;
  }

  /* Receive data */
  if ((err = recv (handle_walk->fd, in, max_bytes_in, 0)) == -1)
  {
    /* The application died */
    ((C_ALL *)in)->words = 1;
    ((C_ALL *)in)->longs = 0;
    ((C_ALL *)in)->call  = SRV_CONN_LOST;

    return handle_walk;
  }
  
  DEBUG3("srv_get_sockets.c: length = %d", err);

  /* The handle should be selected the next call */
  handle_walk->next = selectable_handles;
  selectable_handles = handle_walk;

  if(err == 0)
  {
    return NULL;
  }
  else
  {
    return handle_walk;
  }
}


/*
** Description
** Reply with a message to a client
**
** 1998-09-25 CG
** 1998-12-13 CG
** 1999-08-22 CG
*/
void
Srv_reply (COMM_HANDLE handle,
           void *      out,
           WORD        bytes_out) {
  if (send (handle->fd, out, bytes_out, 0) == -1) {
    DEBUG1 ("oaesis: Srv_reply: send: %s", strerror (errno));
    return;
  }
}

