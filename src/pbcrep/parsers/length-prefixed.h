
#include "../parser.h"

// LE=little-endian BE=big-endian
typedef enum
{
  PBC_LENGTH_PREFIXED_UINT8,
  PBC_LENGTH_PREFIXED_UINT16_LE,
  PBC_LENGTH_PREFIXED_UINT24_LE,
  PBC_LENGTH_PREFIXED_UINT32_LE,
  //PBC_LENGTH_PREFIXED_UINT48_LE,
  //PBC_LENGTH_PREFIXED_UINT64_LE,
  PBC_LENGTH_PREFIXED_UINT16_BE,
  PBC_LENGTH_PREFIXED_UINT24_BE,
  PBC_LENGTH_PREFIXED_UINT32_BE,
  //PBC_LENGTH_PREFIXED_UINT48_BE,
  //PBC_LENGTH_PREFIXED_UINT64_BE,
  PBC_LENGTH_PREFIXED_B128,
  PBC_LENGTH_PREFIXED_B128_BE
} PBC_LengthPrefixed_Format;

PBC_Parser *pbc_parser_new_length_prefixed
                                       (PBC_LengthPrefixed_Format lp_format,
                                        const ProtobufCMessageDescriptor *desc,
                                        PBC_ParserCallbacks         *callbacks,
                                        void                        *callback_data);

