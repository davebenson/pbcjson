#include "../pbcrep.h"
#include <stdlib.h>

void *(*pbcrep_malloc) (size_t size) = malloc;
void  (*pbcrep_free)   (void *allocation) = free;
void *(*pbcrep_realloc) (void *old_alloc, size_t new_size) = realloc;
