/*************************************************************************

    This file is part of the CP/NET 1.1/1.2 server emulator for Unix.

    This module implements the serial port communicarion protocol
    described in Appendix E of the CP/NET documentation.
  
    Copyright (C) 2005, Hector Peraza.

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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>

#include "main.h"
#include "netio.h"
#include "sio.h"


/*----------------------------------------------------------------------*/

extern int _level;     /* which CP/NET version is being emulated */
extern int _netID;     /* our server ID */
extern int _debug;     /* debug mask */

extern FILE *_log;

void wait_for_packet() {
  unsigned char buf[2];

  /* wait for ENQ byte... */
  while (1) {
    while (sio_receive(buf, 1) < 1) {}
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[0]);
      fflush(_log);
    }
    if (buf[0] == ENQ) break;
  }
      
  /* send ACK back */
  buf[0] = ACK;
  sio_send(buf, 1);
  if (_debug & DEBUG_PACKET) {
    fprintf(_log, "\t<< %02X\n", buf[0]);
    fflush(_log);
  }
}

int get_packet(char *data, int *len, int *fnc, int *sid) {
  int i, n, did, siz;
  unsigned char buf[1024], cks;

  /* receive header */
  n = sio_receive(buf, 7);
  cks = 0;
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
    cks += buf[i];
  }
  if (buf[0] != SOH || n != 7) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Bad packet\n\n");
      fflush(_log);
    }
    return -1;
  }
  if (cks != 0) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Checksum error\n\n");
      fflush(_log);
    }
    return -1;
  }
  did = buf[2];
  *sid = buf[3];
  *fnc = buf[4];
  siz = buf[5];
  *len = siz + 1;
  /* fnc 0xf0-0xff are specil proxy functions */
  if (did != _netID && *fnc < 0xf0) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Not for us...\n\n");
      fflush(_log);
    }
    return -1;
  }
      
  /* send ACK */
  buf[0] = ACK;
  sio_send(buf, 1);
  if (_debug & DEBUG_PACKET) {
    fprintf(_log, "\t<< %02X\n", buf[0]);
    fflush(_log);
  }
      
  /* receive data part */
  n = sio_receive(buf, siz+2);
  cks = 0;
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
    cks += buf[i];
  }
  if (buf[0] != STX) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Bad packet\n\n");
      fflush(_log);
    }
    return -1;
  }
  if (n != siz+2) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Bad length\n\n");
      fflush(_log);
    }
    return -1;
  }
  for (i = 0; i < siz+1; ++i) {
    data[i] = buf[i+1];
  }
      
  /* receive checksum field */
  n = sio_receive(buf, 2);
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
    cks += buf[i];
  }
  if (buf[0] != ETX || n != 2) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Bad packet\n\n");
      fflush(_log);
    }
    return -1;
  }
  if (cks != 0) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Checksum error\n\n");
      fflush(_log);
    }
    return -1;
  }
      
  /* receive trailer */
  n = sio_receive(buf, 1);
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
  }
  if (buf[0] != EOT || n != 1) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "Bad packet\n\n");
      fflush(_log);
    }
    return -1;
  }

  /* send ACK */
  buf[0] = ACK;
  sio_send(buf, 1);
  if (_debug & DEBUG_PACKET) {
      fprintf(_log, "\t<< %02X\n", buf[0]);
      fflush(_log);
    }
  
  return 0;
}

int send_packet(int to, int fnc, char *data, int len) {
  int i, n;
  unsigned char buf[1024], cks;

  if (_debug & DEBUG_DATA) {
    fprintf(_log, "Replying\n");
    dump_data(data, len);
    fprintf(_log, "\n");
    fflush(_log);
  }

  if (len < 1 || len > 256) {
    fprintf(stderr, "Error: can't send packet with length %d\n", len);
    return -1;
  }

  buf[0] = ENQ;
  sio_send(buf, 1);
  if (_debug & DEBUG_PACKET) {
      fprintf(_log, "\t<< %02X\n", buf[0]);
      fflush(_log);
    }
  /* wait for ACK */
  n = sio_receive(buf, 1);
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
  }
      
  buf[0] = SOH;
  buf[1] = 1;          /* FMT */
  buf[2] = to;         /* DID */
  buf[3] = _netID;     /* SID */
  buf[4] = fnc;        /* FNC */
  buf[5] = len - 1;    /* SIZ */
  cks = 0;
  for (i = 0; i < 6; ++i) {
    cks += buf[i];
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "\t<< %02X\n", buf[i]);
      fflush(_log);
    }
  }
  buf[6] = -cks;       /* HCS */
  if (_debug & DEBUG_PACKET) {
    fprintf(_log, "\t<< %02X\n", buf[6]);
    fflush(_log);
  }
  sio_send(buf, 7);
  /* wait for ACK */
  n = sio_receive(buf, 1);
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
  }

  buf[0] = STX;
  for (i = 0; i < len; ++i) {
    buf[i+1] = data[i];
  }

  buf[len+1] = ETX;
  cks = 0;
  for (i = 0; i < len + 2; ++i) {
    cks += buf[i];
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "\t<< %02X\n", buf[i]);
      fflush(_log);
    }
  }
  buf[len+2] = -cks;
  if (_debug & DEBUG_PACKET) {
    fprintf(_log, "\t<< %02X\n", buf[len+2]);
    fflush(_log);
  }
  buf[len+3] = EOT;
  if (_debug & DEBUG_PACKET) {
    fprintf(_log, "\t<< %02X\n", buf[len+3]);
    fflush(_log);
  }
  sio_send(buf, len+4);
  /* wait for ACK */
  n = sio_receive(buf, 1);
  for (i = 0; i < n; ++i) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, ">> %02X\n", buf[i]);
      fflush(_log);
    }
  }

  if (n < 1) {
    if (_debug & DEBUG_PACKET) {
      fprintf(_log, "No ACK?\n\n");
      fflush(_log);
    }
  }

  return 0;
}

int send_ok(int to, int fnc) {
  unsigned char uc = 0;
  return send_packet(to, fnc, &uc, 1);
}

int send_error(int to, int fnc) {
  if (_level == CPNET_1_1) {
    unsigned char buf[2] = { 0xff, 0xff };
    return send_packet(to, fnc, buf, 1);  /*2*/
  } else {
    unsigned char buf[2] = { 0xff, 0x0c };
    return send_packet(to, fnc, buf, 2);
  }
}
