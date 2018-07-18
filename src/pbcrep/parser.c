#include <stdlib.h>
#include "parser.h"


bool pbc_parser_feed       (PBC_Parser                  *parser,
                            size_t                       data_length,
                            const uint8_t               *data)
{
  return parser->feed(parser, data_length, data);
}
bool pbc_parser_end_feed   (PBC_Parser                  *parser)
{
  return parser->end_feed(parser);
}
void pbc_parser_destroy    (PBC_Parser                  *parser)
{
  parser->destroy(parser);
}

static void
pbc_parser_destroy 

PBC_Parser *
pbc_parser_create_protected (const ProtobufCMessageDescriptor*message_desc,
                             size_t                       parser_size,
                             PBC_ParserCallbacks         *callbacks,
                             void                        *callback_data)
{
  assert(parser_size >= sizeof(struct PBC_Parser));
  PBC_Parser *rv = malloc (parser_size);
  assert(rv != NULL);
  rv->parser_magic = PBC_PARSER_MAGIC_VALUE;
  rv->content_type = PBC_PARSER_CONTENT_TYPE_ANY;
  rv->message_desc = message_desc;
  rv->callbacks = *callbacks;
  rv->callback_data = callback_data;
  rv->feed = NULL;
  rv->end_feed = NULL;
  rv->destroy = pbcrep_parser_destroy;
  return rv;
}
