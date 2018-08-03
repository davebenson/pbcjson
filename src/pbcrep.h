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

PBCREP_Printer *pbcrep_printer_new (const char *rep_str_spec,
                                    ProtobufCMessageDescriptor *desc,
                                    PBCREP_PrinterTarget target,
                                    PBCREP_Error     **error);
PBCREP_Parser  *pbcrep_parser_new  (const char      *rep_str_spec,
                                    ProtobufCMessageDescriptor *desc,
                                    PBCREP_ParserTarget target,
                                    PBCREP_Error     **error);

/* Various parsers. */
#include "pbcrep/parsers/json.h"
#include "pbcrep/parsers/length-prefixed.h"
#include "pbcrep/parsers/columnar.h"

/* Various printers. */
#include "pbcrep/printers/json.h"
#include "pbcrep/printers/length-prefixed.h"
#include "pbcrep/printers/columnar.h"

#endif
