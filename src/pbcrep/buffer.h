/* invariant:  if a buffer.size==0, then first_frag/last_frag == NULL.
   corollary:  if a buffer.size==0, then the buffer is using no memory. */

typedef struct PBCREP_Buffer PBCREP_Buffer;
typedef struct PBCREP_BufferFragment PBCREP_BufferFragment;

typedef void (*PBCREP_DestroyNotify)(void *destroy_data);

struct PBCREP_BufferFragment
{
  PBCREP_BufferFragment    *next;
  uint8_t                  *buf;
  unsigned                  buf_max_size;	/* allocation size of buf */
  unsigned                  buf_start;	/* offset in buf of valid data */
  unsigned                  buf_length;	/* length of valid data in buf; != 0 */
  
  bool                      is_foreign;
  PBCREP_DestroyNotify      destroy;
  void                     *destroy_data;
};

struct PBCREP_Buffer
{
  unsigned              size;

  PBCREP_BufferFragment    *first_frag;
  PBCREP_BufferFragment    *last_frag;
};

#define PBCREP_BUFFER_INIT		{ 0, NULL, NULL }


void     pbcrep_buffer_init                (PBCREP_Buffer       *buffer);

unsigned pbcrep_buffer_read                (PBCREP_Buffer    *buffer,
                                            unsigned          max_length,
                                            void             *data);
unsigned pbcrep_buffer_peek                (PBCREP_Buffer* buffer,
                                            unsigned          max_length,
                                            void             *data);
int      pbcrep_buffer_discard             (PBCREP_Buffer    *buffer,
                                            unsigned          max_discard);
char    *pbcrep_buffer_read_line           (PBCREP_Buffer    *buffer);

char    *pbcrep_buffer_parse_string0       (PBCREP_Buffer    *buffer);
                        /* Returns first char of buffer, or -1. */
int      pbcrep_buffer_peek_byte           (const PBCREP_Buffer *buffer);
int      pbcrep_buffer_read_byte           (PBCREP_Buffer    *buffer);

uint8_t  pbcrep_buffer_byte_at             (PBCREP_Buffer    *buffer,
                                            unsigned          index);
uint8_t  pbcrep_buffer_last_byte           (PBCREP_Buffer    *buffer);
/* 
 * Appending to the buffer.
 */
void     pbcrep_buffer_append              (PBCREP_Buffer    *buffer, 
                                            unsigned          length,
                                            const void       *data);

PBCREP_INLINE void pbcrep_buffer_append_small(PBCREP_Buffer  *buffer, 
                                            unsigned          length,
                                            const void       *data);
void     pbcrep_buffer_append_string       (PBCREP_Buffer    *buffer, 
                                            const char       *string);
PBCREP_INLINE void pbcrep_buffer_append_byte(PBCREP_Buffer    *buffer, 
                                            uint8_t           byte);
void      pbcrep_buffer_append_byte_f      (PBCREP_Buffer    *buffer, 
                                            uint8_t           byte);
void     pbcrep_buffer_append_repeated_byte(PBCREP_Buffer    *buffer, 
                                            size_t            count,
                                            uint8_t           byte);
#define pbcrep_buffer_append_zeros(buffer, count) \
  pbcrep_buffer_append_repeated_byte ((buffer), 0, (count))


void     pbcrep_buffer_append_string0      (PBCREP_Buffer    *buffer,
                                            const char       *string);

void     pbcrep_buffer_append_foreign      (PBCREP_Buffer    *buffer,
					    unsigned          length,
                                            const void       *data,
					    PBCREP_DestroyNotify destroy,
					    void             *destroy_data);

void     pbcrep_buffer_printf              (PBCREP_Buffer    *buffer,
					    const char       *format,
					    ...) PBCREP_GNUC_PRINTF(2,3);
void     pbcrep_buffer_vprintf             (PBCREP_Buffer    *buffer,
					    const char       *format,
					    va_list           args);

uint8_t  pbcrep_buffer_get_last_byte       (PBCREP_Buffer    *buffer);
uint8_t  pbcrep_buffer_get_byte_at         (PBCREP_Buffer    *buffer,
                                            size_t            idx);


/* --- appending data that will be filled in later --- */
typedef struct {
  PBCREP_Buffer *buffer;
  PBCREP_BufferFragment *fragment;
  unsigned offset;
  unsigned length;
} PBCREP_BufferPlaceholder;

void     pbcrep_buffer_append_placeholder  (PBCREP_Buffer    *buffer,
                                            unsigned          length,
                                            PBCREP_BufferPlaceholder *out);
void     pbcrep_buffer_placeholder_set     (PBCREP_BufferPlaceholder *placeholder,
                                            const void       *data);

/* --- buffer-to-buffer transfers --- */
/* Take all the contents from src and append
 * them to dst, leaving src empty.
 */
unsigned pbcrep_buffer_drain               (PBCREP_Buffer    *dst,
                                            PBCREP_Buffer    *src);

