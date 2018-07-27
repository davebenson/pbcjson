//
// PBCREP_Printer
//
// Render to a generic binary data-sink.
//

typedef struct PBCREP_Printer  PBCREP_Printer;
#define PBCREP_PRINTER_MAGIC_VALUE 0xb011aafb

// API for PBCREP_Printer.
bool pbcrep_printer_print    (PBCREP_Printer *printer,
                              const ProtobufCMessage *message,
                              PBCREP_Error **error);
bool pbcrep_printer_close    (PBCREP_Printer *printer,
                              PBCREP_Error **error);
bool pbcrep_printer_is_closed(PBCREP_Printer *printer);
void pbcrep_printer_destroy  (PBCREP_Printer *printer);


// Callback-driven API for pushing to the binary backend.
//
// This structure can be initialized by methods
// like pbcrep_printer_target_make_stdio(),
// pbcrep_printer_target_make_fixed_length().
typedef struct {
  bool (*append) (PBCREP_Printer *printer,
                  size_t          data_length,
                  const uint8_t  *data,
                  void           *callback_data,
                  PBCREP_Error  **error);
  bool (*close)  (PBCREP_Printer *printer,
                  void           *callback_data,
                  PBCREP_Error  **error);
  void (*destroy)(PBCREP_Printer *printer);
  void *callback_data;
} PBCREP_PrinterTarget;

typedef enum
{
  PBCREP_STDIO_CLOSE_METHOD_NONE,               // close method is a no-op
  PBCREP_STDIO_CLOSE_METHOD_FCLOSE,             // call fclose()
  PBCREP_STDIO_CLOSE_METHOD_PCLOSE              // call pclose()
} PBCREP_StdioCloseMethod;

#if PBCREP_SUPPORTS_STDIO
PBCREP_PrinterTarget
pbcrep_printer_target_make_stdio (FILE *fp,
                                  PBCREP_StdioCloseMethod close_method);
#endif

PBCREP_PrinterTarget
pbcrep_printer_target_make_fixed_length (size_t max_length,
                                         size_t *used_length_out,
                                         uint8_t *data_out);

struct PBCREP_Printer
{
  uint32_t printer_magic;
  const ProtobufCMessageDescriptor  *message_desc;
  PBCREP_PrinterTarget target;
  unsigned end_print_called : 1;
  unsigned end_print_failed : 1;
  unsigned close_called : 1;
  unsigned close_failed : 1;
  bool (*print)    (PBCREP_Printer *printer,
                    const ProtobufCMessage *message,
                    PBCREP_Error **error);
  bool (*end_print)(PBCREP_Printer *printer,
                    PBCREP_Error **error);
};

PBCREP_Printer *pbcrep_printer_create_protected
                               (const ProtobufCMessageDescriptor*message_desc,
                                size_t                       printer_size,
                                PBCREP_PrinterTarget        *target);

