
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

  // may be NULL
  const char *error_message_extra;
} PBCREP_Error;

