/*************************************************************************

    CP/NET 1.1/1.2 server emulator for Unix systems.

    This program is used to communicate with a single CP/M requester
    connected to a PC serial port using the RS232 protocol described
    in Appendix E of the CP/NET documentation.
  
    The program emulates either version 1.1 or 1.2 of CP/NET, and
    therefore must be used with a requester running the same CP/NET
    version.

    CP/NET versions 1.0, 1.1 and 1.2 are NOT compatible with each other.
    Although they may use the same physical-layer protocol (for a serial
    line, for example), they differ at the network-layer level.

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
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>

#include "main.h"
#include "cpnet11.h"
#include "cpnet12.h"
#include "netio.h"
#include "sio.h"
#include "inifile.h"
#include "cpmutl.h"


/*----------------------------------------------------------------------*/

int  _netID = 0;   /* our server ID */
int  _clientID = 0; /* our client's ID */
int  _debug = 0;   /* debug mask */
FILE *_log = NULL;

int  _logged_in = 0;
char _passwd[8];

char _sdev[256];
int  _speed;

int  _level = CPNET_1_2;

extern uchar allocv[256];

/* CP/M disk number to Unix directory mappings */
char *disk_to_dir[16];

/* LST to Unix device mappings */
struct lstmap {
  char *fname;
  FILE *f;
} lst_to_dev[8];

/* CP/NET function names, for debugging */
char *fn_name[] = {
  /* 00 */  "system reset",
  /* 01 */  "console input",
  /* 02 */  "console output",
  /* 03 */  "raw console input",
  /* 04 */  "raw console output",
  /* 05 */  "list output",
  /* 06 */  "direct console I/O",
  /* 07 */  "get I/O byte",
  /* 08 */  "set I/O byte",
  /* 09 */  "print string",
  /* 10 */  "read console buffer",
  /* 11 */  "get console status",
  /* 12 */  "get version number",
  /* 13 */  "reset disk system",
  /* 14 */  "select disk",
  /* 15 */  "open file",
  /* 16 */  "close file",
  /* 17 */  "search first",
  /* 18 */  "search next",
  /* 19 */  "delete file",
  /* 20 */  "read sequential",
  /* 21 */  "write sequential",
  /* 22 */  "create file",
  /* 23 */  "rename file",
  /* 24 */  "get login vector",
  /* 25 */  "get current disk",
  /* 26 */  "set DMA address",
  /* 27 */  "get allocation vector",
  /* 28 */  "write protect disk",
  /* 29 */  "get R/O vector",
  /* 30 */  "set file attributes",
  /* 31 */  "get DPB",
  /* 32 */  "get/set user code",
  /* 33 */  "read random",
  /* 34 */  "write random",
  /* 35 */  "compute file size",
  /* 36 */  "set random record",
  /* 37 */  "reset drive",
  /* 38 */  "access drive",
  /* 39 */  "free drive",
  /* 40 */  "write random with zero fill",

  /* 64 */  "login",
  /* 65 */  "logoff",
  /* 66 */  "send message on network",
  /* 67 */  "receive message on network",
  /* 68 */  "get network status",
  /* 69 */  "get config table address"
};

/*----------------------------------------------------------------------*/

