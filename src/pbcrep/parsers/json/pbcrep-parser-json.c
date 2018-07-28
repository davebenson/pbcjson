#include "json-cb-parser.h"
#include "../../../pbcrep.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>//DEBUG

static inline size_t
sizeof_field_from_type (ProtobufCType type)
{
  switch (type)
    {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_FLOAT: return 4;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_DOUBLE: return 8;

    case PROTOBUF_C_TYPE_BOOL: return sizeof(protobuf_c_boolean);
    case PROTOBUF_C_TYPE_ENUM: return 4;
    case PROTOBUF_C_TYPE_STRING: return sizeof(char*);
    case PROTOBUF_C_TYPE_BYTES: return sizeof(ProtobufCBinaryData);
    case PROTOBUF_C_TYPE_MESSAGE: return sizeof(ProtobufCMessage*);
    }
}
#define REPEATED_VALUE_ARRAY_CHUNK_SIZE     512

#define IS_POWER_OF_TWO_OR_ZERO(x)  ((x) & ((x) - 1))

typedef struct RepeatedValueArrayList RepeatedValueArrayList;
struct RepeatedValueArrayList {
  RepeatedValueArrayList *next;
};

typedef struct PBCREP_Parser_JSON_Stack {
  const ProtobufCFieldDescriptor *field_desc;
  ProtobufCMessage *message;  // contains field corresponding to field_desc
  RepeatedValueArrayList *rep_list_backward;
  size_t n_repeated_values;
} PBCREP_Parser_JSON_Stack;

typedef struct Slab Slab;
struct Slab
{
  size_t size;
  Slab *next;
};

#define SLAB_GET_DATA(slab)   ((uint8_t *) ((slab) + 1))


typedef struct PBCREP_Parser_JSON PBCREP_Parser_JSON;
struct PBCREP_Parser_JSON {
  PBCREP_Parser base;

  JSON_CallbackParser *json_parser;

  unsigned stack_depth;
  unsigned max_stack_depth;
  PBCREP_Parser_JSON_Stack *stack;

  // Set to 1 if an unknown field name is encountered;
  // if an object or array is encountered, the depth is tracked.
  // if '}', ']' or a flat value is encountered at depth 1,
  // then it skipping is over and skip_depth is set to 0.
  unsigned skip_depth;

  Slab *slab_ring;
  size_t slab_used;
  Slab *cur_first;

  const char * error_code;
  const char * error_message;

  RepeatedValueArrayList *recycled_repeated_nodes;
};

static void *
parser_alloc_slow_case (PBCREP_Parser_JSON *parser,
                        size_t           size)
{
  // do we need a new slab?
  if (parser->slab_ring->next == parser->cur_first
   || parser->slab_ring->next->size < size)
    {
      // allocate a new slab
      size *= 3;
      size += ((1<<10)-1);
      size &= ~((1<<10)-1);
      Slab *slab = malloc (sizeof (Slab) + size);
      slab->next = parser->slab_ring->next;
      parser->slab_ring->next = slab;
      parser->slab_ring = slab;
    }
  else
    {
      // Move to next recycled slab,
      // which we checked above was sufficiently large.
      parser->slab_ring = parser->slab_ring->next;
    }

  void *rv = SLAB_GET_DATA (parser->slab_ring);
  parser->slab_used = size;
  return rv;
}

static inline void *
parser_alloc (PBCREP_Parser_JSON *parser,
              size_t              size,
              size_t              align)
{
  parser->slab_used += align - 1;
  parser->slab_used &= ~(align - 1);
  if (parser->slab_used + size <= parser->slab_ring->size)
    {
      void *rv = SLAB_GET_DATA (parser->slab_ring) + parser->slab_used;
      parser->slab_used += size;
      return rv;
    }
  else
    return parser_alloc_slow_case (parser, size);
}

static void
parser_allocator_reset (PBCREP_Parser_JSON *parser)
{
  parser->cur_first = parser->slab_ring;
  parser->slab_used = 0;
}

static inline RepeatedValueArrayList *
parser_allocate_repeated_value_array_list_node (PBCREP_Parser_JSON *parser)
{
  if (parser->recycled_repeated_nodes != NULL)
    {
      RepeatedValueArrayList *rv = parser->recycled_repeated_nodes;
      parser->recycled_repeated_nodes = rv->next;
      return rv;
    }
  else
    {
      return malloc (sizeof (RepeatedValueArrayList) + REPEATED_VALUE_ARRAY_CHUNK_SIZE);
    }
}

