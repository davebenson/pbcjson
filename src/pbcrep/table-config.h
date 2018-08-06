
typedef struct PBCREP_TableConfig PBCREP_TableConfig;

/*
   A complicated TSV spec:
       tsv:no_header:field=name:field=phones[0].number->pn1:field=phones[1].number->pn1

   A default CSV spec:
       csv:repcount=name=3

 */

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
pbcrep_table_config_add_column (PBCREP_TableConfig *config,
                                const char         *spec,
                                PBCREP_FieldParser  parser,
                                PBCREP_FieldPrinter printer,
                                PBCREP_Error      **error);


/* Make an immutable configuration.
 * This is shared between copy commands if
 * the mutable config doesn't change.
 */
PBCREP_TableConfig *
pbcrep_table_config_copy (PBCREP_TableConfig *);

bool
pbcrep_table_config_is_immutable (PBCREP_TableConfig *config);


#ifdef PBCREP_TABLE_CONFIG_DECLARE_INTERNALS
struct PBCREP_TableConfig
{
  ProtobufCMessageDescriptor *desc;
  bool is_mutable;
  unsigned ref_count;
  unsigned n_fields;
  PBCREP_TableField *fields;
  PBCREP_TableFieldAccessor *accessor_heap;
};

PBCREP_TableConfig *
pbcrep_table_config_destroy (PBCREP_TableConfig *);


struct PBCREP_TableField
{
  size_t first_accessor;
  char *name;
  void *fallback_value;
  ProtobufCFieldType field_type;
  void *field_type_descriptor;
}
typedef enum
{
  PBCREP_FIELD_ACCESSOR_TYPE_OFFSET,
  PBCREP_FIELD_ACCESSOR_TYPE_ARRAY,
  PBCREP_FIELD_ACCESSOR_TYPE_OPTIONAL
} PBCREP_FieldAccessorType;
struct PBCREP_FieldAccessor
{
  PBCREP_FieldAccessorType type;
  union {
    size_t offset;
    struct {
      uint32_t quantifier_offset;
      uint32_t offset;
      uint32_t sizeof_elt;
      uint32_t index;
    } array;
    struct {
      uint32_t quantifier_offset;
      uint32_t offset;
    } optional;
  } info;
};
#endif
