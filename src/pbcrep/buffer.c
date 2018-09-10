/* PBCREP - a library to write servers
    Copyright (C) 1999-2000 Dave Benson

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

    Contact:
        daveb@ffem.org <Dave Benson>
*/

/* Free blocks to hold around to avoid repeated mallocs... */
#define MAX_RECYCLED		16

/* Size of allocations to make. */
#define BUF_CHUNK_SIZE		32768

/* Max fragments in the iovector to writev. */
#define MAX_FRAGMENTS_TO_WRITE	16

/* This causes fragments not to be transferred from buffer to buffer,
 * and not to be allocated in pools.  The result is that stack-trace
 * based debug-allocators work much better with this on.
 *
 * On the other hand, this can mask over some abuses (eg stack-based
 * foreign buffer fragment bugs) so we disable it by default.
 */ 
#define PBCREP_DEBUG_BUFFER_ALLOCATIONS	(0 && PBCREP_DEBUG)

#include <alloca.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>      /* for vsnprintf() */
#include <errno.h>
#include "pbcrep.h"

/* --- PBCREP_BufferFragment implementation --- */
static inline int 
pbcrep_buffer_fragment_avail (PBCREP_BufferFragment *fragment)
{
  return fragment->buf_max_size - fragment->buf_start - fragment->buf_length;
}
static inline uint8_t *
pbcrep_buffer_fragment_start (PBCREP_BufferFragment *fragment)
{
  return fragment->buf + fragment->buf_start;
}
static inline uint8_t *
pbcrep_buffer_fragment_end (PBCREP_BufferFragment *fragment)
{
  return fragment->buf + fragment->buf_start + fragment->buf_length;
}

/* --- PBCREP_BufferFragment recycling --- */
#if !PBCREP_DEBUG_BUFFER_ALLOCATIONS
static int num_recycled = 0;
static PBCREP_BufferFragment* recycling_stack = 0;

#endif

static PBCREP_BufferFragment *
new_native_fragment()
{
  PBCREP_BufferFragment *fragment;
#if PBCREP_DEBUG_BUFFER_ALLOCATIONS
  fragment = (PBCREP_BufferFragment *) pbcrep_malloc (BUF_CHUNK_SIZE);
  fragment->buf_max_size = BUF_CHUNK_SIZE - sizeof (PBCREP_BufferFragment);
#else  /* optimized (?) */
  if (recycling_stack)
    {
      fragment = recycling_stack;
      recycling_stack = recycling_stack->next;
      num_recycled--;
    }
  else
    {
      fragment = (PBCREP_BufferFragment *) pbcrep_malloc (BUF_CHUNK_SIZE);
      fragment->buf_max_size = BUF_CHUNK_SIZE - sizeof (PBCREP_BufferFragment);
    }
#endif	/* !PBCREP_DEBUG_BUFFER_ALLOCATIONS */
  fragment->buf_start = fragment->buf_length = 0;
  fragment->next = 0;
  fragment->buf = (uint8_t *) (fragment + 1);
  fragment->is_foreign = 0;
  return fragment;
}

static PBCREP_BufferFragment *
new_foreign_fragment (unsigned             length,
                      const void          *ptr,
		      PBCREP_DestroyNotify     destroy,
		      void                *ddata)
{
  PBCREP_BufferFragment *fragment;
  fragment = PBCREP_NEW (PBCREP_BufferFragment);
  fragment->is_foreign = 1;
  fragment->buf_start = 0;
  fragment->buf_length = length;
  fragment->buf_max_size = length;
  fragment->next = NULL;
  fragment->buf = (uint8_t *) ptr;
  fragment->destroy = destroy;
  fragment->destroy_data = ddata;
  return fragment;
}

#if PBCREP_DEBUG_BUFFER_ALLOCATIONS
#define recycle(fragment) do{ \
    if (fragment->is_foreign && fragment->destroy != NULL) \
      fragment->destroy (fragment->destroy_data); \
    pbcrep_free (fragment); \
   }while(0)
#else	/* optimized (?) */
static void
recycle(PBCREP_BufferFragment* fragment)
{
  if (fragment->is_foreign)
    {
      if (fragment->destroy)
        fragment->destroy (fragment->destroy_data);
      pbcrep_free (fragment);
      return;
    }
#if defined(MAX_RECYCLED)
  if (num_recycled >= MAX_RECYCLED)
    {
      pbcrep_free (fragment);
      return;
    }
#endif
  fragment->next = recycling_stack;
  recycling_stack = fragment;
  num_recycled++;
}
#endif	/* !PBCREP_DEBUG_BUFFER_ALLOCATIONS */

/* --- Global public methods --- */
/**
 * _pbcrep_buffer_cleanup_recycling_bin:
 * 
 * Free unused buffer fragments.  (Normally some are
 * kept around to reduce strain on the global allocator.)
 */
void
_pbcrep_buffer_cleanup_recycling_bin ()
{
#if !PBCREP_DEBUG_BUFFER_ALLOCATIONS
  while (recycling_stack != NULL)
    {
      PBCREP_BufferFragment *next;
      next = recycling_stack->next;
      pbcrep_free (recycling_stack);
      recycling_stack = next;
    }
  num_recycled = 0;
#endif
}
      
/* --- Public methods --- */
/**
 * pbcrep_buffer_construct:
 * @buffer: buffer to initialize (as empty).
 *
 * Construct an empty buffer out of raw memory.
 * (This is equivalent to filling the buffer with 0s)
 */
void
pbcrep_buffer_init(PBCREP_Buffer *buffer)
{
  buffer->first_frag = buffer->last_frag = NULL;
  buffer->size = 0;
}

#if defined(PBCREP_DEBUG) || PBCREP_DEBUG_BUFFER_ALLOCATIONS
static inline pbcrep_boolean
verify_buffer (const PBCREP_Buffer *buffer)
{
  const PBCREP_BufferFragment *fragment;
  unsigned total = 0;
  for (fragment = buffer->first_frag; fragment != NULL; fragment = fragment->next)
    {
      if (fragment->buf_length == 0)
        return PBCREP_FALSE;
      total += fragment->buf_length;
    }
  return total == buffer->size;
}
#define CHECK_INTEGRITY(buffer)	pbcrep_assert (verify_buffer (buffer))
#else
#define CHECK_INTEGRITY(buffer)
#endif

