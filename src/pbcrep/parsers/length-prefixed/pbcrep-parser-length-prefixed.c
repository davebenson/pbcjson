#include "../../../pbcrep.h"
#include <string.h>
#include <stdlib.h>

#define LENBUF_SIZE 12

typedef struct PBCREP_Parser_LengthPrefixed PBCREP_Parser_LengthPrefixed;
struct PBCREP_Parser_LengthPrefixed {
  PBCREP_Parser base;
  uint8_t lenbuf[LENBUF_SIZE];
  uint8_t lenbuf_len;
  PBCREP_LengthPrefixed_Format lp_format;
  bool in_length_prefix;

  size_t length_from_prefix;
  size_t buf_alloced, buf_length;
  uint8_t *buf;
};

static void *
PBCREP_LP_alloc (void *d, size_t s)
{
  (void) d;
  return pbcrep_malloc (s);
}
static void
PBCREP_LP_free (void *d, void *ptr)
{
  (void) d;
  pbcrep_free (ptr);
}

/* TODO: optimize by providing different feed implementations for
 * each LengthPrefixFormat.
 */
static bool
length_prefixed__feed     (PBCREP_Parser      *parser,
                           size_t              data_length,
                           const uint8_t      *data,
                           PBCREP_Error      **error)
{
  PBCREP_Parser_LengthPrefixed *lp = (PBCREP_Parser_LengthPrefixed*) parser;
  if (lp->in_length_prefix)
    goto in_length_prefix;
  else
    goto in_data;

in_length_prefix:
  switch (lp->lp_format)
    {
    case PBCREP_LENGTH_PREFIXED_UINT8:
      if (data_length == 0)
        {
          lp->in_length_prefix = true;
          return true;
        }
      lp->length_from_prefix = *data;
      data++;
      data_length--;
      goto in_data;

    case PBCREP_LENGTH_PREFIXED_UINT16_LE:
      if (data_length + lp->lenbuf_len < 2)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 2 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint16_t)lp->lenbuf[0]
                                 | ((uint16_t)lp->lenbuf[1] << 8);
          data += copy;
          data_length -= copy;
          goto in_data;
        }

    case PBCREP_LENGTH_PREFIXED_UINT24_LE:
      if (data_length + lp->lenbuf_len < 3)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 3 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint32_t)lp->lenbuf[0]
                                 | ((uint32_t)lp->lenbuf[1] << 8)
                                 | ((uint32_t)lp->lenbuf[2] << 16);
          data += copy;
          data_length -= copy;
          goto in_data;
        }

    case PBCREP_LENGTH_PREFIXED_UINT32_LE:
      if (data_length + lp->lenbuf_len < 4)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 4 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint32_t)lp->lenbuf[0]
                                 | ((uint32_t)lp->lenbuf[1] << 8)
                                 | ((uint32_t)lp->lenbuf[2] << 16)
                                 | ((uint32_t)lp->lenbuf[3] << 24);
          data += copy;
          data_length -= copy;
          goto in_data;
        }
    case PBCREP_LENGTH_PREFIXED_UINT16_BE:
      if (data_length + lp->lenbuf_len < 2)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 2 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint16_t)lp->lenbuf[1]
                                 | ((uint16_t)lp->lenbuf[0] << 8);
          data += copy;
          data_length -= copy;
          goto in_data;
        }

    case PBCREP_LENGTH_PREFIXED_UINT24_BE:
      if (data_length + lp->lenbuf_len < 3)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 3 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint32_t)lp->lenbuf[2]
                                 | ((uint32_t)lp->lenbuf[1] << 8)
                                 | ((uint32_t)lp->lenbuf[0] << 16);
          data += copy;
          data_length -= copy;
          goto in_data;
        }
    case PBCREP_LENGTH_PREFIXED_UINT32_BE:
      if (data_length + lp->lenbuf_len < 4)
        {
          memcpy (lp->lenbuf + lp->lenbuf_len, data, data_length);
          lp->lenbuf_len += data_length;
          lp->in_length_prefix = true;
          return true;
        }
      else
        {
          unsigned copy = 4 - lp->lenbuf_len;
          memcpy (lp->lenbuf + lp->lenbuf_len, data, copy);
          lp->length_from_prefix = (uint32_t)lp->lenbuf[3]
                                 | ((uint32_t)lp->lenbuf[2] << 8)
                                 | ((uint32_t)lp->lenbuf[1] << 16)
                                 | ((uint32_t)lp->lenbuf[0] << 24);
          data += copy;
          data_length -= copy;
          goto in_data;
        }

    case PBCREP_LENGTH_PREFIXED_B128:
      while (lp->lenbuf_len < LENBUF_SIZE && data_length > 0)
        {
          bool last = (*data & 0x80) == 0;
          lp->lenbuf[lp->lenbuf_len++] = *data++;
          data_length--;
          if (last)
            goto done_b128_le;
        }
      if (lp->lenbuf_len == LENBUF_SIZE)
        {
          *error = pbcrep_error_new (
            "BAD_B128",
            "overlong or bad B128-encoded length-prefix"
          );
          return false;
        }
      lp->in_length_prefix = true;
      return true;
      
      done_b128_le:
        {
          /* compute payload length */
          size_t v = 0;
          size_t shift = 0;
          for (unsigned i = 0; i < lp->lenbuf_len; i++)
            {
              v |= ((lp->lenbuf[i] & 0x7f) << shift);
              shift += 7;
            }
          lp->length_from_prefix = v;
        }
        goto in_data;
      
    case PBCREP_LENGTH_PREFIXED_B128_BE:
      while (lp->lenbuf_len < LENBUF_SIZE && data_length > 0)
        {
          bool last = (*data & 0x80) == 0;
          lp->lenbuf[lp->lenbuf_len++] = *data++;
          data_length--;
          if (last)
            goto done_b128_be;
        }
      if (lp->lenbuf_len == LENBUF_SIZE)
        {
          *error = pbcrep_error_new (
            "BAD_B128",
            "overlong or bad B128-encoded big-endian length-prefix"
          );
          return false;
        }
      lp->in_length_prefix = true;
      return true;
      
      done_b128_be:
        {
          /* compute payload length */
          size_t v = 0;
          for (unsigned i = 0; i < lp->lenbuf_len; i++)
            {
              v <<= 7;
              v |= (lp->lenbuf[i] & 0x7f);
            }
          lp->length_from_prefix = v;
        }
        goto in_data;
    }

