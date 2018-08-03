
typedef struct PBCREP_TableConfig PBCREP_TableConfig;
typedef struct PBCREP_TableImmutableConfiguration PBCREP_TableImmutableConfiguration;

PBCREP_TableConfig *
pbcrep_table_config_new (const ProtobufCMessageDescriptor *desc);

PBCREP_TableConfig *
pbcrep_table_config_new_default (const ProtobufCMessageDescriptor *desc);

/*
 *     spec ::= member_spec opt_as_clause
 *     member_spec ::= BAREWORD
 *                   | member_spec DOT BAREWORD
 *                   | member_spec LBRACE NUMBER RBRACE
 *     opt_as_clause ::= 
 *                   | AS BAREWORD
 */
bool
pbcrep_table_config_add_column_spec (PBCREP_TableConfig *config,
                                     const char         *spec,
                                     PBCREP_FieldParser  parser,
                                     PBCREP_FieldPrinter printer,
                                     PBCREP_Error      **error);



/* --- interfaces for printers and parsers, which want immutable configs --- */
void
pbcrep_table_immutable_configuration_ref (PBCREP_TableImmutableConfiguration*);
void
pbcrep_table_immutable_configuration_unref (PBCREP_TableImmutableConfiguration*);