/**
 * pbcrep_buffer_append:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @data: binary data to add to the buffer.
 * @length: length of @data to add to the buffer.
 *
 * Append data into the buffer.
 */
void
pbcrep_buffer_append(PBCREP_Buffer    *buffer,
		  unsigned      length,
                  const void * data)
{
  CHECK_INTEGRITY (buffer);
  buffer->size += length;
  while (length > 0)
    {
      unsigned avail;
      if (!buffer->last_frag)
	{
	  buffer->last_frag = buffer->first_frag = new_native_fragment ();
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	}
      else
	{
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	  if (avail <= 0)
	    {
	      buffer->last_frag->next = new_native_fragment ();
	      avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	      buffer->last_frag = buffer->last_frag->next;
	    }
	}
      if (avail > length)
	avail = length;
      memcpy (pbcrep_buffer_fragment_end (buffer->last_frag), data, avail);
      data = (const char *) data + avail;
      length -= avail;
      buffer->last_frag->buf_length += avail;
    }
  CHECK_INTEGRITY (buffer);
}

void
pbcrep_buffer_append_repeated_byte (PBCREP_Buffer    *buffer, 
                                 size_t        count,
                                 uint8_t       byte)
{
  CHECK_INTEGRITY (buffer);
  buffer->size += count;
  while (count > 0)
    {
      unsigned avail;
      if (!buffer->last_frag)
	{
	  buffer->last_frag = buffer->first_frag = new_native_fragment ();
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	}
      else
	{
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	  if (avail <= 0)
	    {
	      buffer->last_frag->next = new_native_fragment ();
	      avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	      buffer->last_frag = buffer->last_frag->next;
	    }
	}
      if (avail > count)
	avail = count;
      memset (pbcrep_buffer_fragment_end (buffer->last_frag), byte, avail);
      count -= avail;
      buffer->last_frag->buf_length += avail;
    }
  CHECK_INTEGRITY (buffer);
}

#if 0
void
pbcrep_buffer_append_repeated_data (PBCREP_Buffer    *buffer, 
                                 const void * data_to_repeat,
                                 size_t         data_length,
                                 size_t         count)
{
  ...
}
#endif

/**
 * pbcrep_buffer_append_string:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @string: NUL-terminated string to append to the buffer.
 *  The NUL is not appended.
 *
 * Append a string to the buffer.
 */
void
pbcrep_buffer_append_string(PBCREP_Buffer  *buffer,
                         const char *string)
{
  pbcrep_return_if_fail (string != NULL, "string must be non-null");
  pbcrep_buffer_append (buffer, strlen (string), string);
}

/**
 * pbcrep_buffer_append_byte:
 * @buffer: the buffer to add the byte to.
 * @character: the byte to add to the buffer.
 *
 * Append a byte to a buffer.
 */
void
pbcrep_buffer_append_byte_f(PBCREP_Buffer *buffer,
		       uint8_t    byte)
{
  pbcrep_buffer_append_byte (buffer, byte);
}

/**
 * pbcrep_buffer_append_string0:
 * @buffer: the buffer to add data to.  Data is put at the end of the buffer.
 * @string: NUL-terminated string to append to the buffer;
 *  NUL is appended.
 *
 * Append a NUL-terminated string to the buffer.  The NUL is appended.
 */
void
pbcrep_buffer_append_string0      (PBCREP_Buffer    *buffer,
				const char   *string)
{
  pbcrep_buffer_append (buffer, strlen (string) + 1, string);
}

/**
 * pbcrep_buffer_read:
 * @buffer: the buffer to read data from.
 * @data: buffer to fill with up to @max_length bytes of data.
 * @max_length: maximum number of bytes to read.
 *
 * Removes up to @max_length data from the beginning of the buffer,
 * and writes it to @data.  The number of bytes actually read
 * is returned.
 *
 * returns: number of bytes transferred.
 */
