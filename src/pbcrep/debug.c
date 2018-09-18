//TODO add state enum to header.

#include "../pbcrep.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if HAS_BACKTRACE && defined(HAVE_EXECINFO_H)
#  include <execinfo.h>
#endif

#include "dsk-list-macros.h"

// Provide replacements for malloc/free that do a lot of error checking.
//
//
// Pad the allocation to check for underruns, overruns, non-initialized memory, etc.
//
// Implementation notes:
//    * do not use assert(), instead use explicit checks, since
//      this entire module is optional, and so we want
//      it to be co-usable with -DNDEBUG.
//


// magic values - these are just chosen to be unlikely to occur
// normal allocations.
#define HEADER_MAGIC           0x3451af33defa318f


typedef struct PBCREP_DebugHeader PBCREP_DebugHeader;
struct PBCREP_DebugHeader
{
  size_t alloc_size;
  size_t n_backtrace_levels;
  PBCREP_DebugHeader *prev, *next;
  uint64_t magic;
};

//
// There's no need for a footer structure,
// because C can't really express the situation well.
//
// But, if alloc_size is not a multiple of 8, then we will
// write PARTIAL_FOOTER_MAGIC to the remainder of that 8-byte piece.
//
// FOOTER_MAGIC will be written to the aligned memory after that.
//
#define PARTIAL_FOOTER_MAGIC   {0xaa,0x9d,0xde,0xa8,0xfa,0x04,0x35,0x7f}
#define FOOTER_MAGIC           0x4a25a039def8818d
                               //0123456789012345

#define PADDED_SIZE_FROM_SIZE(s)    (((s) + 7) / 8 * 8)

#define ALLOC_BYTE      0xde
#define DEAD_BYTE       0xed
#define FREED_BYTE      0xef

#if HAS_BACKTRACE
#define BACKTRACE_MAX_SIZE     8
#else
#define BACKTRACE_MAX_SIZE     0
#endif

static PBCREP_DebugHeader *first_alloc = NULL;
static PBCREP_DebugHeader *last_alloc = NULL;

static const uint8_t partial_footer_magic[8] = PARTIAL_FOOTER_MAGIC;

static PBCREP_DebugHeader *free_queue_first = NULL;
static PBCREP_DebugHeader *free_queue_last = NULL;
static size_t free_queue_size = 0;
static size_t max_free_queue_size = 128;

#define GET_ALLOC_LIST()    \
        PBCREP_DebugHeader *, first_alloc, last_alloc, prev, next
#define GET_HOLDING_FREE_LIST()    \
        PBCREP_DebugHeader *, free_queue_first, free_queue_last, prev, next

static void *
debug_malloc (size_t s)
{
  size_t padded_size = PADDED_SIZE_FROM_SIZE(s);
  size_t total_size = BACKTRACE_MAX_SIZE * sizeof(void*)
                    + sizeof (PBCREP_DebugHeader)
                    + padded_size
                    + sizeof(uint64_t);         // post-padding footer
  PBCREP_DebugHeader *h = (PBCREP_DebugHeader *) ((void **) malloc (total_size) + BACKTRACE_MAX_SIZE);
  h->alloc_size = s;
#if HAS_BACKTRACE
  h->n_backtrace_levels = backtrace ((void **)h - BACKTRACE_MAX_SIZE,
                                     BACKTRACE_MAX_SIZE);
#else
  h->n_backtrace_levels = 0;
#endif

  // append to allocation list
  DSK_LIST_APPEND (GET_ALLOC_LIST(), h);

  // header magic
  h->magic = HEADER_MAGIC;
  
  char *rv = (char*)(h + 1);

  // footer magic
  memcpy (rv + s, partial_footer_magic, padded_size - s);
  * (uint64_t *) (rv + padded_size) = FOOTER_MAGIC;

  // fill allocation
  memset (rv, 0xed, s);

  return rv;
}

static bool
is_memset (const void *mem, char c, size_t size)
{
  const char *m = mem;
  while (size--)
    if (*m++ != c)
      return false;
  return true;
}

static void
debug_fail (const char *format, ...)
{
  va_list args;
  va_start (args, format);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end (args);
  abort();
}


static void
debug_free (void *ptr)
{
  // Obtain a pointer to the header that precedes this allocation.
  PBCREP_DebugHeader *header = (PBCREP_DebugHeader*)ptr - 1;

  if (header->magic != HEADER_MAGIC)
    debug_fail ("free of %p: header corrupt", ptr);

  size_t size = header->alloc_size;
  size_t padded_size = PADDED_SIZE_FROM_SIZE(size);
  size_t pad_length = padded_size - size;

  if (memcmp ((char*)ptr + size, partial_footer_magic, pad_length) != 0)
    debug_fail ("free of %p: partial-footer magic bad", ptr);
  if (*(uint64_t*)((char*)ptr + padded_size) != FOOTER_MAGIC)
    debug_fail ("free of %p: footer magic bad", ptr);

  // remove from allocation list
  DSK_LIST_REMOVE (GET_ALLOC_LIST(), header);

  // add to free-queue
  DSK_LIST_APPEND (GET_HOLDING_FREE_LIST(), header);
  free_queue_size++;

  // fill with bad-byte
  memset (header + 1, FREED_BYTE, header->alloc_size);

  // remove oldest element from free-queue
  while (free_queue_size > max_free_queue_size)
    {
      // remove first queue member
      assert (free_queue_first != NULL);
      
      PBCREP_DebugHeader *kheader = free_queue_first;
      DSK_LIST_REMOVE_FIRST(GET_HOLDING_FREE_LIST());
      free_queue_size -= 1;

      // verify that freed member is full of FREED_BYTE
      void *free_ptr = kheader + 1;
      if (!is_memset (free_ptr, FREED_BYTE, kheader->alloc_size))
        debug_fail ("allocation %p modified after free", free_ptr);

      // fill with DEAD_BYTE
      memset (free_ptr, DEAD_BYTE, kheader->alloc_size);
      // TODO: also destroy kheader and footer

      // actually liberate the memory
      free (((void **) kheader) - BACKTRACE_MAX_SIZE);
    }
}

static void *
debug_realloc (void *ptr, size_t n)
{
  void *rv = n == 0 ? NULL : debug_malloc (n);
  if (ptr != NULL)
    {
      PBCREP_DebugHeader *header = (PBCREP_DebugHeader*)ptr - 1;
      size_t min_size = header->alloc_size < n ? header->alloc_size : n;
      memcpy (rv, ptr, min_size);
    }
  debug_free (ptr);
  return rv;
}

// set malloc/free for debugging.
void pbcrep_setup_debug_allocator(void)
{
  pbcrep_malloc = debug_malloc;
  pbcrep_free = debug_free;
  pbcrep_realloc = debug_realloc;
}
