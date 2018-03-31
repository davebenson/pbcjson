#include "json-cb-parser.h"
#include "pbc-parser-json.h"
#include <stdlib.h>
#include <string.h>

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

// Compute the number of bytes used by a particular
// type in the RepeatedValueArrayList.
// Since both values are powers-of-two, we return the CHUNK_SIZE.
//
// this function should evaluate to a constant return
// (since the asserts should be provable at compile time)
static inline size_t
repeated_value_array_size_for_type(ProtobufCType type)
{
  // all other types are "obviously" powers of 2
  if (type == PROTOBUF_C_TYPE_BYTES)
    assert(IS_POWER_OF_TWO_OR_ZERO (sizeof(ProtobufCBinaryData)));

  // the simple assumption below fails if this isn't true.
  assert(IS_POWER_OF_TWO_OR_ZERO (REPEATED_VALUE_ARRAY_CHUNK_SIZE));
  
  return REPEATED_VALUE_ARRAY_CHUNK_SIZE;
}

typedef struct PBC_JSON_ParserError {
  const char *error_message;
  const char *error_code;

  //TODO: stack

  /* For ignorable errors:
   *      returning TRUE from ParserErrorCallback will cause processing to continue.
   *      returning FALSE from ParserErrorCallback will cause pbc_json_parser_feed to return FALSE.
   * For non-ignorable:
   *      the ParserErrorCallback ret-val is ignored, and treated as FALSE.
   */
} PBC_JSON_ParserError;

typedef struct RepeatedValueArrayList RepeatedValueArrayList;
struct RepeatedValueArrayList {
  RepeatedValueArrayList *next;
};

typedef struct PBC_JSON_Parser_Stack {
  const ProtobufCFieldDescriptor *field_desc;
  ProtobufCMessage *message;  // contains field corresponding to field_desc
  RepeatedValueArrayList *rep_list_backward;
  size_t n_repeated_values;
} PBC_JSON_Parser_Stack;

typedef struct Slab Slab;
struct Slab
{
  size_t size;
  Slab *next;
};

#define SLAB_GET_DATA(slab)   ((uint8_t *) ((slab) + 1))


typedef struct PBC_JSON_Parser PBC_JSON_Parser;
struct PBC_JSON_Parser {
  PBC_Parser base;

  unsigned stack_depth;
  unsigned max_stack_depth;
  PBC_JSON_Parser_Stack *stack;

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
parser_alloc_slow_case (PBC_JSON_Parser *parser,
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
parser_alloc (PBC_JSON_Parser *parser,
              size_t           size,
              size_t           align)
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
parser_allocator_reset (PBC_JSON_Parser *parser)
{
  parser->cur_first = parser->slab_ring;
  parser->slab_used = 0;
}

static inline RepeatedValueArrayList *
parser_allocate_repeated_value_array_list_node (PBC_JSON_Parser *parser)
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
parser_free_repeated_value_array_list_node (PBC_JSON_Parser *parser,
                                            RepeatedValueArrayList *to_free)
{
  to_free->next = parser->recycled_repeated_nodes;
  parser->recycled_repeated_nodes = to_free;
}

static inline void *
parser_allocate_value_from_repeated_value_pool
                              (PBC_JSON_Parser *parser,
                               size_t           size_bytes)
{
  if (parser->recycled_repeated_nodes == NULL)
    {
      ...
    }
  else if (parser->...)
    {
      ...
    }
  else
    {
      ...
    }
}

static bool
json__start_object   (void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
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
      p->stack[0].field_desc = NULL;
      p->stack[0].rep_list_backward = NULL;
      p->stack[0].n_repeated_values = 0;
      p->stack_depth = 1;
    }
  else
    {
      PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
      if (s->field_desc->type != PROTOBUF_C_TYPE_MESSAGE)
        {
          p->error_code = "OBJECT_NOT_ALLOWED_FOR_FIELD";
          p->error_message = "Only Message Fields may be stored as objects";
          return false;
        }
      const ProtobufCMessageDescriptor *md = s->field_desc->descriptor;
      s[1].message = parser_alloc (p, p->base.message_desc->sizeof_message, 8);
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
  PBC_JSON_Parser *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth -= 1;
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  --(p->stack_depth);
  if (p->stack_depth == 0)
    p->base.callbacks.message_callback (&p->base, p->stack[0].message, p->base.callback_data);
  return true;
}

static bool
json__start_array    (void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
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
      PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
      s->rep_list_backward = parser_allocate_repeated_value_array_list_node (p);
      s->rep_list_backward->next = NULL;
      s->n_repeated_values = 0;
      return true;
    }
}

static bool
json__end_array      (void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
  if (p->skip_depth > 0)
    {
      p->skip_depth -= 1;
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
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

  RepeatedValueArrayList *partial_arr = s->rep_list_backward;
  s->rep_list_backward = partial_arr->next;

  size_t contig_full_slab_size = repeated_value_array_size_for_type(f->type);
  size_t full_slab_n_elts = contig_full_slab_size / sizeof_elt;
  size_t remaining_elts = s->n_repeated_values;

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
          memcpy (contig_at, at + 1, contig_full_slab_size);
          contig_at += contig_full_slab_size;
          remaining_elts -= full_slab_n_elts;

          // recycle node to a per-parser list
          parser_free_repeated_value_array_list_node (p, at);
        }
    }

  // write the final partial RepeatedValueArrayList
  memcpy (contig_at, partial_arr + 1, remaining_elts * sizeof_elt);