unsigned
pbcrep_buffer_read(PBCREP_Buffer    *buffer,
		unsigned      max_length,
                void         *data)
{
  unsigned rv = 0;
  unsigned orig_max_length = max_length;
  CHECK_INTEGRITY (buffer);
  while (max_length > 0 && buffer->first_frag)
    {
      PBCREP_BufferFragment *first = buffer->first_frag;
      if (first->buf_length <= max_length)
	{
	  memcpy (data, pbcrep_buffer_fragment_start (first), first->buf_length);
	  rv += first->buf_length;
	  data = (char *) data + first->buf_length;
	  max_length -= first->buf_length;
	  buffer->first_frag = first->next;
	  if (!buffer->first_frag)
	    buffer->last_frag = NULL;
	  recycle (first);
	}
      else
	{
	  memcpy (data, pbcrep_buffer_fragment_start (first), max_length);
	  rv += max_length;
	  first->buf_length -= max_length;
	  first->buf_start += max_length;
	  data = (char *) data + max_length;
	  max_length = 0;
	}
    }
  buffer->size -= rv;
  pbcrep_assert (rv == orig_max_length || buffer->size == 0);
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * pbcrep_buffer_peek:
 * @buffer: the buffer to peek data from the front of.
 *    This buffer is unchanged by the operation.
 * @data: buffer to fill with up to @max_length bytes of data.
 * @max_length: maximum number of bytes to peek.
 *
 * Copies up to @max_length data from the beginning of the buffer,
 * and writes it to @data.  The number of bytes actually copied
 * is returned.
 *
 * This function is just like pbcrep_buffer_read() except that the 
 * data is not removed from the buffer.
 *
 * returns: number of bytes copied into data.
 */
unsigned
pbcrep_buffer_peek     (const PBCREP_Buffer *buffer,
		     unsigned         max_length,
                     void            *data)
{
  int rv = 0;
  PBCREP_BufferFragment *fragment = (PBCREP_BufferFragment *) buffer->first_frag;
  CHECK_INTEGRITY (buffer);
  while (max_length > 0 && fragment)
    {
      if (fragment->buf_length <= max_length)
	{
	  memcpy (data, pbcrep_buffer_fragment_start (fragment), fragment->buf_length);
	  rv += fragment->buf_length;
	  data = (char *) data + fragment->buf_length;
	  max_length -= fragment->buf_length;
	  fragment = fragment->next;
	}
      else
	{
	  memcpy (data, pbcrep_buffer_fragment_start (fragment), max_length);
	  rv += max_length;
	  data = (char *) data + max_length;
	  max_length = 0;
	}
    }
  return rv;
}

/**
 * pbcrep_buffer_read_line:
 * @buffer: buffer to read a line from.
 *
 * Parse a newline (\n) terminated line from
 * buffer and return it as a newly allocated string.
 * The newline is changed to a NUL character.
 *
 * If the buffer does not contain a newline, then NULL is returned.
 *
 * returns: a newly allocated NUL-terminated string, or NULL.
 */
char *
pbcrep_buffer_read_line(PBCREP_Buffer *buffer)
{
  int length = 0;
  char *rv;
  PBCREP_BufferFragment *at;
  int newline_length;
  CHECK_INTEGRITY (buffer);
  for (at = buffer->first_frag; at; at = at->next)
    {
      uint8_t *start = pbcrep_buffer_fragment_start (at);
      uint8_t *got;
      got = memchr (start, '\n', at->buf_length);
      if (got)
	{
	  length += got - start;
	  break;
	}
      length += at->buf_length;
    }
  if (at == NULL)
    return NULL;
  rv = pbcrep_malloc (length + 1);
  /* If we found a newline, read it out, truncating
   * it with NUL before we return from the function... */
  if (at)
    newline_length = 1;
  else
    newline_length = 0;
  pbcrep_buffer_read (buffer, length + newline_length, rv);
  rv[length] = 0;
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * pbcrep_buffer_parse_string0:
 * @buffer: buffer to read a line from.
 *
 * Parse a NUL-terminated line from
 * buffer and return it as a newly allocated string.
 *
 * If the buffer does not contain a newline, then NULL is returned.
 *
 * returns: a newly allocated NUL-terminated string, or NULL.
 */
char *
pbcrep_buffer_parse_string0(PBCREP_Buffer *buffer)
{
  int index0 = pbcrep_buffer_index_of (buffer, '\0');
  char *rv;
  if (index0 < 0)
    return NULL;
  rv = pbcrep_malloc (index0 + 1);
  pbcrep_buffer_read (buffer, index0 + 1, rv);
  return rv;
}

/**
 * pbcrep_buffer_peek_byte:
 * @buffer: buffer to peek a single byte from.
 *
 * Get the first byte in the buffer as a positive or 0 number.
 * If the buffer is empty, -1 is returned.
 * The buffer is unchanged.
 *
 * returns: an unsigned character or -1.
 */
int
pbcrep_buffer_peek_byte(const PBCREP_Buffer *buffer)
{
  if (buffer->first_frag == NULL)
    return -1;
  else
    return * pbcrep_buffer_fragment_start ((PBCREP_BufferFragment *) buffer->first_frag);
}

/**
 * pbcrep_buffer_read_byte:
 * @buffer: buffer to read a single byte from.
 *
 * Get the first byte in the buffer as a positive or 0 number,
 * and remove the character from the buffer.
 * If the buffer is empty, -1 is returned.
 *
 * returns: an unsigned character or -1.
 */
int
pbcrep_buffer_read_byte (PBCREP_Buffer *buffer)
{
  uint8_t c;
  return (pbcrep_buffer_read (buffer, 1, &c) == 0) ? -1 : c;
}

/**
 * pbcrep_buffer_discard:
 * @buffer: the buffer to discard data from.
 * @max_discard: maximum number of bytes to discard.
 *
 * Removes up to @max_discard data from the beginning of the buffer,
 * and returns the number of bytes actually discarded.
 *
 * returns: number of bytes discarded.
 */
int
pbcrep_buffer_discard(PBCREP_Buffer *buffer,
                   unsigned      max_discard)
{
  int rv = 0;
  CHECK_INTEGRITY (buffer);
  while (max_discard > 0 && buffer->first_frag)
    {
      PBCREP_BufferFragment *first = buffer->first_frag;
      if (first->buf_length <= max_discard)
	{
	  rv += first->buf_length;
	  max_discard -= first->buf_length;
	  buffer->first_frag = first->next;
	  if (!buffer->first_frag)
	    buffer->last_frag = NULL;
	  recycle (first);
	}
      else
	{
	  rv += max_discard;
	  first->buf_length -= max_discard;
	  first->buf_start += max_discard;
	  max_discard = 0;
	}
    }
  buffer->size -= rv;
  CHECK_INTEGRITY (buffer);
  return rv;
}

/**
 * pbcrep_buffer_writev:
 * @read_from: buffer to take data from.
 * @fd: file-descriptor to write data to.
 *
 * Writes as much data as possible to the
 * given file-descriptor using the writev(2)
 * function to deal with multiple fragments
 * efficiently, where available.
 *
 * returns: the number of bytes transferred,
 * or -1 on a write error (consult errno).
 */
int
pbcrep_buffer_writev (PBCREP_Buffer       *read_from,
		   int              fd)
{
  int rv;
  struct iovec *iov;
  int nfrag, i;
  PBCREP_BufferFragment *frag_at = read_from->first_frag;
  CHECK_INTEGRITY (read_from);
  for (nfrag = 0; frag_at != NULL
#ifdef MAX_FRAGMENTS_TO_WRITE
       && nfrag < MAX_FRAGMENTS_TO_WRITE
#endif
       ; nfrag++)
    frag_at = frag_at->next;
  iov = (struct iovec *) alloca (sizeof (struct iovec) * nfrag);
  frag_at = read_from->first_frag;
  for (i = 0; i < nfrag; i++)
    {
      iov[i].iov_len = frag_at->buf_length;
      iov[i].iov_base = pbcrep_buffer_fragment_start (frag_at);
      frag_at = frag_at->next;
    }
  rv = writev (fd, iov, nfrag);
  if (rv < 0 && (errno == EINTR || errno == EAGAIN))
    return 0;
  if (rv <= 0)
    return rv;
  pbcrep_buffer_discard (read_from, rv);
  return rv;
}

/**
 * pbcrep_buffer_writev_len:
 * @read_from: buffer to take data from.
 * @fd: file-descriptor to write data to.
 * @max_bytes: maximum number of bytes to write.
 *
 * Writes up to max_bytes bytes to the
 * given file-descriptor using the writev(2)
 * function to deal with multiple fragments
 * efficiently, where available.
 *
 * returns: the number of bytes transferred,
 * or -1 on a write error (consult errno).
 */
int
pbcrep_buffer_writev_len (PBCREP_Buffer *read_from,
		       int        fd,
		       unsigned      max_bytes)
{
  int rv;
  struct iovec *iov;
  int nfrag, i;
  unsigned bytes;
  PBCREP_BufferFragment *frag_at = read_from->first_frag;
  CHECK_INTEGRITY (read_from);
  for (nfrag = 0, bytes = 0; frag_at != NULL && bytes < max_bytes
#ifdef MAX_FRAGMENTS_TO_WRITE
       && nfrag < MAX_FRAGMENTS_TO_WRITE
#endif
       ; nfrag++)
    {
      bytes += frag_at->buf_length;
      frag_at = frag_at->next;
    }
  iov = (struct iovec *) alloca (sizeof (struct iovec) * nfrag);
  frag_at = read_from->first_frag;
  for (bytes = max_bytes, i = 0; i < nfrag && bytes > 0; i++)
    {
      unsigned frag_bytes = frag_at->buf_length;
      if (frag_bytes > bytes)
        frag_bytes = bytes;
      iov[i].iov_len = frag_bytes;
      iov[i].iov_base = pbcrep_buffer_fragment_start (frag_at);
      frag_at = frag_at->next;
      bytes -= frag_bytes;
    }
  rv = writev (fd, iov, i);
  if (rv < 0 && (errno == EINTR || errno == EAGAIN))
    return 0;
  if (rv <= 0)
    return rv;
  pbcrep_buffer_discard (read_from, rv);
  return rv;
}

pbcrep_boolean
pbcrep_buffer_write_all_to_fd (PBCREP_Buffer       *read_from,
		            int              fd,
                            PBCREP_Error       **error)
{
  while (read_from->size > 0)
    {
      if (pbcrep_buffer_writev (read_from, fd) < 0)
        {
          pbcrep_set_error (error, "error writing to fd %d: %s",
                         fd, strerror (errno));
          return PBCREP_FALSE;
        }
    }
  return PBCREP_TRUE;
}

/**
 * pbcrep_buffer_read_in_fd:
 * @write_to: buffer to append data to.
 * @read_from: file-descriptor to read data from.
 *
 * Append data into the buffer directly from the
 * given file-descriptor.
 *
 * returns: the number of bytes transferred,
 * or -1 on a read error (consult errno).
 */
/* TODO: zero-copy! */
int
pbcrep_buffer_readv(PBCREP_Buffer *write_to,
                      int        read_from)
{
  char buf[8192];
  int rv = read (read_from, buf, sizeof (buf));
  if (rv < 0)
    return rv;
  pbcrep_buffer_append (write_to, rv, buf);
  return rv;
}

/**
 * pbcrep_buffer_clear:
 * @to_destroy: the buffer to empty.
 *
 * Remove all fragments from a buffer, leaving it empty.
 * The buffer is guaranteed to not to be consuming any resources,
 * but it also is allowed to start using it again.
 */
void
pbcrep_buffer_clear(PBCREP_Buffer *to_destroy)
{
  PBCREP_BufferFragment *at = to_destroy->first_frag;
  CHECK_INTEGRITY (to_destroy);
  while (at)
    {
      PBCREP_BufferFragment *next = at->next;
      recycle (at);
      at = next;
    }
#if PBCREP_DEBUG_BUFFER_ALLOCATIONS
  to_destroy->first_frag = (void*)(size_t)1;
  to_destroy->last_frag = (void*)(size_t)2;
#endif
}

void
pbcrep_buffer_reset(PBCREP_Buffer *to_destroy)
{
  pbcrep_buffer_clear (to_destroy);
  to_destroy->first_frag = to_destroy->last_frag = NULL;
  to_destroy->size = 0;
}


/**
 * pbcrep_buffer_index_of:
 * @buffer: buffer to scan.
 * @char_to_find: a byte to look for.
 *
 * Scans for the first instance of the given character.
 * returns: its index in the buffer, or -1 if the character
 * is not in the buffer.
 */
int
pbcrep_buffer_index_of(PBCREP_Buffer *buffer,
                    char       char_to_find)
{
  PBCREP_BufferFragment *at = buffer->first_frag;
  int rv = 0;
  while (at)
    {
      uint8_t *start = pbcrep_buffer_fragment_start (at);
      uint8_t *saught = memchr (start, char_to_find, at->buf_length);
      if (saught)
	return (saught - start) + rv;
      else
	rv += at->buf_length;
      at = at->next;
    }
  return -1;
}

/**
 * pbcrep_buffer_str_index_of:
 * @buffer: buffer to scan.
 * @str_to_find: a string to look for.
 *
 * Scans for the first instance of the given string.
 * returns: its index in the buffer, or -1 if the string
 * is not in the buffer.
 */
int 
pbcrep_buffer_str_index_of (PBCREP_Buffer *buffer,
                         const char *str_to_find)
{
  PBCREP_BufferFragment *fragment = buffer->first_frag;
  unsigned rv = 0;
  for (fragment = buffer->first_frag; fragment; fragment = fragment->next)
    {
      const uint8_t *frag_at = fragment->buf + fragment->buf_start;
      unsigned frag_rem = fragment->buf_length;
      while (frag_rem > 0)
        {
          PBCREP_BufferFragment *subfrag;
          const uint8_t *subfrag_at;
          unsigned subfrag_rem;
          const char *str_at;
          if (*frag_at != str_to_find[0])
            {
              frag_at++;
              frag_rem--;
              rv++;
              continue;
            }
          subfrag = fragment;
          subfrag_at = frag_at + 1;
          subfrag_rem = frag_rem - 1;
          str_at = str_to_find + 1;
          if (*str_at == '\0')
            return rv;
          while (subfrag != NULL)
            {
              while (subfrag_rem == 0)
                {
                  subfrag = subfrag->next;
                  if (subfrag == NULL)
                    goto bad_guess;
                  subfrag_at = subfrag->buf + subfrag->buf_start;
                  subfrag_rem = subfrag->buf_length;
                }
              while (*str_at != '\0' && subfrag_rem != 0)
                {
                  if (*str_at++ != *subfrag_at++)
                    goto bad_guess;
                  subfrag_rem--;
                }
              if (*str_at == '\0')
                return rv;
            }
bad_guess:
          frag_at++;
          frag_rem--;
          rv++;
        }
    }
  return -1;
}

/**
 * pbcrep_buffer_drain:
 * @dst: buffer to add to.
 * @src: buffer to remove from.
 *
 * Transfer all data from @src to @dst,
 * leaving @src empty.
 *
 * returns: the number of bytes transferred.
 */
#if PBCREP_DEBUG_BUFFER_ALLOCATIONS
unsigned
pbcrep_buffer_drain (PBCREP_Buffer *dst,
		  PBCREP_Buffer *src)
{
  unsigned rv = src->size;
  PBCREP_BufferFragment *fragment;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  for (fragment = src->first_frag; fragment; fragment = fragment->next)
    pbcrep_buffer_append (dst,
                       fragment->buf_length,
                       pbcrep_buffer_fragment_start (fragment));
  pbcrep_buffer_discard (src, src->size);
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#else	/* optimized */
unsigned
pbcrep_buffer_drain (PBCREP_Buffer *dst,
		  PBCREP_Buffer *src)
{
  unsigned rv = src->size;

  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  if (src->first_frag == NULL)
    return rv;

  dst->size += src->size;

  if (dst->last_frag != NULL)
    {
      dst->last_frag->next = src->first_frag;
      dst->last_frag = src->last_frag;
    }
  else
    {
      dst->first_frag = src->first_frag;
      dst->last_frag = src->last_frag;
    }
  src->size = 0;
  src->first_frag = src->last_frag = NULL;
  CHECK_INTEGRITY (dst);
  return rv;
}
#endif

/**
 * pbcrep_buffer_transfer:
 * @dst: place to copy data into.
 * @src: place to read data from.
 * @max_transfer: maximum number of bytes to transfer.
 *
 * Transfer data out of @src and into @dst.
 * Data is removed from @src.  The number of bytes
 * transferred is returned.
 *
 * returns: the number of bytes transferred.
 */
#if PBCREP_DEBUG_BUFFER_ALLOCATIONS
unsigned
pbcrep_buffer_transfer(PBCREP_Buffer *dst,
		    PBCREP_Buffer *src,
		    unsigned max_transfer)
{
  unsigned rv = 0;
  PBCREP_BufferFragment *fragment;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  for (fragment = src->first_frag; fragment && max_transfer > 0; fragment = fragment->next)
    {
      unsigned length = fragment->buf_length;
      if (length >= max_transfer)
        {
          pbcrep_buffer_append (dst, max_transfer, pbcrep_buffer_fragment_start (fragment));
          rv += max_transfer;
          break;
        }
      else
        {
          pbcrep_buffer_append (dst, length, pbcrep_buffer_fragment_start (fragment));
          rv += length;
          max_transfer -= length;
        }
    }
  pbcrep_buffer_discard (src, rv);
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#else	/* optimized */
unsigned
pbcrep_buffer_transfer(PBCREP_Buffer *dst,
		    PBCREP_Buffer *src,
		    unsigned max_transfer)
{
  unsigned rv = 0;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  while (src->first_frag && max_transfer >= src->first_frag->buf_length)
    {
      PBCREP_BufferFragment *fragment = src->first_frag;
      src->first_frag = fragment->next;
      fragment->next = NULL;
      if (src->first_frag == NULL)
	src->last_frag = NULL;

      if (dst->last_frag)
	dst->last_frag->next = fragment;
      else
	dst->first_frag = fragment;
      dst->last_frag = fragment;

      rv += fragment->buf_length;
      max_transfer -= fragment->buf_length;
    }
  dst->size += rv;
  if (src->first_frag && max_transfer)
    {
      PBCREP_BufferFragment *fragment = src->first_frag;
      pbcrep_buffer_append (dst, max_transfer, pbcrep_buffer_fragment_start (fragment));
      fragment->buf_start += max_transfer;
      fragment->buf_length -= max_transfer;
      rv += max_transfer;
    }
  src->size -= rv;
  CHECK_INTEGRITY (dst);
  CHECK_INTEGRITY (src);
  return rv;
}
#endif	/* !PBCREP_DEBUG_BUFFER_ALLOCATIONS */

/* --- foreign data --- */
/**
 * pbcrep_buffer_append_foreign:
 * @buffer: the buffer to append into.
 * @data: the data to append.
 * @length: length of @data.
 * @destroy: optional method to call when the data is no longer needed.
 * @destroy_data: the argument to the destroy method.
 *
 * This function allows data to be placed in a buffer without
 * copying.  It is the callers' responsibility to ensure that
 * @data will remain valid until the destroy method is called.
 * @destroy may be omitted if @data is permanent, for example,
 * if appended a static string into a buffer.
 */
void pbcrep_buffer_append_foreign (PBCREP_Buffer        *buffer,
				unsigned          length,
                                const void *     data,
				PBCREP_DestroyNotify    destroy,
				void *          destroy_data)
{
  PBCREP_BufferFragment *fragment;

  if (length == 0)
    {
      if (destroy)
        destroy (destroy_data);
      return;
    }

  CHECK_INTEGRITY (buffer);

  fragment = new_foreign_fragment (length, data, destroy, destroy_data);
  fragment->next = NULL;

  if (buffer->last_frag == NULL)
    buffer->first_frag = fragment;
  else
    buffer->last_frag->next = fragment;

  buffer->last_frag = fragment;
  buffer->size += length;

  CHECK_INTEGRITY (buffer);
}

/* --- pbcrep_buffer_polystr_index_of implementation --- */
/* Test to see if a sequence of buffer fragments
 * starts with a particular NUL-terminated string.
 */
static pbcrep_boolean
fragment_n_str(PBCREP_BufferFragment   *fragment,
               unsigned                frag_index,
               const char          *string)
{
  unsigned length = strlen (string);
  for (;;)
    {
      unsigned test_len = fragment->buf_length - frag_index;
      if (test_len > length)
        test_len = length;

      if (memcmp (string,
                  pbcrep_buffer_fragment_start (fragment) + frag_index,
                  test_len) != 0)
        return PBCREP_FALSE;

      length -= test_len;
      string += test_len;

      if (length <= 0)
        return PBCREP_TRUE;
      frag_index += test_len;
      if (frag_index >= fragment->buf_length)
        {
          fragment = fragment->next;
          if (fragment == NULL)
            return PBCREP_FALSE;
        }
    }
}

/**
 * pbcrep_buffer_polystr_index_of:
 * @buffer: buffer to scan.
 * @strings: NULL-terminated set of string.
 *
 * Scans for the first instance of any of the strings
 * in the buffer.
 *
 * returns: the index of that instance, or -1 if not found.
 */
int     
pbcrep_buffer_polystr_index_of    (PBCREP_Buffer    *buffer,
                                char        **strings)
{
  uint8_t init_char_map[16];
  int num_strings;
  int num_bits = 0;
  int total_index = 0;
  PBCREP_BufferFragment *fragment;
  memset (init_char_map, 0, sizeof (init_char_map));
  for (num_strings = 0; strings[num_strings] != NULL; num_strings++)
    {
      uint8_t c = strings[num_strings][0];
      uint8_t mask = (1 << (c % 8));
      uint8_t *rack = init_char_map + (c / 8);
      if ((*rack & mask) == 0)
        {
          *rack |= mask;
          num_bits++;
        }
    }
  if (num_bits == 0)
    return 0;
  for (fragment = buffer->first_frag; fragment != NULL; fragment = fragment->next)
    {
      const uint8_t *frag_start;
      const uint8_t *at;
      int remaining = fragment->buf_length;
      frag_start = pbcrep_buffer_fragment_start (fragment);
      at = frag_start;
      while (at != NULL)
        {
          const uint8_t *start = at;
          if (num_bits == 1)
            {
              at = memchr (start, strings[0][0], remaining);
              if (at == NULL)
                remaining = 0;
              else
                remaining -= (at - start);
            }
          else
            {
              while (remaining > 0)
                {
                  uint8_t i = (uint8_t) (*at);
                  if (init_char_map[i / 8] & (1 << (i % 8)))
                    break;
                  remaining--;
                  at++;
                }
              if (remaining == 0)
                at = NULL;
            }

          if (at == NULL)
            break;

          /* Now test each of the strings manually. */
          {
            char **test;
            for (test = strings; *test != NULL; test++)
              {
                if (fragment_n_str(fragment, at - frag_start, *test))
                  return total_index + (at - frag_start);
              }
            at++;
          }
        }
      total_index += fragment->buf_length;
    }
  return -1;
}

void     pbcrep_buffer_printf              (PBCREP_Buffer    *buffer,
					 const char   *format,
					 ...)
{
  va_list args;
  va_start (args, format);
  pbcrep_buffer_vprintf (buffer, format, args);
  va_end (args);
}

void     pbcrep_buffer_vprintf             (PBCREP_Buffer    *buffer,
					 const char   *format,
					 va_list       args)
{
  PBCREP_BufferFragment *frag = buffer->last_frag;
  unsigned rem = 0;
  uint8_t *at;
  size_t req;
  if (frag != NULL)
    {
      rem = pbcrep_buffer_fragment_avail (frag);
      at = pbcrep_buffer_fragment_end (frag);
    }
  if (rem == 0)
    {
      frag = new_native_fragment ();
      rem = pbcrep_buffer_fragment_avail (frag);
      at = pbcrep_buffer_fragment_end (frag);
    }
  {
    va_list tmp;
    va_copy (tmp, args);
    req = vsnprintf ((char*)at, rem, format, tmp);
    va_end (tmp);
  }
  if (req == 0)
    {
      if (frag != buffer->last_frag)
        recycle (frag);
      return;
    }
  if (req <= rem)
    {
      frag->buf_length += req;
      buffer->size += req;
      if (frag != buffer->last_frag)
        {
          if (buffer->last_frag)
            buffer->last_frag->next = frag;
          else
            buffer->first_frag = frag;
          buffer->last_frag = frag;
        }
    }
  else
    {
      char *slab;

      if (frag != buffer->last_frag)
        recycle (frag);

      slab = pbcrep_malloc (req + 1);
      vsnprintf (slab, req + 1, format, args);

      if (req - rem < BUF_CHUNK_SIZE)
        {
          pbcrep_buffer_append (buffer, req, slab);
          pbcrep_free (slab);
        }
      else
        pbcrep_buffer_append_foreign (buffer, req, slab, pbcrep_free, slab);
    }
}


#if 0
/* --- PBCREP_BufferIterator --- */

/**
 * pbcrep_buffer_iterator_construct:
 * @iterator: to initialize.
 * @to_iterate: the buffer to walk through.
 *
 * Initialize a new #PBCREP_BufferIterator.
 */
void 
pbcrep_buffer_iterator_construct (PBCREP_BufferIterator *iterator,
			       PBCREP_Buffer         *to_iterate)
{
  iterator->fragment = to_iterate->first_frag;
  if (iterator->fragment != NULL)
    {
      iterator->in_cur = 0;
      iterator->cur_data = (uint8_t*)pbcrep_buffer_fragment_start (iterator->fragment);
      iterator->cur_length = iterator->fragment->buf_length;
    }
  else
    {
      iterator->in_cur = 0;
      iterator->cur_data = NULL;
      iterator->cur_length = 0;
    }
  iterator->offset = 0;
}

/**
 * pbcrep_buffer_iterator_peek:
 * @iterator: to peek data from.
 * @out: to copy data into.
 * @max_length: maximum number of bytes to write to @out.
 *
 * Peek data from the current position of an iterator.
 * The iterator's position is not changed.
 *
 * returns: number of bytes peeked into @out.
 */
unsigned
pbcrep_buffer_iterator_peek      (PBCREP_BufferIterator *iterator,
			       void *           out,
			       unsigned              max_length)
{
  PBCREP_BufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;
  uint8_t *out_at = out;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  memcpy (out_at, frag_data + in_frag, out_remaining);
	  out_remaining = 0;
	  break;
	}

      memcpy (out_at, frag_data + in_frag, frag_remaining);
      out_remaining -= frag_remaining;
      out_at += frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) pbcrep_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      in_frag = 0;
    }
  return max_length - out_remaining;
}

/**
 * pbcrep_buffer_iterator_read:
 * @iterator: to read data from.
 * @out: to copy data into.
 * @max_length: maximum number of bytes to write to @out.
 *
 * Peek data from the current position of an iterator.
 * The iterator's position is updated to be at the end of
 * the data read.
 *
 * returns: number of bytes read into @out.
 */
unsigned
pbcrep_buffer_iterator_read      (PBCREP_BufferIterator *iterator,
			       void *           out,
			       unsigned              max_length)
{
  PBCREP_BufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;
  uint8_t *out_at = out;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  memcpy (out_at, frag_data + in_frag, out_remaining);
	  in_frag += out_remaining;
	  out_remaining = 0;
	  break;
	}

      memcpy (out_at, frag_data + in_frag, frag_remaining);
      out_remaining -= frag_remaining;
      out_at += frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) pbcrep_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      in_frag = 0;
    }
  iterator->in_cur = in_frag;
  iterator->fragment = fragment;
  iterator->cur_length = frag_length;
  iterator->cur_data = frag_data;
  iterator->offset += max_length - out_remaining;
  return max_length - out_remaining;
}

