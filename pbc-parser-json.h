#ifndef __PBC_PARSER_JSON_H_
#define __PBC_PARSER_JSON_H_

typedef struct PBC_JSON_ParserOptions PBC_JSON_ParserOptions;

#include "pbc-parser.h"

typedef enum
{
  PBC_JSON_DIALECT_JSON = 0,            // the default, of course
  PBC_JSON_DIALECT_JSON5                // see http://json5.org/
} PBC_JSON_Dialect;

struct PBC_JSON_ParserOptions {
  // max nesting level for objects/arrays
  unsigned max_stack_depth;

  PBC_JSON_Dialect json_dialect;
};

#define PBC_JSON_PARSER_OPTIONS_INIT { \
  64,            /* max_stack_depth */  \
  PBC_JSON_DIALECT_JSON \
}



// === Streaming Record-Reader API ===
PBC_Parser       *pbc_parser_new_json   (ProtobufCMessageDescriptor  *message_desc,
                                         const PBC_JSON_ParserOptions*json_options,
                                         PBC_ParserCallbacks         *callbacks,
                                         void                        *callback_data);

#endif
