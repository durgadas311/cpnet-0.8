/*************************************************************************

    This file is part of the CP/NET 1.1/1.2 server emulator for Unix.
    This module implements ini-file I/O support.
  
    The ini-files use a MS-Windows style, they are composed of sections
    and variable-value pairs like this:
  
      [section]
      variable = value
      ...

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
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "inifile.h"

static int _offset;


/*--------------------------------------------------------------------*/

FILE *ini_openr(const char *filename) {
  _offset = 0;
  return fopen(filename, "r");
}

FILE *ini_openw(const char *filename) {
  _offset = 0;
  return fopen(filename, "w");
}

int ini_close(FILE *ini) {
  return fclose(ini);
}

int ini_rewind(FILE *ini) {
  return fseek(ini, _offset = 0, SEEK_SET);
}

char *ini_get_item(FILE *ini, const char *item, char *value) {
  char  line[INI_MAX_LINE_LEN];
  char *tmp;

  *value = '\0';

  fseek(ini, _offset, SEEK_SET);
  while (!(feof(ini))) {
    if (fgets(line, INI_MAX_LINE_LEN, ini) != NULL) {
      line[strlen(line) - 1] = '\0';
      if ((strchr(line, '[') != 0) && (strchr(line, ']') != 0)) {
	return NULL;
      }
      if (strstr(line, item) == line) {
	strtok(line, "=");
        tmp = strtok(NULL, "=");
        while (tmp && (*tmp == ' ')) tmp++;
        if (tmp) {
          strcpy(value, tmp);
          return value;
	} else {
	  return NULL;
	}
      }
    }
  }
  return NULL;
}

int ini_get_bool(FILE *ini, const char *item, int default_val) {
  char line[INI_MAX_LINE_LEN];
  char *tmp;

  fseek(ini, _offset, SEEK_SET);
  while (!(feof(ini))) {
    if (fgets(line, INI_MAX_LINE_LEN, ini) != NULL) {
      line[strlen(line) - 1] = '\0';
      if ((strchr(line, '[') != 0) && (strchr(line, ']') != 0)) {
        return default_val;
      }
      if (strstr(line, item) == line) {
        strtok(line, "=");
        tmp = strtok(NULL, "=");
        while (tmp && (*tmp == ' ')) tmp++;
        if (tmp) {
          if (strncasecmp(tmp, "true", 4) == 0) return true;
          if (strncasecmp(tmp, "yes", 3)  == 0) return true;
        }
        return false;
      }
    }
  }
  return default_val;
}

int ini_get_next(FILE *ini, char *section) {
  char line[INI_MAX_LINE_LEN];
  char *start, *end;

  fseek(ini, _offset, SEEK_SET);
  while (!(feof(ini))) {
    if (fgets(line, INI_MAX_LINE_LEN, ini) != NULL) {
      line[strlen(line) - 1] = '\0';
      if (((start = strchr(line, '[')) != NULL) && 
          ((end = strchr(start, ']')) != NULL)) {
        *end = '\0';
	strcpy(section, ++start);
	_offset = ftell(ini);
 	return 1;
      }
    }
  }
  return 0;
}

int ini_put_item(FILE *ini, const char *item, const char *value) {
  return fprintf(ini, "%s = %s\n", item, value);
}

int ini_put_bool(FILE *ini, const char *item, int value) {
  return fprintf(ini, "%s = %s\n", item, value ? "true" : "false");
}

int ini_put_next(FILE *ini, const char *section) {
  return fprintf(ini, "[%s]\n", section);
}

int ini_put_newline(FILE *ini) {
  return fprintf(ini, "\n");
}
