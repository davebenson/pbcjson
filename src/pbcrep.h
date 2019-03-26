#ifndef __PBCREP_H_
#define __PBCREP_H_

#include <stdbool.h>
#include <protobuf-c/protobuf-c.h>

#define PBCREP_SUPPORTS_STDIO 1
#include <stdio.h>

/* --- enums --- */

/* How to destroy a "FILE *" argument from standard-io.  */
typedef enum
{
  PBCREP_STDIO_CLOSE_METHOD_NONE,               // close method is a no-op
  PBCREP_STDIO_CLOSE_METHOD_FCLOSE,             // call fclose()
  PBCREP_STDIO_CLOSE_METHOD_PCLOSE              // call pclose()
} PBCREP_StdioCloseMethod;


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
// The result of a read operation.
//
typedef enum
{
  PBCREP_READ_RESULT_OK,
  PBCREP_READ_RESULT_EOF,
  PBCREP_READ_RESULT_BLOCKED,
  PBCREP_READ_RESULT_ERROR
} PBCREP_ReadResult;


/* Include structures common to the printers and parsers. */
#include "pbcrep/feature-macros.h"
#include "pbcrep/inline.h"
#include "pbcrep/error.h"
#include "pbcrep/buffer.h"

/* ****************************
 * Define the interfaces for parsing and printing.
 * **************************** */

/* --- lowest level --- */
/* Parsers convert binary-data into streams of messages. */
#include "pbcrep/parser.h"
/* Printers convert messages into binary-data. */
#include "pbcrep/printer.h"

// binary data handling
#include "pbcrep/binary-data-reader.h"
#include "pbcrep/binary-data-writer.h"

// message-based record-driven files
#include "pbcrep/reader.h"
#include "pbcrep/writer.h"

// Parsers convert binary-data -> messages.
// Printers convert messages -> binary data.

PBCREP_Parser  *pbcrep_try_make_parser  (const char                       *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_Error                    **error);
PBCREP_Parser  *pbcrep_make_parser      (const char                       *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc);
PBCREP_Printer *pbcrep_try_make_printer (const char                       *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc,
                                         PBCREP_Error                    **error);
PBCREP_Printer *pbcrep_make_printer     (const char                       *rep_str_spec,
                                         const ProtobufCMessageDescriptor *desc);



PBCREP_Reader  *pbcrep_try_file_reader  (const char                       *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char                       *repstr,
                                         PBCREP_Error                    **error);
PBCREP_Reader  *pbcrep_make_file_reader (const char                       *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char                       *repstr);
PBCREP_Writer  *pbcrep_try_file_writer  (const char                       *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char                       *repstr,
                                         PBCREP_Error                    **error);
PBCREP_Writer  *pbcrep_make_file_writer (const char                       *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char                       *repstr);


/*
 * Single-Message File Reader/Writer.
EXPECTED_STRUCTURED_VALUE *
 */
ProtobufCMessage *pbcrep_message_from_file(const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr);
ProtobufCMessage *pbcrep_try_message_from_file
                                        (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr,
                                         PBCREP_Error **error);
void            pbcrep_free_message_from_file (ProtobufCMessage *message);
void            pbcrep_message_to_file  (const char *filename,
                                         const ProtobufCMessageDescriptor *desc,
                                         const char *repstr);
bool            pbcrep_try_message_to_file
                                        (const char *filename,
                                         const ProtobufCMessage *message,
                                         const char *repstr,
                                         PBCREP_Error **error);


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


/* Allocator configuration */

//
// These are set to normal defaults (malloc and free).
//
// If you wish to set them, either manually or with pbcrep_setup_debug_allocator(),
// you must do that before calling any other pbcrep function.
// 
extern void *(*pbcrep_malloc) (size_t size);
extern void  (*pbcrep_free)   (void *allocation);
extern void *(*pbcrep_realloc)(void *ptr, size_t new_size);


// set malloc/free for debugging.
void pbcrep_setup_debug_allocator(void);
#endif