/**
 * pbcrep_buffer_iterator_find_char:
 * @iterator: to advance.
 * @c: the character to look for.
 *
 * If it exists,
 * skip forward to the next instance of @c and return TRUE.
 * Otherwise, do nothing and return FALSE.
 *
 * returns: whether the character was found.
 */

gboolean
pbcrep_buffer_iterator_find_char (PBCREP_BufferIterator *iterator,
			       char               c)
{
  PBCREP_BufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;
  unsigned new_offset = iterator->offset;

  if (fragment == NULL)
    return -1;

  for (;;)
    {
      unsigned frag_remaining = frag_length - in_frag;
      const uint8_t * ptr = memchr (frag_data + in_frag, c, frag_remaining);
      if (ptr != NULL)
	{
	  iterator->offset = (ptr - frag_data) - in_frag + new_offset;
	  iterator->fragment = fragment;
	  iterator->in_cur = ptr - frag_data;
	  iterator->cur_length = frag_length;
	  iterator->cur_data = frag_data;
	  return TRUE;
	}
      fragment = fragment->next;
      if (fragment == NULL)
	return FALSE;
      new_offset += frag_length - in_frag;
      in_frag = 0;
      frag_length = fragment->buf_length;
      frag_data = (uint8_t *) fragment->buf + fragment->buf_start;
    }
}

