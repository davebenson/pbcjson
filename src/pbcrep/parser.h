
#if !defined(PBCREP_JSON_DO_NOT_INCLUDE_PROTOBUF_C_H)
#  include <protobuf-c/protobuf-c.h>
#endif
#include <stdbool.h>

typedef struct PBC_Parser  PBC_Parser;
#define PBCREP_PARSER_MAGIC_VALUE 0xa0119afa

typedef struct {
  void (*message_callback) (PBCREP_Parser          *parser,
                            const ProtobufCMessage *message,
                            void                   *callback_data);
  void (*error_callback)   (PBCREP_Parser          *parser,
                            const PBCREP_Error     *error,
                            void                   *callback_data);
  void (*destroy)          (PBCREP_Parser          *parser,
                            void                   *callback_data);
} PBCREP_ParserCallbacks;

bool pbcrep_parser_feed       (PBCREP_Parser               *parser,
                               size_t                       data_length,
                               const uint8_t               *data);
bool pbcrep_parser_end_feed   (PBCREP_Parser               *parser);
void pbcrep_parser_destroy    (PBCREP_Parser               *parser);

typedef enum
{
  PBCREP_PARSER_CONTENT_TYPE_ANY,                // default
  PBCREP_PARSER_CONTENT_TYPE_TEXT,               // unspecified text format
  PBCREP_PARSER_CONTENT_TYPE_JSON,
  PBCREP_PARSER_CONTENT_TYPE_PROTOBUF
} PBCREP_Parser_ContentType;

struct PBCREP_Parser {
  uint32_t parser_magic;
  PBCREP_Parser_ContentType content_type;
  const ProtobufCMessageDescriptor  *message_desc;
  PBCREP_ParserCallbacks callbacks;
  void *callback_data;

  bool  (*feed)     (PBCREP_Parser   *parser,
                     size_t           data_length,
                     const uint8_t   *data);
  bool  (*end_feed) (PBCREP_Parser   *parser);
  void  (*destroy)  (PBCREP_Parser   *parser);
};

PBCREP_Parser *
pbcrep_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                                size_t                       parser_size,
                                PBCREP_ParserCallbacks         *callbacks,
                                void                        *callback_data);

/* this must be called from within
 * the parser->destroy method's implementation.
 */
void
pbcrep_parser_destroy_protected (PBCREP_Parser *parser);

