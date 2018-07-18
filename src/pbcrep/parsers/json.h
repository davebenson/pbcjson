#ifndef __PBC_PARSER_JSON_H_
#define __PBC_PARSER_JSON_H_

typedef struct PBC_Parser_JSONOptions PBC_Parser_JSONOptions;

#include "../parser.h"

typedef enum
{
  PBC_JSON_DIALECT_JSON = 0,            // the default, of course
  PBC_JSON_DIALECT_JSON5                // see http://json5.org/
} PBC_JSON_Dialect;

struct PBC_Parser_JSONOptions {
  // max nesting level for objects/arrays
  unsigned max_stack_depth;

  PBC_JSON_Dialect json_dialect;

  size_t estimated_message_size;
};

#define PBC_PARSER_JSON_OPTIONS_INIT                                 \
  (PBC_Parser_JSONOptions) {                                         \
    64,                     /* max_stack_depth */                    \
    PBC_JSON_DIALECT_JSON,                                           \
    512                     /* estimated_message_size */             \
  }



// === Streaming Record-Reader API ===
PBC_Parser       *pbc_parser_new_json   (const ProtobufCMessageDescriptor  *message_desc,
                                         const PBC_Parser_JSONOptions*json_options,
                                         PBC_ParserCallbacks         *callbacks,
                                         void                        *callback_data);

#endif