/* Like `drain', but only transfers some of the data. */
unsigned pbcrep_buffer_transfer            (PBCREP_Buffer    *dst,
                                            PBCREP_Buffer    *src,
					    unsigned          max_transfer);

/* --- file-descriptor mucking --- */
int      pbcrep_buffer_writev              (PBCREP_Buffer       *read_from,
                                            int                  fd);
int      pbcrep_buffer_writev_len          (PBCREP_Buffer *read_from,
		                            int                  fd,
		                            unsigned             max_bytes);
/* returns TRUE iff all the data was written.  'read_from' is blank. */
bool     pbcrep_buffer_write_all_to_fd  (PBCREP_Buffer       *read_from,
                                         int                  fd,
                                         PBCREP_Error       **error);
int      pbcrep_buffer_readv            (PBCREP_Buffer       *write_to,
                                         int                  fd);

/* --- deallocating memory used by buffer --- */

/* This deallocates memory used by the buffer-- you are responsible
 * for the allocation and deallocation of the PBCREP_Buffer itself. */
void     pbcrep_buffer_clear               (PBCREP_Buffer    *to_destroy);

/* Same as calling clear/init */
void     pbcrep_buffer_reset               (PBCREP_Buffer    *to_reset);

/* Return a string and clear the buffer;
 * a NUL character is appended. */
char *pbcrep_buffer_empty_to_string (PBCREP_Buffer *buffer);

/* --- iterating through the buffer --- */
/* 'frag_offset_out' is the offset of the returned fragment in the whole
   buffer. */
PBCREP_BufferFragment *pbcrep_buffer_find_fragment (PBCREP_Buffer   *buffer,
                                                    unsigned         offset,
                                                    unsigned        *frag_offset_out);

/* Free all unused buffer fragments. */
void     _pbcrep_buffer_cleanup_recycling_bin ();

typedef enum {
  PBCREP_BUFFER_DUMP_DRAIN = (1<<0),
  PBCREP_BUFFER_DUMP_NO_DRAIN = (1<<1),
  PBCREP_BUFFER_DUMP_FATAL_ERRORS = (1<<2),
  PBCREP_BUFFER_DUMP_LEAVE_PARTIAL = (1<<3),
  PBCREP_BUFFER_DUMP_NO_CREATE_DIRS = (1<<4),
  PBCREP_BUFFER_DUMP_EXECUTABLE = (1<<5),
} PBCREP_BufferDumpFlags;

bool     pbcrep_buffer_dump (PBCREP_Buffer          *buffer,
                             const char             *filename,
                             PBCREP_BufferDumpFlags  flags,
                             PBCREP_Error          **error);
                          

/* misc */
int pbcrep_buffer_index_of(PBCREP_Buffer *buffer, char char_to_find);

unsigned pbcrep_buffer_fragment_peek (PBCREP_BufferFragment *fragment,
                                   unsigned           offset,
                                   unsigned           length,
                                   void              *buf);
bool pbcrep_buffer_fragment_advance (PBCREP_BufferFragment **frag_inout,
                                     unsigned           *offset_inout,
                                     unsigned            skip);

/* HACKS */
/* NOTE: the buffer is INVALID after this call, since no empty
   fragments are allowed.  You MUST deal with this if you do 
   not actually add data to the buffer */
void pbcrep_buffer_append_empty_fragment (PBCREP_Buffer *buffer);

void pbcrep_buffer_maybe_remove_empty_fragment (PBCREP_Buffer *buffer);

/* a way to delete the fragment from pbcrep_buffer_append_empty_fragment() */
void pbcrep_buffer_fragment_free (PBCREP_BufferFragment *fragment);


#if PBCREP_CAN_INLINE || defined(PBCREP_IMPLEMENT_INLINES)
PBCREP_INLINE void
pbcrep_buffer_append_small(PBCREP_Buffer    *buffer, 
                           unsigned          length,
                           const void       *data)
{
  PBCREP_BufferFragment *f = buffer->last_frag;
  if (f != NULL
   && !f->is_foreign
   && f->buf_start + f->buf_length + length <= f->buf_max_size)
    {
      uint8_t *dst = f->buf + (f->buf_start + f->buf_length);
      const uint8_t *src = data;
      f->buf_length += length;
      buffer->size += length;
      while (length--)
        *dst++ = *src++;
    }
  else
    pbcrep_buffer_append (buffer, length, data);
}
PBCREP_INLINE void
pbcrep_buffer_append_byte(PBCREP_Buffer    *buffer, 
                          uint8_t           byte)
{
  PBCREP_BufferFragment *f = buffer->last_frag;
  if (f != NULL
   && !f->is_foreign
   && f->buf_start + f->buf_length < f->buf_max_size)
    {
      f->buf[f->buf_start + f->buf_length] = byte;
      ++(f->buf_length);
      buffer->size += 1;
    }
  else
    pbcrep_buffer_append (buffer, 1, &byte);
}

#endif
