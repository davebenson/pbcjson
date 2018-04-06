#include "../length-prefixed.h"
#include <string.h>
#include <stdlib.h>

#define LENBUF_SIZE 12

typedef struct PBC_Parser_LengthPrefixed PBC_Parser_LengthPrefixed;
struct PBC_Parser_LengthPrefixed {
  PBC_Parser base;
  uint8_t lenbuf[LENBUF_SIZE];
  uint8_t lenbuf_len;
  PBC_LengthPrefixed_Format lp_format;
  bool in_length_prefix;

  size_t length_from_prefix;
  size_t buf_alloced, buf_length;
  uint8_t *buf;
};

static void *
PBC_LP_alloc (void *d, size_t s)
{
  (void) d;
  return malloc (s);
}
static void
PBC_LP_free (void *d, void *ptr)
{
  (void) d;
  free (ptr);
}

static bool
length_prefixed__feed     (PBC_Parser      *parser,
                           size_t           data_length,
                           const uint8_t   *data)
{
  PBC_Parser_LengthPrefixed *lp = (PBC_Parser_LengthPrefixed*) parser;
  if (lp->in_length_prefix)
    goto in_length_prefix;
  else
    goto in_data;

in_length_prefix:
  switch (lp->lp_format)
    {
    case PBC_LENGTH_PREFIXED_UINT8:
      if (data_length == 0)
        {
          lp->in_length_prefix = true;
          return true;
        }
      lp->length_from_prefix = *data;
      data++;
      data_length--;
      goto in_data;

    case PBC_LENGTH_PREFIXED_UINT16_LE:
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

    case PBC_LENGTH_PREFIXED_UINT24_LE:
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

    case PBC_LENGTH_PREFIXED_UINT32_LE:
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
    case PBC_LENGTH_PREFIXED_UINT16_BE:
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

    case PBC_LENGTH_PREFIXED_UINT24_BE:
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
    case PBC_LENGTH_PREFIXED_UINT32_BE:
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

    case PBC_LENGTH_PREFIXED_B128:
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
          PBC_Parser_Error error = {
            "BAD_B128",
            "overlong or bad B128-encoded length-prefix"
          };
          parser->callbacks.error_callback (parser, &error, parser->callback_data);
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
      
    case PBC_LENGTH_PREFIXED_B128_BE:
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
          PBC_Parser_Error error = {
            "BAD_B128",
            "overlong or bad B128-encoded big-endian length-prefix"
          };
          parser->callbacks.error_callback (parser, &error, parser->callback_data);
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
      lp->buf = realloc (lp->buf, lp->buf_alloced);
    }
  if (lp->buf_length + data_length >= lp->length_from_prefix)
    {
      size_t copy = lp->length_from_prefix - lp->buf_length;
      memcpy (lp->buf + lp->buf_length, data, copy);
      data += copy;
      data_length -= copy;

      ProtobufCAllocator allocator = {
        PBC_LP_alloc,
        PBC_LP_free,
        lp
      };
      ProtobufCMessage *msg = protobuf_c_message_unpack(lp->base.message_desc, &allocator, lp->buf_length, lp->buf);
      if (msg == NULL)
        {
          PBC_Parser_Error er = {
            "PROTOBUF_MALFORMED",
            "Error unpacking Protocol Buffers message"
          };
          parser->callbacks.error_callback (parser, &er, parser->callback_data);
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
length_prefixed__end_feed (PBC_Parser      *parser)
{
  PBC_Parser_LengthPrefixed *lp = (PBC_Parser_LengthPrefixed*) parser;
  if (lp->in_length_prefix)
    {
      if (lp->lenbuf_len == 0)
        return true;
      PBC_Parser_Error error = {
        "PARTIAL_RECORD",
        "terminated in length-prefix itself"
      };
      parser->callbacks.error_callback(parser, &error, parser->callback_data);
      return false;
    }
  else
    {
      PBC_Parser_Error error = {
        "PARTIAL_RECORD",
        "terminated in data body"
      };
      parser->callbacks.error_callback(parser, &error, parser->callback_data);
      return false;
    }
}

static void
length_prefixed__destroy  (PBC_Parser      *parser)
{
  PBC_Parser_LengthPrefixed *lp = (PBC_Parser_LengthPrefixed*) parser;
  if (lp->buf != NULL)
    free (lp->buf);
  pbc_parser_destroy_protected (parser);
}

PBC_Parser *
pbc_parser_new_length_prefixed (PBC_LengthPrefixed_Format lp_format,
                                const ProtobufCMessageDescriptor *desc,
                                PBC_ParserCallbacks         *callbacks,
                                void                        *callback_data)
{
  PBC_Parser *p;
  PBC_Parser_LengthPrefixed *lp;
  p = pbc_parser_create_protected (desc, sizeof (PBC_Parser_LengthPrefixed), callbacks, callback_data);
  lp = (PBC_Parser_LengthPrefixed *) p;

  lp->lp_format = lp_format;
  lp->lenbuf_len = 0;
  lp->length_from_prefix = 0;
  lp->buf_alloced = 0;
  lp->buf_length = 0;
  lp->buf = NULL;

  return p;
}