/**
 * pbcrep_buffer_iterator_skip:
 * @iterator: to advance.
 * @max_length: maximum number of bytes to skip forward.
 *
 * Advance an iterator forward in the buffer,
 * returning the number of bytes skipped.
 *
 * returns: number of bytes skipped forward.
 */
unsigned
pbcrep_buffer_iterator_skip      (PBCREP_BufferIterator *iterator,
			       unsigned              max_length)
{
  PBCREP_BufferFragment *fragment = iterator->fragment;

  unsigned frag_length = iterator->cur_length;
  const uint8_t *frag_data = iterator->cur_data;
  unsigned in_frag = iterator->in_cur;

  unsigned out_remaining = max_length;

  while (fragment != NULL)
    {
      unsigned frag_remaining = frag_length - in_frag;
      if (out_remaining <= frag_remaining)
	{
	  in_frag += out_remaining;
	  out_remaining = 0;
	  break;
	}

      out_remaining -= frag_remaining;

      fragment = fragment->next;
      if (fragment != NULL)
	{
	  frag_data = (uint8_t *) pbcrep_buffer_fragment_start (fragment);
	  frag_length = fragment->buf_length;
	}
      else
	{
	  frag_data = NULL;
	  frag_length = 0;
	}
      in_frag = 0;
    }
  iterator->in_cur = in_frag;
  iterator->fragment = fragment;
  iterator->cur_length = frag_length;
  iterator->cur_data = frag_data;
  iterator->offset += max_length - out_remaining;
  return max_length - out_remaining;
}
#endif

