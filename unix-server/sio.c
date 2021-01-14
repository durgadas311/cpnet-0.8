/*************************************************************************

    This file is part of the CP/NET 1.1/1.2 server emulator for Unix.
    This module implements low level serial port I/O routines.
  
    Copyright (C) 2004, Hector Peraza.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>

#include "sio.h"


/*----------------------------------------------------------------------*/

static int _fdo = -1;
static int _fdi = -1;
static char *_sdev = NULL;
static int _stdio = 0;

int sio_open(char *sdev, int speed) {
  struct termios ts;
  int off = 0;
  int modemlines;

  sio_close();

  if (strcmp(sdev, "-") == 0) {
    /* requires _log to be changed... */
    _fdo = 1;
    _fdi = 0;
    _stdio = 1;
    if (_sdev) free(_sdev);
    _sdev = strdup(sdev);
    return 0;
  } else {
    _fdo = _fdi = open(sdev, O_RDWR | O_NOCTTY | O_NONBLOCK, 0);
  }

  if (_sdev) free(_sdev);
  _sdev = strdup(sdev);

  if (_fdo < 0) {
    perror(sdev);
    return -1;
  }

  if (!isatty(_fdo)) {
    fprintf(stderr, "%s: not a tty\n", sdev);
    close(_fdo);
    _fdo = -1;
    return -1;
  }

  if (tcgetattr(_fdo, &ts) == -1) {
    fprintf(stderr, "failed to get tty settings\n");
    perror(sdev);
    close(_fdo);
    _fdo = -1;
    return -1;
  }

  cfmakeraw(&ts);
  ts.c_iflag = IGNBRK;
  ts.c_oflag = 0;
  ts.c_cflag = CS8 | CREAD | CLOCAL;
  ts.c_lflag = 0;
  ts.c_cc[VMIN] = 0;    /* 1 */;
  ts.c_cc[VTIME] = 50;  /* one character at a time, 5 sec timeout */

  if (cfsetospeed(&ts, speed) == -1) {
    fprintf(stderr, "failed to set output speed\n");
    perror(sdev);
  }

  if (cfsetispeed(&ts, speed) == -1) {
    fprintf(stderr, "failed to set input speed\n");
    perror(sdev);
  }

  if (tcsetattr(_fdo, TCSAFLUSH, &ts) == -1) {
    fprintf(stderr, "failed to set line attributes\n");
    perror(sdev);
  }

  /* Set the line back to blocking mode after setting CLOCAL */
  if (ioctl(_fdo, FIONBIO, &off) < 0) {
    fprintf(stderr, "failed to set blocking mode\n");
    perror(sdev);
  }

  modemlines = TIOCM_RTS;
  if (ioctl(_fdo, TIOCMBIC, &modemlines)) {
    /* can't clear RTS line, see ERRNO */
    fprintf(stderr, "failed to clear RTS line\n");
    perror(sdev);
  }

  return 0;
}

int sio_close() {
  int retc;

  if (_fdo == -1) {
    errno = EBADF;
    return -1;
  }

  retc = close(_fdo);
  _fdo = -1;

  return retc;
}

int sio_set_speed(int speed) {
  struct termios ts;

  if (_fdo < 0 || _stdio) return 0;

  if (tcgetattr(_fdo, &ts) == -1) {
    perror(_sdev);
    return -1;
  }

  if (cfsetospeed(&ts, speed) == -1) {
    perror(_sdev);
    return -1;
  }

  if (cfsetispeed(&ts, speed) == -1) {
    perror(_sdev);
    return -1;
  }

  if (tcsetattr(_fdo, TCSAFLUSH, &ts) == -1) {
    perror(_sdev);
    return -1;
  }

  return 0;
}

int sio_send(char *buf, int len) {
  if (_fdo < 0) return -1;
  if (len < 0) len = strlen(buf);
  if (write(_fdo, buf, len) == len) return 0;
  return -1;
}

int sio_receive(char *buf, int len) {
  int i, n;

  if (_fdo < 0) return -1;
  for (i = 0; i < len; ++i) {
    n = read(_fdi, &buf[i], 1);
    if (n == 0) {		/* timeout! */
      if (_stdio) { /* for stdio, this is fatal */
        exit(0);
      }
      break;
    }
  }
  return i;
}
