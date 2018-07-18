
#include "../parser.h"

// LE=little-endian BE=big-endian
typedef enum
{
  PBCREP_LENGTH_PREFIXED_UINT8,
  PBCREP_LENGTH_PREFIXED_UINT16_LE,
  PBCREP_LENGTH_PREFIXED_UINT24_LE,
  PBCREP_LENGTH_PREFIXED_UINT32_LE,
  //PBCREP_LENGTH_PREFIXED_UINT48_LE,
  //PBCREP_LENGTH_PREFIXED_UINT64_LE,
  PBCREP_LENGTH_PREFIXED_UINT16_BE,
  PBCREP_LENGTH_PREFIXED_UINT24_BE,
  PBCREP_LENGTH_PREFIXED_UINT32_BE,
  //PBCREP_LENGTH_PREFIXED_UINT48_BE,
  //PBCREP_LENGTH_PREFIXED_UINT64_BE,
  PBCREP_LENGTH_PREFIXED_B128,
  PBCREP_LENGTH_PREFIXED_B128_BE
} PBCREP_LengthPrefixed_Format;

PBCREP_Parser *pbcrep_parser_new_length_prefixed
                                 (PBCREP_LengthPrefixed_Format lp_format,
                                  const ProtobufCMessageDescriptor *desc,
                                  PBCREP_ParserCallbacks   *callbacks,
                                  void                     *callback_data);