PBCREP_BufferFragment *pbcrep_buffer_find_fragment (PBCREP_Buffer   *buffer,
                                             unsigned     offset,
                                             unsigned    *frag_offset_out)
{
  PBCREP_BufferFragment *fragment = buffer->first_frag;
  unsigned frag_offset = 0;
  while (frag_offset < offset)
    {
      if (offset >= frag_offset + fragment->buf_length)
        {
          frag_offset += fragment->buf_length;
          fragment = fragment->next;
        }
      else
        {
          *frag_offset_out = frag_offset;
          return fragment;
        }
    }
  *frag_offset_out = frag_offset;
  return fragment;
}
uint8_t pbcrep_buffer_get_last_byte (PBCREP_Buffer *buffer)
{
  PBCREP_BufferFragment *f = buffer->last_frag;
  return f->buf[f->buf_start + f->buf_length - 1];
}
uint8_t pbcrep_buffer_get_byte_at (PBCREP_Buffer *buffer, size_t idx)
{
  unsigned frag_offset;
  PBCREP_BufferFragment *frag = pbcrep_buffer_find_fragment (buffer, idx, &frag_offset);
  unsigned off = idx - frag_offset;
  return frag->buf[frag->buf_start + off];
}


unsigned
pbcrep_buffer_fragment_peek (PBCREP_BufferFragment *fragment,
                          unsigned           offset,
                          unsigned           length,
                          void              *buf)
{
  char *b = buf;
  unsigned rv = 0;
  if (fragment->buf_length == offset)
    {
      offset = 0;
      fragment = fragment->next;
    }
  if (fragment->buf_length >= length + offset)
    {
      memcpy (buf, fragment->buf + fragment->buf_start + offset, length);
      return length;
    }
  rv = fragment->buf_length - length;
  memcpy (buf, fragment->buf + fragment->buf_start + offset, rv);
  fragment = fragment->next;
  b += rv;
  while (fragment)
    {
      if (fragment->buf_length >= length - rv)
        {
          memcpy (b, fragment->buf + fragment->buf_start, length - rv);
          return length;
        }
      else
        {
          memcpy (b, fragment->buf + fragment->buf_start, fragment->buf_length);
          rv += fragment->buf_length;
          b += fragment->buf_length;
          fragment = fragment->next;
        }
    }
  return rv;
}

