#ifndef __PBCREP_H_
#error only include pbcrep.h
#endif

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
