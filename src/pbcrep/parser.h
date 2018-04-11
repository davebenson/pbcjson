
#if !defined(PBC_JSON_DO_NOT_INCLUDE_PROTOBUF_C_H)
#  include <protobuf-c/protobuf-c.h>
#endif
#include <stdbool.h>

typedef struct PBC_Parser  PBC_Parser;
#define PBC_PARSER_MAGIC_VALUE 0xa0119afa

typedef struct PBC_Parser_Error {
  const char *error_message;
  const char *error_code;
} PBC_Parser_Error;

typedef struct {
  void (*message_callback) (PBC_Parser             *parser,
                            const ProtobufCMessage *message,
                            void                   *callback_data);
  void (*error_callback)   (PBC_Parser             *parser,
                            const PBC_Parser_Error *error,
                            void                   *callback_data);
  void (*destroy)          (PBC_Parser             *parser,
                            void                   *callback_data);
} PBC_ParserCallbacks;

bool pbc_parser_feed       (PBC_Parser                  *parser,
                            size_t                       data_length,
                            const uint8_t               *data);
bool pbc_parser_end_feed   (PBC_Parser                  *parser);
void pbc_parser_destroy    (PBC_Parser                  *parser);

typedef enum
{
  PBC_PARSER_CONTENT_TYPE_ANY,                // default
  PBC_PARSER_CONTENT_TYPE_TEXT,               // unspecified text format
  PBC_PARSER_CONTENT_TYPE_JSON,
  PBC_PARSER_CONTENT_TYPE_PROTOBUF
} PBC_Parser_ContentType;

struct PBC_Parser {
  uint32_t parser_magic;
  PBC_Parser_ContentType content_type;
  const ProtobufCMessageDescriptor  *message_desc;
  PBC_ParserCallbacks callbacks;
  void *callback_data;

  bool  (*feed)     (PBC_Parser      *parser,
                     size_t           data_length,
                     const uint8_t   *data);
  bool  (*end_feed) (PBC_Parser      *parser);
  void  (*destroy)  (PBC_Parser      *parser);
};

PBC_Parser *
pbc_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                             size_t                       parser_size,
                             PBC_ParserCallbacks         *callbacks,
                             void                        *callback_data);
void
pbc_parser_destroy_protected (PBC_Parser *parser);