pbcrep_boolean pbcrep_buffer_fragment_advance (PBCREP_BufferFragment **frag_inout,
                                         unsigned           *offset_inout,
                                         unsigned            skip)
{
  PBCREP_BufferFragment *fragment = *frag_inout;
  if (fragment->buf_length >= *offset_inout + skip)
    {
      *offset_inout += skip;
      return PBCREP_TRUE;
    }
  skip -= (fragment->buf_length - *offset_inout);
  while (skip > 0 && fragment != NULL)
    {
      if (skip >= fragment->buf_length)
        {
          skip -= fragment->buf_length;
          fragment = fragment->next;
        }
      else
        {
          *offset_inout = skip;
          *frag_inout = fragment;
          return PBCREP_TRUE;
        }
    }
  return PBCREP_FALSE;
}

void
pbcrep_buffer_append_empty_fragment (PBCREP_Buffer *buffer)
{
  PBCREP_BufferFragment *fragment = new_native_fragment ();
  if (buffer->last_frag)
    buffer->last_frag->next = fragment;
  else
    buffer->first_frag = fragment;
  buffer->last_frag = fragment;
}

void     pbcrep_buffer_append_placeholder  (PBCREP_Buffer    *buffer,
                                         unsigned      length,
                                         PBCREP_BufferPlaceholder *out)
{
  out->buffer = buffer;
  out->fragment = buffer->last_frag;
  if (out->fragment)
    out->offset = out->fragment->buf_start + out->fragment->buf_length;
  else
    out->offset = 0;
  out->length = length;

  CHECK_INTEGRITY (buffer);
  buffer->size += length;
  while (length > 0)
    {
      unsigned avail;
      if (!buffer->last_frag)
	{
	  buffer->last_frag = buffer->first_frag = new_native_fragment ();
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	}
      else
	{
	  avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	  if (avail <= 0)
	    {
	      buffer->last_frag->next = new_native_fragment ();
	      avail = pbcrep_buffer_fragment_avail (buffer->last_frag);
	      buffer->last_frag = buffer->last_frag->next;
	    }
	}
      if (avail > length)
	avail = length;
      length -= avail;
      buffer->last_frag->buf_length += avail;
    }
  if (out->fragment == NULL)
    out->fragment = buffer->first_frag;
  CHECK_INTEGRITY (buffer);
}


