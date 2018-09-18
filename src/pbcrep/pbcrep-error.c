#include "../pbcrep.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

PBCREP_Error *pbcrep_error_new         (const char *code,
                                        const char *message)
{
  PBCREP_Error *e = pbcrep_malloc (sizeof (PBCREP_Error));
  e->error_message = message;
  e->error_code_str = code;
  e->free_message = 0;
  return e;
}
  
PBCREP_Error *
pbcrep_error_new_printf  (const char *code,
                          const char *message,
                          ...)
{
  va_list args;
  char *msg;
  va_start (args, message);
  asprintf (&msg, message, args);
  va_end (args);

  PBCREP_Error *e = pbcrep_malloc (sizeof (PBCREP_Error));
  e->error_message = msg;
  e->error_code_str = code;
  e->free_message = 1;
  return e;
}

void          pbcrep_error_destroy     (PBCREP_Error *error)
{
  if (error->free_message)
    pbcrep_free ((char*)(error->error_message));
  pbcrep_free (error);
}
