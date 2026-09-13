#ifndef VALIDATE_INTERNAL_H
#define VALIDATE_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

extern struct g {
  const char *exename;
} g;

int validate_letfloor_text();

#endif
