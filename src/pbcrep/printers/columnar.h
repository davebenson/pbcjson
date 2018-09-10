/*
 * Columnar Printers.
 *
 * The columns are named and are in a fixed order.
 * 
 * The printers really come in two parts:
 *      (1) a specification that maps fields and subfields of
 *          the ProtobufCMessageDescriptor.
 *
 *          The is called a PBCREP_PrinterColumnarConfig.
 *
 *      (2) a converter that takes arrays of strings and prints them.
 *          It has handlers for headers and footers.
 *
 *          The is called a PBCREP_RawColumnarPrinter.
 */



typedef struct PBCREP_PrinterColumnarConfig PBCREP_PrinterColumnarConfig;
typedef struct PBCREP_RawColumnarPrinter PBCREP_RawColumnarPrinter;

PBCREP_Printer *pbcrep_printer_new_tabsep (ProtobufCMessageDescriptor *desc,
                                           bool                 header);
PBCREP_Printer *pbcrep_printer_new_csv    (ProtobufCMessageDescriptor *desc,
                                           bool                 header);


PBCREP_PrinterColumnarConfig *
pbcrep_printer_columnar_config_new   (const ProtobufCMessageDescriptor *desc,
                                      const char                 *column1_spec,
                                      ...);
PBCREP_PrinterColumnarConfig *
pbcrep_printer_columnar_config_new_v (const ProtobufCMessageDescriptor *desc,
                                      unsigned                    n_columns,
                                      char                      **column_specs);


void
pbcrep_printer_columnar_config_unref  (PBCREP_PrinterColumnarConfig *config);

PBCREP_Printer *
pbcrep_printer_new_columnar (PBCREP_PrinterColumnarConfig *config,
                             PBCREP_RawColumnarPrinter    *raw_printer);

struct PBCREP_RawColumnarPrinter {
  bool (*begin_print)(PBCREP_RawColumnarPrinter *printer,
                      size_t                     n_fields,
                      char                     **field_names,
                      PBCREP_PrinterTarget      *target,
                      PBCREP_Error             **error);
  bool (*print)      (PBCREP_RawColumnarPrinter *printer,
                      PBCREP_PrinterTarget      *target,
                      size_t                     n_field,
                      char                     **field_values,
                      PBCREP_Error             **error);
  bool (*end_print)  (PBCREP_RawColumnarPrinter *printer,
                      size_t                     n_fields,
                      char                     **field_names,
                      PBCREP_PrinterTarget      *target,
                      PBCREP_Error             **error);
  void (*destroy)    (PBCREP_RawColumnarPrinter *printer);
  
  // For our use-case the target is instead the PBCREP_Printer.
};

PBCREP_RawColumnarPrinter *pbcrep_raw_columnar_printer_new_tabsep
                                        (PBCREP_PrinterTarget *target,
                                         bool                  print_header);
PBCREP_RawColumnarPrinter *pbcrep_raw_columnar_printer_new_csv
                                        (PBCREP_PrinterTarget *target,
                                         bool                  print_header);


