#include "debug.h"
#include <stdlib.h>
#include <stdio.h>

void
debug_panic (const char *file, int line, const char *function,
             const char *message, ...)
{
  printf ("PANIC at %s:%d in %s()\n", file, line, function);
  exit (1);
}