static inline void
parser_free_repeated_value_array_list_node (PBCREP_Parser_JSON *parser,
                                            RepeatedValueArrayList *to_free)
{
  to_free->next = parser->recycled_repeated_nodes;
  parser->recycled_repeated_nodes = to_free;
}

static inline void *
parser_allocate_value_from_repeated_value_pool
                              (PBCREP_Parser_JSON *parser,
                               size_t              size_bytes)
{
  size_t slab_n_elts = REPEATED_VALUE_ARRAY_CHUNK_SIZE / size_bytes;
  assert((slab_n_elts & (slab_n_elts-1)) == 0);
  PBCREP_Parser_JSON_Stack *s = parser->stack + parser->stack_depth - 1;
  size_t cur_slab_n = s->n_repeated_values & (slab_n_elts-1);
  RepeatedValueArrayList *list;
  if (cur_slab_n == 0)
    {
      if (parser->recycled_repeated_nodes == NULL)
        {
          list = parser->recycled_repeated_nodes;
          parser->recycled_repeated_nodes->next = list;
        }
      else
        {
          list = malloc (sizeof(RepeatedValueArrayList)
                       + REPEATED_VALUE_ARRAY_CHUNK_SIZE);
        }
      list->next = s->rep_list_backward;
      s->rep_list_backward = list;
    }
  else
    list = s->rep_list_backward;
  uint8_t *rv = (uint8_t *) (list + 1) + size_bytes * cur_slab_n;
  s->n_repeated_values += 1;
  return rv;
}

static bool
json__start_object   (void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth += 1;
      return true;
    }
  if (p->stack_depth == 0)
    {
      parser_allocator_reset (p);
      p->stack[0].message = parser_alloc (p, p->base.message_desc->sizeof_message, 8);
      protobuf_c_message_init (p->base.message_desc, p->stack[0].message);
      fprintf(stderr, "ALLOCATED MESSAGE %p at stack depth 0 named %s\n", p->stack[0].message, p->base.message_desc->name);
      p->stack[0].field_desc = NULL;
      p->stack[0].rep_list_backward = NULL;
      p->stack[0].n_repeated_values = 0;
      p->stack_depth = 1;
    }
  else
    {
      PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
      if (s->field_desc->type != PROTOBUF_C_TYPE_MESSAGE)
        {
          p->error_code = "OBJECT_NOT_ALLOWED_FOR_FIELD";
          p->error_message = "Only Message Fields may be stored as objects";
          return false;
        }
      const ProtobufCMessageDescriptor *md = s->field_desc->descriptor;
      s[1].message = parser_alloc (p, p->base.message_desc->sizeof_message, 8);
      fprintf(stderr, "ALLOCATED MESSAGE %p at stack depth %u (%s)\n", s[1].message, p->stack_depth, md->name);
      protobuf_c_message_init (md, s[1].message);
      s[1].field_desc = NULL;
      s[1].n_repeated_values = 0;
      s[1].rep_list_backward = NULL;
      * (ProtobufCMessage **) ((char*)s[0].message + s[0].field_desc->offset) = s[1].message;
      p->stack_depth += 1;
    }
  return true;
}

static bool
json__end_object     (void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth -= 1;
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  s->field_desc = NULL;
  --(p->stack_depth);
  if (p->stack_depth == 0)
    p->base.target.message_callback (&p->base, p->stack[0].message, p->base.target.callback_data);
  return true;
}

static bool
json__start_array    (void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth++;
      return true;
    }
  if (p->stack_depth == 0)
    {
      p->error_code = "ARRAY_NOT_ALLOWED_AT_TOPLEVEL";
      p->error_message = "Toplevel JSON object must not be array";
      return false;
    }
  else if (p->stack[p->stack_depth-1].field_desc->label != PROTOBUF_C_LABEL_REPEATED)
    {
      p->error_code = "NOT_A_REPEATED_FIELD";
      p->error_message = "Arrays are only allowed for repeated fields";
      return false;
    }
  else
    {
      PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
      s->rep_list_backward = parser_allocate_repeated_value_array_list_node (p);
      s->rep_list_backward->next = NULL;
      s->n_repeated_values = 0;
      return true;
    }
}