  // recycle final rep_list node
  parser_free_repeated_value_array_list_node (p, partial_arr);

  return true;
}

static bool
json__object_key     (unsigned key_length,
                      const char *key,
                      void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
  (void) key_length;
  if (p->skip_depth > 0)
    {
      /* skip_depth==1 implies that we just got an unknown object key.
       * either we should be a value (which will remove skip_depth==1),
       * or a structured value that'll increase skip_depth by one,
       * so it'll be at least 2 at that point. */
      assert(p->skip_depth != 1);
      return true;
    }
  
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
  ProtobufCMessage *message = s->message;
  const ProtobufCMessageDescriptor *msg_desc = message->descriptor;
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

static bool
json__number_value   (unsigned number_length,
                      const char *number,
                      void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
  const ProtobufCFieldDescriptor *f = s->field_desc;
  void *member = (char *) s->message + f->offset;
  void *qmember = (char *) s->message + f->quantifier_offset;
  void *value = member;
  size_t sizeof_member = sizeof_field_from_type (f->type);
  if (f->label == PROTOBUF_C_LABEL_REPEATED)
    {
      if (s->rep_list_backward != NULL)
        {
          value = parser_allocate_value_from_repeated_value_pool (p, ...);
          ...
        }
      else
        {
          // single unrepeated value
          * (size_t *) qmember = 1;
          * (void **) member = value = parser_alloc (...);
        }
    }
  else if (f->label == PROTOBUF_C_LABEL_OPTIONAL)
    {
      * (protobuf_c_boolean *) qmember = PROTOBUF_C_TRUE;
    }


  ... parse_number_to_value (number, f, value, error_out);
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

static bool
json__string_value   (unsigned string_length,
                      const char *string,
                      void *callback_data)
{
  PBC_JSON_Parser *p = callback_data;
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }

  assert(p->stack_depth > 0);
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
  assert(p->field_desc != NULL);
  ...
}

static bool
json__boolean_value  (int boolean_value,
                      void *callback_data)
{
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
  assert(s->field_desc != NULL);
  void *value = (char*) s->message + s->field_desc->offset;
  int value01 = boolean_value ? 1 : 0;
  switch (s->field_desc->type)
    {
    ...
    }
  return true;
}

static bool
json__null_value     (void *callback_data)
{
  if (p->skip_depth > 0)
    {
      if (p->skip_depth == 1)
        p->skip_depth = 0;
      return true;
    }
  assert(p->stack_depth > 0);
  PBC_JSON_Parser_Stack *s = p->stack + p->stack_depth - 1;
  assert(s->field_desc != NULL);
  void *qmember = (char*) s->message + s->field_desc->quantifier_offset;
  void *member = (char*) s->message + s->field_desc->offset;
  switch (s->field_desc->label)
    {
    case PROTOBUF_C_LABEL_REPEATED:
      ...
    case PROTOBUF_C_LABEL_OPTIONAL:
      ...
    case PROTOBUF_C_LABEL_REQUIRED:
      ...
    }
  int value01 = boolean_value ? 1 : 0;
  switch (s->field_desc->type)
    {
    ...
    }
  return true;
}

static bool
json__error          (const JSON_CallbackParser_ErrorInfo *error,
                      void *callback_data)
{
  ...
}

static void
json__destroy       (void *callback_data)
{
  ...
}

static JSON_Callbacks json_callbacks = JSON_CALLBACKS_DEF(json__, );

PBC_JSON_Parser *
pbc_json_parser_new  (ProtobufCMessageDescriptor  *message_desc,
                      PBC_JSON_ParserFuncs        *funcs,
                      PBC_JSON_ParserOptions      *options,
                      void                        *callback_data)
{
  size_t size = sizeof (PBC_JSON_Parser)
              + sizeof (PBC_JSON_Parser_Stack) * options->max_stack_depth
              + sizeof (Slab)
              + options->estimated_message_size;

  JSON_CallbackParser_Options cb_parser_options;
  switch (options->json_dialect)
    {
      case PBC_JSON_DIALECT_JSON:
        cb_parser_options = JSON_CALLBACK_PARSER_OPTIONS_INIT;
        break;
      case PBC_JSON_DIALECT_JSON5:
        cb_parser_options = JSON_CALLBACK_PARSER_OPTIONS_INIT_JSON5;
        break;
      default:
        return NULL;
    }

  PBC_JSON_Parser *parser = malloc (size);
  parser->json_parser = json_callback_parser_new (&json_callbacks,
                                                  parser,
                                                  &cb_parser_options);
  parser->message_desc = message_desc;
  parser->funcs = *funcs;
  parser->callback_data = callback_data;

  /* If 1, we are in an unknown field of a known object type.
   * If >1, we are skipping and object/array-depth == skip_depth-1
   */
  parser->skip_depth = 0;

  parser->stack_depth = 0;
  parser->max_stack_depth = options->max_stack_depth;
  parser->stack = (PBC_JSON_Parser_Stack *) (parser + 1);

  parser->cur_first = (Slab *) (parser->stack + options->max_stack_depth);
  parser->cur_first->data = (uint8_t *) (parser->cur_first + 1);
  parser->slab_ring = parser->cur_first;
  parser->slab_ring->next = parser->slab_ring;
  parser->slab_used = 0;
  parser->error_code = NULL;
  parser->error_message = NULL;
  parser->recycled_repeated_nodes = NULL;

  return parser;
} 

