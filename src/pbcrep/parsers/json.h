#ifndef __PBCREP_H_
#error only include pbcrep.h
#endif

typedef struct PBCREP_Parser_JSONOptions PBCREP_Parser_JSONOptions;

typedef enum
{
  PBCREP_JSON_DIALECT_JSON = 0,            // the default, of course
  PBCREP_JSON_DIALECT_JSON5                // see http://json5.org/
} PBCREP_JSON_Dialect;

struct PBCREP_Parser_JSONOptions {
  // max nesting level for objects/arrays
  unsigned max_stack_depth;

  PBCREP_JSON_Dialect json_dialect;

  size_t estimated_message_size;
};

#define PBCREP_PARSER_JSON_OPTIONS_INIT                              \
  (PBCREP_Parser_JSONOptions) {                                      \
    64,                     /* max_stack_depth */                    \
    PBCREP_JSON_DIALECT_JSON,                                        \
    512                     /* estimated_message_size */             \
  }



// === Streaming Record-Reader API ===
PBCREP_Parser *
pbcrep_parser_new_json  (const ProtobufCMessageDescriptor  *message_desc,
                         const PBCREP_Parser_JSONOptions*json_options,
                         PBCREP_ParserTarget         target);


bool
pbcrep_parser_is_json   (PBCREP_Parser *parser);
