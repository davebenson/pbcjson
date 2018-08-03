
typedef struct PBCREP_RawColumnarParser PBCREP_RawColumnarParser;

/*
 * Configuring a new PBCREP_Parser that uses a columnar format.
 */
typedef enum
{
  PBCREP_PARSER_COLUMNAR_MODE_TABSEP,
  PBCREP_PARSER_COLUMNAR_MODE_CSV,
  PBCREP_PARSER_COLUMNAR_MODE_CUSTOM
} PBCREP_Parser_ColumnarMode;

typedef enum {
  PBCREP_PARSER_CSV_HEADER_IGNORE,
  PBCREP_PARSER_CSV_HEADER_OBEY,
  PBCREP_PARSER_CSV_HEADER_MISMATCH_WARN,
  PBCREP_PARSER_CSV_HEADER_DOES_NOT_EXIST,
  PBCREP_PARSER_CSV_HEADER_OPTIONAL_IGNORE,
  PBCREP_PARSER_CSV_HEADER_OPTIONAL_OBEY,
  PBCREP_PARSER_CSV_HEADER_OPTIONAL_MISMATCH_WARN
} PBCREP_Parser_CSV_HeaderMode;

typedef struct {
  PBCREP_Parser_ColumnarMode mode;
  PBCREP_Parser_CSV_HeaderMode header_mode;
  ProtobufCMessage *initial_message;
  char **field_specs;
  PBCREP_RawColumnarParser *custom_columnar_parser;
} PBCREP_ParserColumnarConfig;

void pbcrep_parser_new_columnar (PBCREP_ParserColumnarConfig config);



/* --- Custom Columnar formats --- */
/* To implement a custom columnar format,
 * you must provide an "object" that matches
 * PBCREP_RawColumnarParser.
 *
 * Your 'feed' method should call 'handler' whenever a new
 * row is finished.
 *
 * 'handler' and 'handler_data' are set by pbcrep_parser_new_columnar();
 * they must both be NULL when this function is called.
 */

/*
 * For Custom Columnar data types.
 *
 * This converts raw binary data into rows of columns;
 * each column is a string.
 */
typedef bool (*PBCREP_RawColumnarParserHandler)
                        (PBCREP_RawColumnarParser *parser,
                         size_t                 n_columns,
                         char                 **columns,
                         void                  *handler_data);

struct PBCREP_RawColumnarParser
{
  bool (*feed)     (PBCREP_RawColumnarParser *parser,
                    size_t                 len,
                    const uint8_t         *data,
                    PBCREP_Error         **error);
  bool (*end_feed) (PBCREP_RawColumnarParser *parser,
                    PBCREP_Error         **error);
  void (*destroy)  (PBCREP_RawColumnarParser *parser);

  PBCREP_RawColumnarParserHandler handler;
  void *handler_data;
};
