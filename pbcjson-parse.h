#include <...>

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



// === Streaming Record-Reader API ===
PBC_JSON_Parser *pbc_json_parser_new     (ProtobufCMessageDescriptor  *message_desc,
                                          PBC_JSON_ParserFuncs        *funcs,
                                          PBC_JSON_ParserOptions      *options,
                                          void                        *callback_data);
bulp_bool        pbc_json_parser_feed    (PBC_JSON_Parser             *parser,
                                          size_t                       json_data_length,
                                          const uint8_t               *json_data);
void             pbc_json_parser_destroy (PBC_JSON_Parser             *parser);


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

// Example parsing a single record:
//   
// Example parsing a stream of records:
//   
