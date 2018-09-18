
/* --- Tokenization --- */
typedef enum {
  TOKEN_BAREWORD,
  TOKEN_EQUALS,
  TOKEN_NUMBER,
  TOKEN_QUOTED_STRING,
  TOKEN_SEMICOLON,
  TOKEN_INTEGER,                // includes sign
  TOKEN_FLOAT,                  // includes sign

  // Initially, tokens are all in a big flat array.
  // After calling tokens_treeify(),
  // they will instead be in a tree-structure.
  // TOKEN_FLAT_* pairs will be converted to
  // TOKEN_TREE_BRACED or TOKEN_TREE_BRACKETED,
  // with errors on mismatch and no stray FLAT tokens allowed.
  TOKEN_FLAT_LBRACE,
  TOKEN_FLAT_RBRACE,
  TOKEN_FLAT_LBRACKET,
  TOKEN_FLAT_RBRACKET,

  TOKEN_TREE_BRACKETED,
  TOKEN_TREE_BRACED,
} TokenType;

typedef struct {
  TokenType type;
  size_t str_len;
  const char *str;      // points into 'text'.
  unsigned n_subtokens;
  Token *subtokens;
} Token;

static void
clear_token (Token *to_clear)
{
  if (to_clear->type == TOKEN_TREE_BRACED
   || to_clear->type == TOKEN_TREE_BRACKETED)
    {
      size_t i, N = to_clear->n_subtokens;
      for (i = 0; i < N; i++)
        clear_token (to_clear->subtokens + i);
      pbcrep_free (to_clear->subtokens);
      to_clear->n_subtokens = 0;
      to_clear->subtokens = NULL;
    }
}

/* filename: only used for error messages */
static Token *scan_tokens (const char *filename,
                           size_t text_len,
                           const char *text,
                           size_t *n_tokens_out,
                           PBCREP_Error **error)
{
  const char *end = text + text_len;
  unsigned line_no = 1;
  const char *at = text;
  unsigned n_tokens = 16;
  Token *tokens = pbcrep_malloc (sizeof (Token) * n_tokens);
  while (at < end)
    {
      if (*at == ' ')
        {
          column++;
          at++;
          continue;
        }
      if (*at == '\r')
        {
          at++;
          continue;
        }
      if (*at == '\n')
        {
          column = 1;
          line_no++;
          at++;
          continue;
        }
      if (*at == '_'
       || ('a' <= *at && *at <= 'z')
       || ('A' <= *at && *at <= 'Z'))
        {
          ...
        }
      else if (*at == '-'
            || ('0' <= *at && *at <= '9')
            || *at == '.')
        {
          bool is_int;
          size_t num_len = scan_number (at, end, &is_int, &number_len);
          if (num_len == 0)
            {
              *error = pbcrep_error_new ("error scanning number (%s:%u)",
                                         filename, line_no);
              goto error_cleanup;
            }
          ...
        }
      else if (*text == '{')
        APPEND_ONECHAR_TOKEN (TOKEN_FLAT_LBRACE);
      else if (*text == '}')
        APPEND_ONECHAR_TOKEN (TOKEN_FLAT_RBRACE);
      else if (*text == '[')
        APPEND_ONECHAR_TOKEN (TOKEN_FLAT_LBRACKET);
      else if (*text == ']')
        APPEND_ONECHAR_TOKEN (TOKEN_FLAT_RBRACKET);
      else if (*text == ';')
        APPEND_ONECHAR_TOKEN (TOKEN_SEMICOLON);
      else if (*text == '=')
        APPEND_ONECHAR_TOKEN (TOKEN_EQUALS);
      else if (*text == '"')
        {
          ... dq string
        }
      else 
        {
          *error = ...;
          ... unexpected 
          goto error_cleanup;
        }
    }
  return tokens;

error_cleanup:
  for (unsigned i = 0; i < n_tokens; i++)
    clear_token (tokens + i);
  pbcrep_free (tokens);
  return NULL;
}

/* --- Token Tree-ification --- */
static bool treeify_tokens (size_t *n_tokens_inout,
                            Token **tokens_inout,
                            PBCREP_Error **error)
{
  ...
}

/* --- Parse messages, enums and services --- */
...