in_data:
  if (lp->length_from_prefix > lp->buf_alloced)
    {
      lp->buf_alloced = lp->length_from_prefix;
      lp->buf = pbcrep_realloc (lp->buf, lp->buf_alloced);
    }
  if (lp->buf_length + data_length >= lp->length_from_prefix)
    {
      size_t copy = lp->length_from_prefix - lp->buf_length;
      memcpy (lp->buf + lp->buf_length, data, copy);
      data += copy;
      data_length -= copy;

      ProtobufCAllocator allocator = {
        PBCREP_LP_alloc,
        PBCREP_LP_free,
        lp
      };
      ProtobufCMessage *msg = protobuf_c_message_unpack(lp->base.message_desc, &allocator, lp->buf_length, lp->buf);
      if (msg == NULL)
        {
          *error = pbcrep_error_new (
            "PROTOBUF_MALFORMED",
            "Error unpacking Protocol Buffers message"
          );
          return false;
        }

      lp->buf_length = 0;
      lp->lenbuf_len = 0;
      goto in_length_prefix;
    }
  else
    {
      memcpy (lp->buf + lp->buf_length, data, data_length);
      lp->buf_length += data_length;
      return true;
    }
}

static bool
length_prefixed__end_feed (PBCREP_Parser      *parser,
                           PBCREP_Error      **error)
{
  PBCREP_Parser_LengthPrefixed *lp = (PBCREP_Parser_LengthPrefixed*) parser;
  if (lp->in_length_prefix)
    {
      if (lp->lenbuf_len == 0)
        return true;
      *error = pbcrep_error_new (
        "PARTIAL_RECORD",
        "terminated in length-prefix itself"
      );
      return false;
    }
  else
    {
      *error = pbcrep_error_new (
        "PARTIAL_RECORD",
        "terminated in data body"
      );
      return false;
    }
}

static void
length_prefixed__destruct (PBCREP_Parser      *parser)
{
  PBCREP_Parser_LengthPrefixed *lp = (PBCREP_Parser_LengthPrefixed*) parser;
  if (lp->buf != NULL)
    pbcrep_free (lp->buf);
}

PBCREP_Parser *
pbcrep_parser_new_length_prefixed (PBCREP_LengthPrefixed_Format lp_format,
                                   const ProtobufCMessageDescriptor *desc)
{
  PBCREP_Parser *p;
  PBCREP_Parser_LengthPrefixed *lp;
  p = pbcrep_parser_create_protected (desc, sizeof (PBCREP_Parser_LengthPrefixed));
  lp = (PBCREP_Parser_LengthPrefixed *) p;

  lp->lp_format = lp_format;
  lp->lenbuf_len = 0;
  lp->length_from_prefix = 0;
  lp->buf_alloced = 0;
  lp->buf_length = 0;
  lp->buf = NULL;
  lp->base.destruct = length_prefixed__destruct;
  lp->base.feed = length_prefixed__feed;
  lp->base.end_feed = length_prefixed__end_feed;
  return p;
}

void
pbcrep_parser_length_prefixed_set_format (PBCREP_Parser *parser,
                                           PBCREP_LengthPrefixed_Format format)
{
  assert (parser->feed == length_prefixed__feed);

  // TODO: assert that this is only called from message-handler.
  ((PBCREP_Parser_LengthPrefixed *) parser)->lp_format = format;
}
