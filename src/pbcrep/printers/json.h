
typedef struct PBCREP_JSON_PrinterOptions PBCREP_JSON_PrinterOptions;
struct PBCREP_JSON_PrinterOptions
{
  unsigned quoteless_keys : 1;
  unsigned formatted : 1;

  // if formatted
  unsigned indent_size;
};

PBCREP_Printer *pbcrep_printer_new_json (ProtobufCMessageDescriptor *desc,
                                         PBCREP_JSON_PrinterOptions *options);


