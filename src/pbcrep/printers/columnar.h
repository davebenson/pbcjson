/*
 * Tabular Printers.
 *
 * The columns are named and are in a fixed order.
 * 
 * The printers really come in two parts:
 *      (1) a specification that maps fields and subfields of
 *          the ProtobufCMessageDescriptor.
 *
 *          The is called a PBCREP_PrinterTabularConfig.
 *
 *      (2) a converter that takes arrays of strings and prints them.
 *          It has handlers for headers and footers.
 *
 *          The is called a PBCREP_RawTabularPrinter.
 */



typedef struct PBCREP_PrinterTabularConfig PBCREP_PrinterTabularConfig;
typedef struct PBCREP_RawTabularPrinter PBCREP_RawTabularPrinter;

PBCREP_Printer *pbcrep_printer_new_tabsep (ProtobufCMessageDescriptor *desc,
                                           bool                 header,
                                           PBCREP_PrinterTarget target);
PBCREP_Printer *pbcrep_printer_new_csv    (ProtobufCMessageDescriptor *desc,
                                           bool                 header,
                                           PBCREP_PrinterTarget target);


PBCREP_PrinterTabularConfig *
pbcrep_printer_tabular_config_new   (const ProtobufCMessageDescriptor *desc,
                                      const char                 *column1_spec,
                                      ...);
PBCREP_PrinterTabularConfig *
pbcrep_printer_tabular_config_new_v (const ProtobufCMessageDescriptor *desc,
                                      unsigned                    n_columns,
                                      char                      **column_specs);
PBCREP_PrinterTabularConfigFactory *
pbcrep_printer_tabular_config_new_default (const ProtobufCMessageDescriptor *desc);

bool
pbcrep_printer_tabular_config_append (PBCREP_PrinterTabularConfig *config,
                                       const char *spec,
                                       PBCREP_Error **error);

void
pbcrep_printer_tabular_config_destroy (PBCREP_PrinterTabularConfig *config);

PBCREP_Printer *
pbcrep_printer_new_tabular (PBCREP_PrinterTabularConfig *config,
                             PBCREP_RawTabularPrinter    *raw_printer);

struct PBCREP_RawTabularPrinter {
  bool (*begin_print)(PBCREP_RawTabularPrinter *printer,
                      size_t                     n_fields,
                      char                     **field_names,
                      PBCREP_PrinterTarget      *target;
                      PBCREP_Error             **error);
  bool (*print)      (PBCREP_RawTabularPrinter *printer,
                      PBCREP_PrinterTarget      *target;
                      size_t                     n_field,
                      char                     **field_values,
                      PBCREP_Error             **error);
  bool (*end_print)  (PBCREP_RawTabularPrinter *printer,
                      size_t                     n_fields,
                      char                     **field_names,
                      PBCREP_PrinterTarget      *target,
                      PBCREP_Error             **error);
  void (*destroy)    (PBCREP_RawTabularPrinter *printer);
  
  // For our use-case the target is instead the PBCREP_Printer.
};

PBCREP_RawTabularPrinter *pbcrep_raw_tabular_printer_new_tabsep
                                        (PBCREP_PrinterTarget *target,
                                         bool                  print_header);
PBCREP_RawTabularPrinter *pbcrep_raw_tabular_printer_new_csv
                                        (PBCREP_PrinterTarget *target,
                                         bool                  print_header);


