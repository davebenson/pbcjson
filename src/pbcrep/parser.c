#include <stdlib.h>
#include "../pbcrep.h"


bool
pbcrep_parser_feed       (PBCREP_Parser               *parser,
                          size_t                       data_length,
                          const uint8_t               *data)
{
  return parser->feed(parser, data_length, data);
}
bool
pbcrep_parser_end_feed   (PBCREP_Parser               *parser)
{
  return parser->end_feed(parser);
}
void
pbcrep_parser_destroy    (PBCREP_Parser               *parser)
{
  parser->destroy(parser);

  if (parser->target.destroy != NULL)
    parser->target.destroy (parser, parser->target.callback_data);

  // Now, undo any work done by
  // pbc_parser_create_protected().
  free (parser);
}

PBCREP_Parser *
pbcrep_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                                size_t                       parser_size,
                                PBCREP_ParserTarget          target)
{
  assert(parser_size >= sizeof(struct PBCREP_Parser));
  PBCREP_Parser *rv = malloc (parser_size);
  assert(rv != NULL);
  rv->parser_magic = PBCREP_PARSER_MAGIC_VALUE;
  rv->content_type = PBCREP_PARSER_CONTENT_TYPE_ANY;
  rv->message_desc = message_desc;
  rv->target = target;
  rv->feed = NULL;
  rv->end_feed = NULL;
  rv->destroy = pbcrep_parser_destroy;
  return rv;
}