void     pbcrep_buffer_placeholder_set     (PBCREP_BufferPlaceholder *placeholder,
                                         const void       *data)
{
  unsigned rem = placeholder->length;
  PBCREP_BufferFragment *frag = placeholder->fragment;
  unsigned offset = placeholder->offset;
  if (rem > 0)
    for (;;)
      {
        unsigned avail = frag->buf_start + frag->buf_length - offset;
        if (PBCREP_LIKELY (avail >= rem))
          {
            memcpy (frag->buf + offset, data, rem);
            return;
          }
        else
          {
            memcpy (frag->buf + offset, data, avail);
            rem -= avail;
            data = (const char *) data + avail;
            frag = frag->next;
            offset = 0;
          }
      }
}



void pbcrep_buffer_maybe_remove_empty_fragment (PBCREP_Buffer *buffer)
{
  if (buffer->last_frag->buf_length == 0)
    {
      PBCREP_BufferFragment **p = &(buffer->first_frag);
      while (*p != buffer->last_frag)
        p = &((*p)->next);
      *p = NULL;
      pbcrep_buffer_fragment_free (buffer->last_frag);
      buffer->last_frag = NULL;
    }
}

void
pbcrep_buffer_fragment_free (PBCREP_BufferFragment *fragment)
{
  recycle (fragment);
}

char *pbcrep_buffer_empty_to_string (PBCREP_Buffer *buffer)
{
  char *rv = pbcrep_malloc (buffer->size + 1);
  rv[buffer->size] = 0;
  pbcrep_buffer_read (buffer, buffer->size, rv);
  return rv;
}

pbcrep_boolean pbcrep_buffer_dump (PBCREP_Buffer          *buffer,
                             const char         *filename,
                             PBCREP_BufferDumpFlags  flags,
                             PBCREP_Error          **error)
{
  // exactly one of PBCREP_BUFFER_DUMP_NO_DRAIN or PBCREP_BUFFER_DUMP_DRAIN
  // must be given.
  PBCREP_BufferDumpFlags drain_flags = (flags & (PBCREP_BUFFER_DUMP_DRAIN|PBCREP_BUFFER_DUMP_NO_DRAIN));
  pbcrep_assert (drain_flags == PBCREP_BUFFER_DUMP_NO_DRAIN
           || drain_flags == PBCREP_BUFFER_DUMP_DRAIN);

  PBCREP_Error *fatal_error_buf = NULL;
  if (flags & PBCREP_BUFFER_DUMP_FATAL_ERRORS)
    error = &fatal_error_buf;
  PBCREP_DirOpenfdFlags open_flags = PBCREP_DIR_OPENFD_MAY_CREATE
                               | PBCREP_DIR_OPENFD_WRITABLE
                               | PBCREP_DIR_OPENFD_TRUNCATE
                               | ((flags & PBCREP_BUFFER_DUMP_NO_CREATE_DIRS) ? PBCREP_DIR_OPENFD_NO_MKDIR : 0)
                               ;
  unsigned mode = (flags & PBCREP_BUFFER_DUMP_EXECUTABLE) ? 0777 : 0666;
  int fd = pbcrep_dir_openfd (NULL, filename, open_flags, mode, error);
  if (fd < 0)
    goto error;
  unsigned n_frags = 0;
  for (PBCREP_BufferFragment *frag = buffer->first_frag; frag; frag = frag->next)
    n_frags++;
  struct iovec *iov = alloca (sizeof (struct iovec) * n_frags);
  unsigned iov_index = 0;
  for (PBCREP_BufferFragment *frag = buffer->first_frag; frag; frag = frag->next)
    {
      iov[iov_index].iov_base = frag->buf + frag->buf_start;
      iov[iov_index].iov_len = frag->buf_length;
      iov_index++;
    }
  ssize_t writev_rv;
  size_t rem = buffer->size;
retry_writev:
  writev_rv = writev (fd, iov, n_frags);
  if (writev_rv < 0)
    {
      if (errno == EINTR)
        goto retry_writev;
      pbcrep_set_error (error, "error writing to '%s': %s",
                     filename, strerror (errno));
      unlink (filename);
      goto error;
    }
  size_t written = writev_rv;
  if (written < rem)
    {
      while (written > 0)
        {
          if (iov->iov_len <= (size_t) writev_rv)
            {
              written -= iov->iov_len;
              iov++;
              n_frags--;
            }
          else
            {
              iov->iov_len -= writev_rv;
              iov->iov_base += writev_rv;
              written = 0;
            }
        }
      rem -= writev_rv;
      goto retry_writev;
    }
  else
    pbcrep_assert ((size_t) writev_rv == buffer->size);
  close (fd);
  if (flags & PBCREP_BUFFER_DUMP_DRAIN)
    pbcrep_buffer_discard (buffer, buffer->size);
  return PBCREP_TRUE;

error:
  if (flags & PBCREP_BUFFER_DUMP_FATAL_ERRORS)
    pbcrep_error ("error writing to %s: %s", filename, fatal_error_buf->message);
  return PBCREP_FALSE;
}
