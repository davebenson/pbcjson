#include <stdlib.h>
#include "../pbcrep.h"


bool
pbcrep_parser_feed       (PBCREP_Parser               *parser,
                          size_t                       data_length,
                          const uint8_t               *data,
                          PBCREP_Error               **error)
{
  return parser->feed(parser, data_length, data, error);
}
bool
pbcrep_parser_end_feed   (PBCREP_Parser               *parser,
                          PBCREP_Error               **error)
{
  return parser->end_feed(parser, error);
}
void
pbcrep_parser_destroy    (PBCREP_Parser               *parser)
{
  if (parser->destruct != NULL)
    parser->destruct(parser);

  // Now, undo any work done by
  // pbc_parser_create_protected().
  free (parser);
}

PBCREP_Parser *
pbcrep_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                                size_t                       parser_size)
{
  assert(parser_size >= sizeof(struct PBCREP_Parser));
  PBCREP_Parser *rv = malloc (parser_size);
  assert(rv != NULL);
  rv->parser_magic = PBCREP_PARSER_MAGIC_VALUE;
  rv->content_type = PBCREP_PARSER_CONTENT_TYPE_ANY;
  rv->message_desc = message_desc;
  rv->feed = NULL;
  rv->end_feed = NULL;
  rv->destruct = NULL;
  return rv;
}