int main(int argc, char *argv[]) {
  int  i;
  char *ini_name = NULL;
  FILE *log;
  unsigned char buf[1024];

  /* initializations */
  _log = stdout;

  for (i = 0; i < 16; ++i) {
    disk_to_dir[i] = NULL;
  }
  for (i = 0; i < 8; ++i) {
    lst_to_dev[i].fname = NULL;
    lst_to_dev[i].f = NULL;
  }
  strcpy(_sdev, "/dev/ttyS0");
  _speed = B38400;

  strncpy(_passwd, "PASSWORD", 8);

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-ini") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -ini option requires argument\n", argv[0]);
      } else {
        ini_name = argv[i];
      }
      break;
    }
  }
  read_ini(ini_name);
  
  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-level") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -level option requires argument\n", argv[0]);
      } else {
        if (strcmp(argv[i], "1.1") == 0)
          _level = CPNET_1_1;
        else if (strcmp(argv[1], "1.2") == 0)
          _level = CPNET_1_2;
        else
          fprintf(stderr, "%s: unknown CP/NET level, must be 1.1 or 1.2\n", argv[0]);
      }
    } else if (strcmp(argv[i], "-log") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -log option requires argument\n", argv[0]);
      } else {
        log = fopen(argv[i], "w");
        if (log != NULL) _log = log;
      }
    } else if (strcmp(argv[i], "-debug") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -debug option requires argument\n", argv[0]);
      } else {
        _debug = atoi(argv[i]);
      }
    } else if (strcmp(argv[i], "-port") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -port option requires argument\n", argv[0]);
      } else {
        strncpy(_sdev, argv[i], 255);
        _sdev[255] = '\0';
      }
    } else if (strcmp(argv[i], "-speed") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -speed option requires argument\n", argv[0]);
      } else {
        if (set_speed(atoi(argv[i])) < 0) {
          fprintf(stderr, "%s: invalid serial speed, defaulting to 38400\n", argv[0]);
        }
      }
    } else if (strcmp(argv[i], "-pwd") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -pwd option requires argument\n", argv[0]);
      } else {
        int j, len = strlen(argv[i]);
        for (j = 0; j < 8; ++j) {
          if (j >= len)
            _passwd[j] = ' ';
          else
            _passwd[j] = toupper(argv[i][j]);
        }
      }
    } else if (strcmp(argv[i], "-netid") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -netid option requires argument\n", argv[0]);
      } else {
        _netID = atoi(argv[i]);
      }
    } else if (strcmp(argv[i], "-clientid") == 0) {
      ++i;
      if (!argv[i]) {
        fprintf(stderr, "%s: -clientid option requires argument\n", argv[0]);
      } else {
        _clientID = strtol(argv[i], NULL, 0);
      }
    } else if (strcmp(argv[i], "-ini") == 0) {
      ++i;  /* we already processed this option */
    } else if ((strcmp(argv[i], "-version") == 0) ||
               (strcmp(argv[i], "--version") == 0)) {
      printf("%s: CP/NET server version %s\n", argv[0], VERSION);
      return 0;
    } else if ((strcmp(argv[i], "-help") == 0) ||
               (strcmp(argv[i], "--help") == 0)) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "%s: unknown option %s, use -help for help\n", argv[0], argv[i]);
    }
  }

  if (!disk_to_dir[0]) {
    getcwd(buf, 1024);
    strcat(buf, "/cpmfiles");
    disk_to_dir[0] = strdup(buf);  /* by default only disk A: allowed */
  }

  if (_debug & DEBUG_MISC) {
    for (i = 0; i < 16; ++i) {
      if (disk_to_dir[i]) {
        fprintf(_log, "%c: = %s\n", i + 'A', disk_to_dir[i]);
        fflush(_log);
      }
    }
  }

  for (i = 0; i < 8; ++i) {
    if (lst_to_dev[i].fname) {
      if (_debug & DEBUG_MISC) {
        fprintf(_log, "LST%d: = %s\n", i, lst_to_dev[i].fname);
        fflush(_log);
      }
      if (strcmp(lst_to_dev[i].fname, "-") == 0) {
        lst_to_dev[i].f = stdout;
      } else if (strcmp(lst_to_dev[i].fname, "--") == 0) {
        lst_to_dev[i].f = stderr;
      } else {
        lst_to_dev[i].f = fopen(lst_to_dev[i].fname, "a");
        if (!lst_to_dev[i].f) {
          fprintf(stderr, "%s: could not open %s for lst%d output\n",
                  argv[0], lst_to_dev[i].fname, i);
        }
      }
    }
  }
  
  if (sio_open(_sdev, _speed) != 0) {
    fprintf(stderr, "%s: could not open %s\n", argv[0], _sdev);
    return 1;
  }

  if (_debug & DEBUG_MISC) {
    fprintf(_log, "emulating CP/NET %s\n", (_level == CPNET_1_1) ? "1.1" : "1.2");
    fprintf(_log, "%s open at %d baud\n", _sdev, get_baud(_speed));
    fprintf(_log, "server id is %d\n", _netID);
    fprintf(_log, "network password is %.8s\n", _passwd);
    fprintf(_log, "entering server loop...\n");
    fflush(_log);
  }

  /* main loop - inside cpnet_11() and cpnet_12() routines */

  if (_level == CPNET_1_1) {
    cpnet_11();
  } else {
    cpnet_12();
  }

  /* right now we never get here */
  for (i = 0; i < 16; ++i) {
    if (disk_to_dir[i]) free(disk_to_dir[i]);
  }
  for (i = 0; i < 8; ++i) {
    if (lst_to_dev[i].f &&
        (lst_to_dev[i].f != stdout) &&
        (lst_to_dev[i].f != stderr)) fclose(lst_to_dev[i].f);
    if (lst_to_dev[i].fname) free(lst_to_dev[i].fname);
  }

  return 0;
}

void usage(char *pname) {
  printf("usage: %s [options]\n", pname);
  printf("  where 'options' is one or more of the following:\n");
  printf("    -help          shows this help\n");
  printf("    -version       shows the program version\n");
  printf("    -ini filename  specify an alternate initialization file\n");
  printf("    -port device   sets the serial port to 'device'\n");
  printf("    -speed value   sets the serial speed to 'value'\n");
  printf("    -level level   sets the emulated CP/NET version (1.1 or 1.2)\n");
  printf("    -netid value   sets the network id to 'value'\n");
  printf("    -pwd passwd    sets the network password to 'passwd'\n");
  printf("    -debug value   sets the debug mask to 'value'\n");
  printf("  the debug mask can be any combination of the following bits:\n");
  printf("    0  no debug information is output\n");
  printf("    1  show packet-level data transfer\n");
  printf("    2  show network-level data transfer\n");
  printf("    4  show other miscellaneous debug information\n");
  printf("\n");
  printf(" This program is free software; you can redistribute it and/or\n");
  printf(" modify it under the terms of the GNU General Public License.\n");
  printf("\n");
}