static bool
json__end_array      (void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth -= 1;
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  const ProtobufCFieldDescriptor *f = s->field_desc;
  assert(f != NULL);
  assert(f->label == PROTOBUF_C_LABEL_REPEATED);

  // The "quantifier" member: for repeated fields this is a size_t
  void *qmember = (char *) s->message + f->quantifier_offset;
  *(size_t *) qmember = s->n_repeated_values;

  void *member = (char *) s->message + f->offset;
  size_t sizeof_elt = sizeof_field_from_type (f->type);

  // contiguous array (the final array)
  uint8_t *contig_array = parser_alloc (p, s->n_repeated_values * sizeof_elt, sizeof_elt);
  * (void **) member = contig_array;

  // output pointer into contig_array
  uint8_t *contig_at = contig_array;

  size_t full_slab_n_elts = REPEATED_VALUE_ARRAY_CHUNK_SIZE / sizeof_elt;
  size_t n_partial_elts = s->n_repeated_values % full_slab_n_elts;
  RepeatedValueArrayList *partial = NULL;
  if (n_partial_elts > 0)
    {
      partial = s->rep_list_backward;
      s->rep_list_backward = partial->next;
    }

  // reverse rep_list_backward (which may be empty),
  // but whose elements are totally full of values.
  if (s->rep_list_backward != NULL)
    {
      RepeatedValueArrayList *new_list = NULL;
      RepeatedValueArrayList *at = s->rep_list_backward;
      RepeatedValueArrayList *next;

      // reverse list: the new (reversed) list will be in new_list.
      while (at != NULL)
        {
          next = at->next;
          at->next = new_list;
          new_list = at;
          at = next;
        }

      // memcpy into final contiguous array
      for (at = new_list; at != NULL; at = next)
        {
          next = at->next;
          // memcpy records (everything they point at should be in the slab)
          memcpy (contig_at, at + 1, REPEATED_VALUE_ARRAY_CHUNK_SIZE);
          contig_at += REPEATED_VALUE_ARRAY_CHUNK_SIZE;

          // recycle node to a per-parser list
          parser_free_repeated_value_array_list_node (p, at);
        }
    }

  // write the final partial RepeatedValueArrayList
  if (partial != NULL)
    {
      memcpy (contig_at, partial + 1, n_partial_elts * sizeof_elt);
      parser_free_repeated_value_array_list_node (p, partial);
    }
  s->field_desc = NULL;

  return true;
}

static bool
json__object_key     (unsigned key_length,
                      const char *key,
                      void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  (void) key_length;
  fprintf(stderr, "object_key: key=%s\n",key);
  if (p->skip_depth > 0)
    {
      /* skip_depth==1 implies that we just got an unknown object key.
       * either we should be a value (which will remove skip_depth==1),
       * or a structured value that'll increase skip_depth by one,
       * so it'll be at least 2 at that point. */
      assert(p->skip_depth != 1);
      return true;
    }
  
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  ProtobufCMessage *message = s->message;
  fprintf(stderr, "looking for field %s in message %p\n", key, message);
  const ProtobufCMessageDescriptor *msg_desc = message->descriptor;
  assert(msg_desc->magic == PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC);
  assert (s->field_desc == NULL);
  const ProtobufCFieldDescriptor *field_desc = protobuf_c_message_descriptor_get_field_by_name (msg_desc, key);
  if (field_desc == NULL)
    {
      p->skip_depth = 1;
      return true;
    }
  s->field_desc = field_desc;
  return true;
}

static void *
prepare_flat_value (PBCREP_Parser_JSON *p)
{
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  const ProtobufCFieldDescriptor *f = s->field_desc;
  void *member = (char *) s->message + f->offset;
  void *qmember = (char *) s->message + f->quantifier_offset;
  size_t sizeof_member = sizeof_field_from_type (f->type);
  void *value = member;
  if (f->label == PROTOBUF_C_LABEL_REPEATED)
    {
      if (s->rep_list_backward != NULL)
        {
          value = parser_allocate_value_from_repeated_value_pool (p, sizeof_member);
        }
      else
        {
          // single unrepeated value
          * (size_t *) qmember = 1;
          * (void **) member = value = parser_alloc (p, sizeof_member, sizeof_member);
        }
    }
  else if (f->label == PROTOBUF_C_LABEL_OPTIONAL)
    {
      * (protobuf_c_boolean *) qmember = 1;
    }
  return value;
}

