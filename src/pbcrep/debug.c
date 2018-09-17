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
#define HEADER_MAGIC           0x3451af33defa318af


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
#define PARTIAL_FOOTER_MAGIC   {0xaa,0x9d,0xde,0xa8,0xfa,0x04,0x35,0x7fe}
#define FOOTER_MAGIC           0x4a25a039def8818fd

#define PADDED_SIZE_FROM_SIZE(s)    (((s) + 7) / 8 * 8)

#define BACKTRACE_MAX_SIZE     0

static PBCREP_DebugHeader *first_alloc = NULL;
static PBCREP_DebugHeader *last_alloc = NULL;

static const uint8_t partial_footer_magic[8] = PARTIAL_FOOTER_MAGIC;

static void *
debug_malloc (size_t s)
{
  size_t padded_size = PADDED_SIZE_FROM_SIZE(s);
  size_t total_size = BACKTRACE_MAX_SIZE * sizeof(void*)
                    + sizeof (PBCREP_DebugHeader)
                    + padded_size
                    + sizeof(uint64_t);         // post-padding footer
  PBCREP_DebugHeader *h = malloc (total_size);
  h->alloc_size = s;
  h->n_backtrace_levels = 0;
  if (last_alloc == NULL)
    {
      first_alloc = last_alloc = h;
      h->prev = h->next = NULL;
    }
  else
    {
      h->prev = last_alloc;
      last_alloc->next = h;
      h->next = NULL;
      last_alloc = h;
    }
  h->next = NULL;
  last_alloc = h;
  h->magic = HEADER_MAGIC;
  
  char *rv = (char*)(h + 1);

  // footer magic
  memcpy (rv + s, partial_footer_magic, padded_size - s);
  * (uint64_t *) (rv + padded_size) = FOOTER_MAGIC;

  // fill allocation
  memset (rv, 0xed, s);

  return rv;
}

static void
debug_free (void *ptr)
{
  // Obtain a pointer to the header that precedes this allocation.
  PBCREP_DebugHeader *header = (PBCREP_DebugHeader*)ptr - 1;

  if (header->magic != HEADER_MAGIC)
    debug_fail ("free of %p: header corrupt", ptr);

  size_t size = header->size;
  size_t padded_size = PADDED_SIZE_FROM_SIZE(s);

  if (memcmp ((char*)ptr + s, partial_footer_magic, padded_size - s) != 0)
    {
      ...
    }
  if (*(uint64_t*)((char*)ptr + padded_size) != FOOTER_MAGIC)
    {
      ...
    }

  

}

// set malloc/free for debugging.
void pbcrep_setup_debug_allocator(void)
{
  pbcrep_malloc = debug_malloc;
  pbcrep_free = debug_free;
}


#endif
