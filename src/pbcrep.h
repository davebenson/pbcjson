#ifndef __PBCREP_H_
#define __PBCREP_H_

#include <stdbool.h>
#include <protobuf-c/protobuf-c.h>

#define PBCREP_SUPPORTS_STDIO 1
#include <stdio.h>

/* Include structures common to the printers and parsers. */
#include "pbcrep/error.h"

/* Define the interfaces for parsing and printing. */
#include "pbcrep/parser.h"
#include "pbcrep/printer.h"

#include "pbcrep/reader.h"
#include "pbcrep/writer.h"

PBCREP_Parser  *pbcrep_try_make_parser (const char      *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_ParserTarget target,
                                         PBCREP_Error     **error);
PBCREP_Parser  *pbcrep_make_parser      (const char      *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_ParserTarget target);
PBCREP_Printer *pbcrep_try_make_printer (const char *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_PrinterTarget target,
                                         PBCREP_Error     **error);
PBCREP_Printer *pbcrep_make_printer     (const char *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_PrinterTarget target);



PBCREP_Reader  *pbcrep_try_file_reader  (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr,
                                         PBCREP_Error     **error);
PBCREP_Reader  *pbcrep_make_file_reader (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr);
PBCREP_Writer  *pbcrep_try_file_writer  (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr,
                                         PBCREP_Error     **error);
PBCREP_Writer  *pbcrep_make_file_writer (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr);

typedef enum {
  PBCREP_TRANSFER_RESULT_SUCCESS,
  PBCREP_TRANSFER_RESULT_READ_FAILED,
  PBCREP_TRANSFER_RESULT_WRITE_FAILED
} PBCREP_TransferResultCode;
typedef struct {
  PBCREP_TransferResultCode code;
  PBCREP_Error *error;
  uint64_t n_transferred;
} PBCREP_TransferResult;


PBCREP_TransferResult pbcrep_try_transfer_messages (PBCREP_Reader *input,
                                                    PBCREP_Writer *output,
                                                    PBCREP_Error **error);
PBCREP_TransferResult pbcrep_transfer_messages (PBCREP_Reader *input,
                                                PBCREP_Writer *output);


/* Various parsers. */
#include "pbcrep/parsers/json.h"
#include "pbcrep/parsers/length-prefixed.h"
#include "pbcrep/parsers/columnar.h"

/* Various printers. */
#include "pbcrep/printers/json.h"
#include "pbcrep/printers/length-prefixed.h"
#include "pbcrep/printers/columnar.h"

#endif
