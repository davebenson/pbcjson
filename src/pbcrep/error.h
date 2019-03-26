
typedef struct PBCREP_Error {
  const char *error_message;

  // The interpretation of the error_code_str depends on
  // the type of PBC_Parser.
  //
  // For example, for a JSON parser,
  // it may be any of the many JSON errors: BAD_NUMBER, UNEXPECTED_CHAR, etc.
  //
  // And yes, this 'code' is all uppercase and underscores (for JSON at least)
  const char *error_code_str;

  unsigned free_message : 1;
} PBCREP_Error;


PBCREP_Error *pbcrep_error_new         (const char *code,
                                        const char *message);
PBCREP_Error *pbcrep_error_new_printf  (const char *code,
                                        const char *message,
                                        ...) PBCREP_GNUC_PRINTF(2,3);
void          pbcrep_error_destroy     (PBCREP_Error *error);


