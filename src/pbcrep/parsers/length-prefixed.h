#ifndef __PBCREP_H_
#error only include pbcrep.h
#endif

//
// PBCREP_LengthPrefixed_Format
//
// LE=little-endian BE=big-endian
//
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

//
// pbc_parser_new_length_prefixed()
//
// Create a parser that uses prefixes to provide the message's length.
//
PBCREP_Parser *pbcrep_parser_new_length_prefixed
                                 (PBCREP_LengthPrefixed_Format lp_format,
                                  const ProtobufCMessageDescriptor *desc);


//
// pbcrep_parser_length_prefixed_set_format()
//
// This should only be called from your message callback, target.callback.
//
void pbcrep_parser_length_prefixed_set_format
                                 (PBCREP_Parser *parser,
                                  PBCREP_LengthPrefixed_Format format);
