#include <...>

typedef enum
{
  JSON_VALIDATION_IN_BETWEEN_RECORDS,
  JSON_VALIDATION_IN_BETWEEN_RECORDS_GOT_COMMA,
  JSON_VALIDATION_IN_FIELD_NAME,
  JSON_VALIDATION_GOT_FIELD_NAME,
  JSON_VALIDATION_GOT_FIELD_NAME_COLON,
  JSON_VALIDATION_GOT_FIELD_NAME_COLON,
  JSON_VALIDATION_IN_BETWEEN_RECORDS_GOT_COMMA


typedef struct PBC_JSON_ParserError {
  const char *error_message;

  //TODO: stack

  /* For ignorable errors:
   *      returning TRUE from ParserErrorCallback will cause processing to continue.
   *      returning FALSE from ParserErrorCallback will cause pbc_json_parser_feed to return FALSE.
   * For non-ignorable:
   *      the ParserErrorCallback ret-val is ignored, and treated as FALSE.
   */
  unsigned is_ignorable : 1;
} PBC_JSON_ParserError;


typedef struct {
  void (*message_callback) (PBC_JSON_Parser        *parser,
                            const ProtobufCMessage *message,
                            void                   *callback_data);
  void (*error_callback)   (PBC_JSON_Parser        *parser,
                            const PBC_JSON_ParserError *error,
                            void                   *callback_data);
  void (*destroy)          (PBC_JSON_Parser        *parser,
                            void                   *callback_data);
} PBC_JSON_ParserFuncs;


typedef struct PBC_JSON_ParserOptions PBC_JSON_ParserOptions;

struct PBC_JSON_ParserOptions {
  unsigned max_stack_depth;                     // max nesting level for objects/arrays
};

#define PBC_JSON_PARSER_OPTIONS_INIT { \
  32            /* max_stack_depth */ \
}


struct PBC_JSON_Parser {
  ProtobufCMessageDescriptor *message_desc;

  PBC_JSON_ParserFuncs funcs;
  void *callback_data;

  unsigned stack_depth;
  unsigned max_stack_depth
  PBC_JSON_Parser_Stack *stack;
};

static PBC_JSON_Parser *
pbc_json_parser_new  (ProtobufCMessageDescriptor  *message_desc,
                      PBC_JSON_ParserFuncs        *funcs,
                      PBC_JSON_ParserOptions      *options,
                      void                        *callback_data)
{
  PBC_JSON_Parser *parser = memory;
  JSON_Callback_Parser *subp = json_cal

  parser->message_desc = message_desc;
  parser->funcs = *funcs;
  parser->callback_data = callback_data;
  parser->stack_depth = 64;
  parser->max_stack_depth = options->max_stack_depth;
  parser->stack = (PBC_JSON_Parser_Stack *) (parser + 1);

  return parser;
} 

PBC_JSON_Parser *
pbc_json_parser_new     (ProtobufCMessageDescriptor  *message_desc,
                         PBC_JSON_ParserFuncs        *funcs,
                         PBC_JSON_ParserOptions      *options,
                         void                        *callback_data)
{
  return pbc_json_parser_init (malloc (pbc_json_parser_sizeof (options),
                               message_desc, funcs, options, callback_data);
}


bulp_bool
pbc_json_parser_feed    (PBC_JSON_Parser             *parser,
                         size_t                       json_data_length,
                         const uint8_t               *json_data)
{
  if (parser->leftover_length > 0)
    {
      // move onto buffer
      ...

      json_data_length = parser->leftover_length;
      json_data = parser->leftover;
    }
  else
    {
      ...
    }
}


void
pbc_json_parser_destroy (PBC_JSON_Parser   *parser)
{
  if (parser->funcs.destroy != NULL)
    parser->funcs.destroy (parser, parser->callback_data);
  free (parser);
}

#if 0
// === One-Record API ===
// TL;DR: clear 'pool' after you are done
// NOTE: the return value ProtobufCMessage may point into json_data.
//       otherwise, the ProtobufCMessage is entirely contained in memory managed
//       by 'pool'.
ProtobufCMessage* pbc_json_parse         (ProtobufCMessageDescriptor  *message_desc,
                                          ssize_t                      json_data_length,
                                          const uint8_t               *json_data,
                                          BulpMemPool                 *pool,
                                          PBC_JSON_ParserError       **error_out);
void              pbc_json_parse_free    (ProtobufCMessage            *msg);
#endif

// Example parsing a single record:
//   
// Example parsing a stream of records:
//   