static bool
parse_number_to_value (PBCREP_Parser_JSON *p,
                       const char *number,
                       const ProtobufCFieldDescriptor *f,
                       void *value_out)
{
  char *end;
  switch (f->type)
    {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      {
        int v = strtol (number, &end, 0);
        if (number == end)
          goto bad_number;
        * (int32_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      {
        int v = strtoul (number, &end, 0);
        if (number == end)
          goto bad_number;
        * (uint32_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_FLOAT:
      {
        float v = strtod (number, &end);
        if (number == end)
          goto bad_number;
        * (float *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      {
        char *end;
        int64_t v = strtoll (number, &end, 0);
        if (number == end)
          goto bad_number;
        * (int64_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      {
        uint64_t v = strtoull (number, &end, 0);
        if (number == end)
          goto bad_number;
        * (uint64_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_DOUBLE:
      {
        double v = strtod (number, &end);
        if (number == end)
          goto bad_number;
        * (double *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_BOOL:
      {
        unsigned long v = strtoul (number, NULL, 0);
        * (protobuf_c_boolean *) value_out = v ? 1 : 0;
        return true;
      }

    case PROTOBUF_C_TYPE_ENUM:
      {
        unsigned long v = strtoul (number, NULL, 0);
        const ProtobufCEnumDescriptor *ed = f->descriptor;
        const ProtobufCEnumValue *ev = protobuf_c_enum_descriptor_get_value (ed, v);
        if (ev == NULL)
          {
            PBCREP_Error error = {
              .error_code_str = "BAD_ENUM_NUMERIC_VALUE",
              .error_message = "Unknown enum value given as number"
            }; 
            p->base.target.error_callback (&p->base,
                                           &error,
                                           p->base.target.callback_data);
            return false;
          }
        * (uint32_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_STRING:
      * (char **) value_out = strcpy (parser_alloc (p, strlen (number) + 1, 1), number);
      return true;

    case PROTOBUF_C_TYPE_BYTES:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_VALUE_FOR_BYTES",
          .error_message = "Bytes field cannot be initialized with a number"
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }

    case PROTOBUF_C_TYPE_MESSAGE:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_VALUE_FOR_MESSAGE",
          .error_message = "Message field cannot be initialized with a number"
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }
    }
  return false;

bad_number:
  {
    PBCREP_Error error = {
      .error_code_str = "BAD_NUMBER",
      "Numeric value doesn't match Protobuf type"
    }; 
    p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
  }
  return false;
}

static bool
json__number_value   (unsigned number_length,
                      const char *number,
                      void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  (void) number_length;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);

  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  const ProtobufCFieldDescriptor *f = s->field_desc;
  void *value = prepare_flat_value (p);

  if (!parse_number_to_value (p, number, f, value))
    return false;
  s->field_desc = NULL;
  return true;
}

#define json__partial_string_value NULL
#if 0
static unsigned
json__partial_string_value (unsigned cur_string_length_in_bytes,
                            const char *cur_string,
                            void *callback_data)
{
...
}
#endif

static inline int
hexdigit_value (char c)
{
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static bool
parse_string_to_value (PBCREP_Parser_JSON *p,
                       size_t string_length,
                       const char *string,
                       const ProtobufCFieldDescriptor *f,
                       void *value_out)
{
  char *end;
  switch (f->type)
    {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      {
        int v = strtol (string, &end, 0);
        if (string == end)
          goto bad_number;
        * (int32_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      {
        int v = strtoul (string, &end, 0);
        if (string == end)
          goto bad_number;
        * (uint32_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_FLOAT:
      {
        float v = strtod (string, &end);
        if (string == end)
          goto bad_number;
        * (float *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      {
        char *end;
        int64_t v = strtoll (string, &end, 0);
        if (string == end)
          goto bad_number;
        * (int64_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      {
        uint64_t v = strtoull (string, &end, 0);
        if (string == end)
          goto bad_number;
        * (uint64_t *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_DOUBLE:
      {
        double v = strtod (string, &end);
        if (string == end)
          goto bad_number;
        * (double *) value_out = v;
        return true;
      }

    case PROTOBUF_C_TYPE_BOOL:
      {
        unsigned long v = strtoul (string, NULL, 0);
        * (protobuf_c_boolean *) value_out = v ? 1 : 0;
        return true;
      }

    case PROTOBUF_C_TYPE_ENUM:
      {
        const ProtobufCEnumDescriptor *ed = f->descriptor;
        const ProtobufCEnumValue *ev = protobuf_c_enum_descriptor_get_value_by_name (ed, string);
        if (ev == NULL)
          {
            PBCREP_Error error = {
              .error_code_str = "BAD_ENUM_STRING_VALUE",
              "Unknown enum value given as string"
            }; 
            p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
            return false;
          }
        * (uint32_t *) value_out = ev->value;
        return true;
      }

    case PROTOBUF_C_TYPE_STRING:
      {
        char *rv = parser_alloc (p, string_length + 1, 1);
        memcpy (rv, string, string_length + 1);
        * (char **) value_out = rv;
        return true;
      }

    case PROTOBUF_C_TYPE_BYTES:
      {
        ProtobufCBinaryData *bd = value_out;
        bd->len = 0;
        bd->data = parser_alloc (p, string_length / 2, 1);
        const char *end = string + string_length;
        const char *at = string;
        while (at < end)
          {
            int h = hexdigit_value(at[0]);
            if (h < 0)
              {
                if (*at == ' ' || *at == '\n')
                  {
                    at++;
                    continue;
                  }
                PBCREP_Error error = {
                  .error_code_str = "UNEXPECTED_CHAR",
                  .error_message = "only hex-digits and whitespace allowed"
                };
                p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
                return false;
              }
            int h2 = hexdigit_value(at[1]);
            if (h2 < 0)
              {
                PBCREP_Error error = {
                  .error_code_str = "BAD_HEX",
                  .error_message = "bad hex digit"
                };
                p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
                return false;
              }
            bd->data[bd->len++] = (h<<4) | h2;
            at += 2;
          }
        return true;
      }

    case PROTOBUF_C_TYPE_MESSAGE:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_VALUE_FOR_MESSAGE",
          .error_message = "Message field cannot be initialized with a number"
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }
    }
  return false;

bad_number:
  {
    PBCREP_Error error = {
      .error_code_str = "BAD_NUMBER",
      .error_message = "Numeric value doesn't match Protobuf type"
    }; 
    p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
  }
  return false;
}

static bool
json__string_value   (unsigned string_length,
                      const char *string,
                      void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }

  assert(p->stack_depth > 0);
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  const ProtobufCFieldDescriptor *f = s->field_desc;
  void *value = prepare_flat_value (p);

  if (!parse_string_to_value (p, string_length, string, f, value))
    return false;
  s->field_desc = NULL;
  return true;
}

static bool
json__boolean_value  (int boolean_value,
                      void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  assert(s->field_desc != NULL);
  void *value_out = (char*) s->message + s->field_desc->offset;
  switch (s->field_desc->type)
    {
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      * (int32_t *) value_out = boolean_value;
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_FLOAT:
      * (float *) value_out = boolean_value;
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      * (int64_t *) value_out = boolean_value;
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_DOUBLE:
      * (double *) value_out = boolean_value;
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_BOOL:
      * (protobuf_c_boolean *) value_out = boolean_value;
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_ENUM:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_ENUM",
          .error_message = "Enum may not be given as boolean",
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }

    case PROTOBUF_C_TYPE_STRING:
      * (char **) value_out = boolean_value ? "true" : "false";
      s->field_desc = NULL;
      return true;

    case PROTOBUF_C_TYPE_BYTES:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_VALUE_FOR_BYTES",
          .error_message = "Message field cannot be initialized with a boolean"
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }

    case PROTOBUF_C_TYPE_MESSAGE:
      {
        PBCREP_Error error = {
          .error_code_str = "BAD_VALUE_FOR_MESSAGE",
          .error_message = "Message field cannot be initialized with a boolean"
        }; 
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }
    }
  s->field_desc = NULL;
  return true;
}

static bool
json__null_value     (void *callback_data)
{
  PBCREP_Parser_JSON *p = callback_data;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);
  PBCREP_Parser_JSON_Stack *s = p->stack + p->stack_depth - 1;
  assert(s->field_desc != NULL);
  void *qmember = (char*) s->message + s->field_desc->quantifier_offset;
  void *member = (char*) s->message + s->field_desc->offset;
  switch (s->field_desc->label)
    {
    case PROTOBUF_C_LABEL_NONE:
    case PROTOBUF_C_LABEL_REPEATED:
      {
        * (size_t *) qmember = 0;
        return true;
      }
    case PROTOBUF_C_LABEL_OPTIONAL:
      {
      if (s->field_desc->quantifier_offset > 0)
        {
          * (protobuf_c_boolean *) qmember = 0;
        }
      else
        {
          * (void **) member = NULL;
        }
        return true;
      }
      break;

    case PROTOBUF_C_LABEL_REQUIRED:
      {
        PBCREP_Error error = {
          .error_code_str = "NULL_NOT_ALLOWED",
          .error_message = "null not allowed for required field"
        };
        p->base.target.error_callback (&p->base, &error, p->base.target.callback_data);
        return false;
      }
    }
  return true;
}

static void
json__error          (const JSON_CallbackParser_ErrorInfo *error,
                      void *callback_data)
{
  PBCREP_Error e = {
    .error_code_str = error->code_str,
    .error_message = error->message
  };
  PBCREP_Parser_JSON *p = callback_data;
  p->base.target.error_callback (&p->base, &e, p->base.target.callback_data);
}

#define json__destroy NULL

static JSON_Callbacks json_callbacks = JSON_CALLBACKS_DEF(json__, );


static bool
pbc_parser_json_feed (PBCREP_Parser      *parser,
                      size_t           data_length,
                      const uint8_t   *data)
{
  PBCREP_Parser_JSON *p = (PBCREP_Parser_JSON *) parser;
  return json_callback_parser_feed (p->json_parser, data_length, data);
}

static bool
pbc_parser_json_end_feed (PBCREP_Parser      *parser)
{
  PBCREP_Parser_JSON *p = (PBCREP_Parser_JSON *) parser;
  return json_callback_parser_end_feed (p->json_parser);
}

static void
pbc_parser_json_destroy  (PBCREP_Parser      *parser)
{
  PBCREP_Parser_JSON *p = (PBCREP_Parser_JSON *) parser;
  json_callback_parser_destroy (p->json_parser);
  for (unsigned i = 0; i < p->stack_depth; i++)
    {
      PBCREP_Parser_JSON_Stack *s = p->stack + i;
      while (s->rep_list_backward != NULL)
        {
          RepeatedValueArrayList *kill = s->rep_list_backward;
          s->rep_list_backward = kill->next;
          free (kill);
        }
    }
  Slab *at = p->slab_ring->next;
  p->slab_ring->next = NULL;

  Slab *builtin_slab = (Slab *) (p->stack + p->max_stack_depth);
  while (at != NULL)
    {
      Slab *next = at->next;
      if (at != builtin_slab)
        free (at);
      at = next;
    }

  while (p->recycled_repeated_nodes != NULL)
    {
      RepeatedValueArrayList *kill = p->recycled_repeated_nodes;
      p->recycled_repeated_nodes = kill->next;
      free (kill);
    }
}

PBCREP_Parser *
pbcrep_parser_new_json  (const ProtobufCMessageDescriptor  *message_desc,
                         const PBCREP_Parser_JSONOptions   *json_options,
                         PBCREP_ParserTarget                target)
{
  size_t size = sizeof (PBCREP_Parser_JSON)
              + sizeof (PBCREP_Parser_JSON_Stack) * json_options->max_stack_depth
              + sizeof (Slab)
              + json_options->estimated_message_size;

  JSON_CallbackParser_Options cb_parser_options;
  switch (json_options->json_dialect)
    {
      case PBCREP_JSON_DIALECT_JSON:
        cb_parser_options = JSON_CALLBACK_PARSER_OPTIONS_INIT;
        break;
      case PBCREP_JSON_DIALECT_JSON5:
        cb_parser_options = JSON_CALLBACK_PARSER_OPTIONS_INIT_JSON5;
        break;
      default:
        return NULL;
    }

  PBCREP_Parser *parser = pbcrep_parser_create_protected (message_desc, size,
                                                          target);
  PBCREP_Parser_JSON *p = (PBCREP_Parser_JSON *) parser;
  p->json_parser = json_callback_parser_new (&json_callbacks,
                                             parser,
                                             &cb_parser_options);

  parser->feed = pbc_parser_json_feed;
  parser->end_feed = pbc_parser_json_end_feed;
  parser->destroy = pbc_parser_json_destroy;

  /* If 1, we are in an unknown field of a known object type.
   * If >1, we are skipping and object/array-depth == skip_depth-1
   */
  p->skip_depth = 0;

  p->stack_depth = 0;
  p->max_stack_depth = json_options->max_stack_depth;
  p->stack = (PBCREP_Parser_JSON_Stack *) (p + 1);

  p->cur_first = (Slab *) (p->stack + json_options->max_stack_depth);
  p->slab_ring = p->cur_first;
  p->slab_ring->next = p->slab_ring;
  p->slab_used = 0;
  p->error_code = NULL;
  p->error_message = NULL;
  p->recycled_repeated_nodes = NULL;

  return parser;
} 

bool
pbcrep_parser_is_json (PBCREP_Parser *parser)
{
  return parser->feed == pbc_parser_json_feed;
}
