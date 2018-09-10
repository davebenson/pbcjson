//
// PBCREP_Printer
//
// Render to an in-memory buffer that can be drained at any time.
//

typedef struct PBCREP_Printer  PBCREP_Printer;
#define PBCREP_PRINTER_MAGIC_VALUE 0xb011aafb

// API for PBCREP_Printer.
bool pbcrep_printer_print    (PBCREP_Printer *printer,
                              const ProtobufCMessage *message,
                              PBCREP_Error **error);
bool pbcrep_printer_end      (PBCREP_Printer *printer,
                              PBCREP_Error **error);
bool pbcrep_printer_is_ended (PBCREP_Printer *printer);
void pbcrep_printer_destroy  (PBCREP_Printer *printer);


struct PBCREP_Printer
{
  uint32_t magic;

  bool (*print)    (PBCREP_Printer *printer,
                    const ProtobufCMessage *message,
                    PBCREP_Error **error);
  bool (*end_print)(PBCREP_Printer *printer,
                    PBCREP_Error **error);
  void (*destroy)  (PBCREP_Printer *printer);

  PBCREP_Buffer output_data;
  bool ended;
};

PBCREP_Printer *pbcrep_printer_new_protected (size_t sizeof_printer);

