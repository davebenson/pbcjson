
typedef struct PBCREP_Parser  PBCREP_Parser;
#define PBCREP_PARSER_MAGIC_VALUE 0xa0119afa

bool pbcrep_parser_feed       (PBCREP_Parser               *parser,
                               size_t                       data_length,
                               const uint8_t               *data,
                               PBCREP_Error               **error);
bool pbcrep_parser_end_feed   (PBCREP_Parser               *parser,
                               PBCREP_Error               **error);
void pbcrep_parser_advance    (PBCREP_Parser               *parser);
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
  ProtobufCMessage *current_message;

  bool  (*feed)     (PBCREP_Parser   *parser,
                     size_t           data_length,
                     const uint8_t   *data,
                     PBCREP_Error   **error);
  bool  (*end_feed) (PBCREP_Parser   *parser,
                     PBCREP_Error   **error);
  bool  (*advance)  (PBCREP_Parser   *parser,
                     PBCREP_Error   **error);
  void  (*destruct) (PBCREP_Parser   *parser);
};

PBCREP_Parser *
pbcrep_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                                size_t                       parser_size);