void dump_data(unsigned char *buf, int len) {
  int i, addr;
  
  addr = 0;
  while (len > 0) {
    fprintf(_log, "%04X: ", addr);
    for (i = 0; i < 16; ++i) {
      if (len > i)
        fprintf(_log, "%02X ", buf[addr+i]);
      else
        fprintf(_log, "   ");
    }
    for (i = 0; i < 16; ++i) {
      if (len > i)
        fprintf(_log, "%c", (buf[addr+i] >= 32 && buf[addr+i] < 127) ? buf[addr+i] : '.');
      else
        fprintf(_log, " ");
    }
    addr += 16;
    len -= 16;
    fprintf(_log, "\n");
    fflush(_log);
  }
}

/*----------------------------------------------------------------------*/

int goto_drive(int drive) {
  if (disk_to_dir[drive])
    return chdir(disk_to_dir[drive]);
  else
    return -1;
}

int lst_output(int num, char *buf, int len) {
  int i;
  
  if (num < 0 || num >= 8) return -1;
  if (!lst_to_dev[num].f) return -1;

  for (i = 0; i < len; ++i) {
    if (buf[i] == 0x1a) break;
    fputc(buf[i], lst_to_dev[num].f);
  }
  fflush(lst_to_dev[num].f);

  return 0;
}

/*----------------------------------------------------------------------*/

int read_ini(char *fname) {
  FILE *ini;
  FILE *log;
  int  i;
  char *home, buf[INI_MAX_LINE_LEN], arg[INI_MAX_LINE_LEN];

  if (fname) {
    strcpy(buf, fname);
  } else {
    home = getenv("HOME");
    if (!home) home = "/";
    sprintf(buf, "%s/.cpnet.ini", home);
  }
  
  ini = ini_openr(buf);
  if (!ini) return -1;

  while (ini_get_next(ini, buf)) {
    if (strcmp(buf, "defaults") == 0) {
      if (ini_get_item(ini, "level", arg)) {
        if (strcmp(arg, "1.1"))
          _level = CPNET_1_1;
        else
          _level = CPNET_1_2;
      }
      if (ini_get_item(ini, "debug", arg)) {
        _debug = atoi(arg);
      }
      if (ini_get_item(ini, "netid", arg)) {
        _netID = atoi(arg);
      }
      if (ini_get_item(ini, "log", arg)) {
        log = fopen(arg, "w");
        if (log != NULL) _log = log;
      }
      if (ini_get_item(ini, "clientid", arg)) {
        _clientID = strtol(arg, NULL, 0);
      }
      if (ini_get_item(ini, "password", arg)) {
        int len = strlen(arg);
        for (i = 0; i < 8; ++i) {
          if (i >= len)
            _passwd[i] = ' ';
          else
            _passwd[i] = toupper(arg[i]);
        }
      }
    } else if (strcmp(buf, "comm") == 0) {
      if (ini_get_item(ini, "device", arg)) {
        strcpy(_sdev, arg);
      }
      if (ini_get_item(ini, "speed", arg)) {
        set_speed(atoi(arg));
      }
    } else if (strcmp(buf, "drives") == 0) {
      for (i = 0; i < 16; ++i) {
        sprintf(buf, "%c", 'A'+i);
        if (ini_get_item(ini, buf, arg)) {
          disk_to_dir[i] = strdup(arg);
        }
      }
    } else if (strcmp(buf, "printers") == 0) {
      for (i = 0; i < 8; ++i) {
        sprintf(buf, "lst%d", i);
        if (ini_get_item(ini, buf, arg)) {
          lst_to_dev[i].fname = strdup(arg);
        }
      }
    }
  }
  
  ini_close(ini);
  return 0;
}

int set_speed(int baud) {
  switch (baud) {
    case   300:  _speed =   B300; break;
    case   600:  _speed =   B600; break;
    case  1200:  _speed =  B1200; break;
    case  2400:  _speed =  B2400; break;
    case  4800:  _speed =  B4800; break;
    case  9600:  _speed =  B9600; break;
    case 19200:  _speed = B19200; break;
    case 38400:  _speed = B38400; break;
    default:     _speed = B38400; return -1;
  }
  return 0;
}

int get_baud(int speed) {
  switch (speed) {
    case   B300:  return 300;
    case   B600:  return 600;
    case  B1200:  return 1200;
    case  B2400:  return 2400;
    case  B4800:  return 4800;
    case  B9600:  return 9600;
    case B19200:  return 19200;
    case B38400:  return 38400;
  }
  return 0;
}
