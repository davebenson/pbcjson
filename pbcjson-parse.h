#ifndef __PBC_JSON_PARSER_H_
#define __PBC_JSON_PARSER_H_

#if !defined(PBC_JSON_DO_NOT_INCLUDE_PROTOBUF_C_H)
#  include <protobuf-c.h>
#endif

typedef struct PBC_JSON_ParserError {
  const char *error_message;

  PBC_JSON_ParserErrorCode error_code;

  //TODO: stack
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
} PBC_ParserCallbacks;


typedef struct PBC_JSON_ParserOptions PBC_JSON_ParserOptions;

struct PBC_JSON_ParserOptions {
  unsigned max_stack_depth;                     // max nesting level for objects/arrays
};

#define PBC_JSON_PARSER_OPTIONS_INIT { \
  64            /* max_stack_depth */  \
}



// === Streaming Record-Reader API ===
PBC_Parser *pbc_parser_new_json         (ProtobufCMessageDescriptor  *message_desc,
                                         PBC_JSON_ParserCallbacks    *callbacks,
                                         void                        *callback_data);
PBC_Parser *pbc_parser_new_json5        (ProtobufCMessageDescriptor  *message_desc,
                                         PBC_JSON_ParserCallbacks    *callbacks,
                                         void                        *callback_data);
PBC_Parser *pbc_parser_new_json_variant (ProtobufCMessageDescriptor  *message_desc,
                                         PBC_JSON_ParserCallbacks    *callbacks,
                                         void                        *callback_data,
                                         PBC_JSON_ParserOptions      *options);

PBC_ParseResult   pbc_parser_feed       (PBC_Parser                  *parser,
                                         size_t                       data_length,
                                         const uint8_t               *data);
PBC_EndResult     pbc_parser_end_feed   (PBC_Parser                  *parser,
                                         size_t                       data_length,
                                         const uint8_t               *data);
void              pbc_parser_destroy    (PBC_Parser                  *parser);



struct PBC_Parser {
  uint32_t parser_magic;
  uint32_t parser_type;
  PBC_ParserCallbacks callbacks;
  void *callback_data;
  ProtobufCMessageDescriptor  *message_desc;

  PBC_ParseResult  (*feed)    (PBC_Parser      *parser,
                               size_t           json_data_length,
                               const uint8_t   *json_data);
  PBC_EndResult    (*end_feed)(PBC_Parser      *parser);
  void             (*destroy) (PBC_Parser      *parser);
};

PBC_Parser *pbc_parser_create_protected (ProtobufCMessageDescriptor  *message_desc,
                                         PBC_JSON_ParserCallbacks    *callbacks,
                                         PBC_JSON_ParserOptions      *options,
                                         void                        *callback_data);

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

#endif
