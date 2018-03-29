#include "json-cb-parser.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define D(code)  

#define DSK_HTML_ENTITY_UTF8_sup3 "\302\263"
#define DSK_HTML_ENTITY_UTF8_gsiml "\342\252\220"

#define FIFTYCHARS  "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx"
#define TWOHUND  FIFTYCHARS FIFTYCHARS FIFTYCHARS FIFTYCHARS
#define THOUCHARS  TWOHUND TWOHUND TWOHUND TWOHUND TWOHUND
#define FIVETHOUCHARS  THOUCHARS THOUCHARS THOUCHARS THOUCHARS THOUCHARS
 
typedef  struct Test {
  const char *json;
  const char *expected_callbacks_encoded;
} Test;

typedef  struct TestInfo {
  Test *test;
  const char *expected_callbacks_at;
  bool failed;
} TestInfo;

static bool
test_json_cb__start_object  (void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__start_object\n"));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '{');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__end_object    (void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__end_object\n"));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '}');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__start_array   (void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__start_array\n"));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == '[');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__end_array     (void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__end_array\n"));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == ']');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__object_key    (unsigned key_length,
                             const char *key,
                             void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__object_key(%s)\n", key));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 'k');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  assert(n == key_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      assert(memcmp (key, t->expected_callbacks_at, key_length) == 0);
      t->expected_callbacks_at += key_length;
    }
  assert (*t->expected_callbacks_at == ' ');
  t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__number_value  (unsigned number_length,
                             const char *number,
                             void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__number_value(%s)\n", number));
  TestInfo *t = callback_data;
  assert(*t->expected_callbacks_at == 'n');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  D(fprintf(stderr, "expected num length=%u actual=%u\n",(unsigned)n, (unsigned)number_length));
  assert(n == number_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      assert(memcmp (number, t->expected_callbacks_at, number_length) == 0);
      t->expected_callbacks_at += number_length;
    }
  if (*t->expected_callbacks_at == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

#define test_json_cb__partial_string_value NULL

static bool
test_json_cb__string_value  (unsigned string_length,
                             const char *string,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  D(fprintf(stderr, "test_json_cb__string_value(%u, %s)\n", string_length, string));
  assert(*t->expected_callbacks_at == 's');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  assert(t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  assert(n == string_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      assert (memcmp (string, t->expected_callbacks_at, string_length) == 0);
      t->expected_callbacks_at += string_length;
    }
  if (*t->expected_callbacks_at == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__boolean_value (int boolean_value,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  D(fprintf(stderr, "test_json_cb__boolean_value(%u)\n", boolean_value));
  assert(*t->expected_callbacks_at == boolean_value ? 'T' : 'F');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}
static bool
test_json_cb__null_value    (void *callback_data)
{
  TestInfo *t = callback_data;
  D(fprintf(stderr, "test_json_cb__null_value\n"));
  assert(*t->expected_callbacks_at == 'N');
  t->expected_callbacks_at++;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  return true;
}

static bool
test_json_cb__error         (const JSON_CallbackParser_ErrorInfo *error,
                             void *callback_data)
{
  TestInfo *t = callback_data;
  fprintf(stderr, "code=%s, message=%s\n",error->code_str, error->message);
  assert(t->expected_callbacks_at[0] == 'E');
  assert(t->expected_callbacks_at[1] == '{');
  t->expected_callbacks_at += 2;
  while (*t->expected_callbacks_at != '}')
    {
      switch (*t->expected_callbacks_at)
        {
          case '}':
            break;
          case 'v':
            assert(t->expected_callbacks_at[1] == '=');
            t->expected_callbacks_at += 2;
            const char *start = t->expected_callbacks_at;
            const char *at = start;
            while (('A' <= *at && *at <= 'Z') || *at == '_'
             ||    ('0' <= *at && *at <= '9'))
              at++;
            const char *errstr = error->code_str;
            assert (strlen (errstr) == (size_t)(at - start));
            assert (memcmp (errstr, start, at - start) == 0);
            while (*at == ' ')
              at++;
            t->expected_callbacks_at = at;
            break;
          default:
            assert(0);  //TODO
        }
    }
  t->expected_callbacks_at += 1;
  if (t->expected_callbacks_at[0] == ' ')
    t->expected_callbacks_at += 1;
  t->failed = true;
  return false;
}

static void
test_json_cb__destroy       (void *callback_data)
{
  (void) callback_data; /// TODO assert that no other callbacks occur
}

JSON_Callbacks callbacks = JSON_CALLBACKS_DEF(test_json_cb__, );
static void
run_test (Test *test,
          unsigned max_write,
         JSON_CallbackParser_Options *options)
{
  TestInfo info = { test, test->expected_callbacks_encoded, false };
  JSON_CallbackParser *parser = json_callback_parser_new (&callbacks, &info, options);
  const char *json_at = test->json;
  unsigned json_rem = strlen (test->json);
  while (json_rem > 0)
    {
      unsigned amt = max_write < json_rem ? max_write : json_rem;
      D(fprintf(stderr, "feeding in: '%.*s'\n", amt, json_at));
      if (!json_callback_parser_feed(parser, amt, (const uint8_t *) json_at))
        {
          if (!info.failed)
            assert(0);
          break;
        }
      json_rem -= amt;
      json_at += amt;
    }
  assert(info.expected_callbacks_at[0] == 0);
}


static Test tests[] = {
  {
    "{\"a\": 42, \"b\": \"c\", \"d\":[1,3], \"eee\":true, \"f\":false, \"g\":null}",
    "{k1=a n2=42 k1=b s1=c k1=d [n1=1 n1=3] k3=eee T k1=f F k1=g N}"
  },
  {
    "[\"\"]",
    "[s0=]"
  },
  {
    "[\"\u00b3\"]", /* U+000B3: SUPERSCRIPT THREE (³) */
    "[s2=" DSK_HTML_ENTITY_UTF8_sup3 "]"
  },
  {
    "[\"" DSK_HTML_ENTITY_UTF8_sup3 "\"]", /* U+000B3: SUPERSCRIPT THREE (³) */
    "[s2=" DSK_HTML_ENTITY_UTF8_sup3 "]"
  },
  {
    "[\"\u2a90\"]", /* U+2A90 */
    "[s3=" DSK_HTML_ENTITY_UTF8_gsiml "]"
  },
  {
    "[\"" DSK_HTML_ENTITY_UTF8_gsiml "\"]",
    "[s3=" DSK_HTML_ENTITY_UTF8_gsiml "]"
  },
  {
    "[\"\\ud83e\\udd10\"]", /* U+1F910: ZIPPER-MOUTH FACE (🤐) */
    "[s4=\xf0\x9f\xa4\x90 ]"
  },
  {
    "{\"x123\": \"" FIVETHOUCHARS "\"}",
    "{k4=x123 s5000=" FIVETHOUCHARS "}"
  },
  {
    "[1,2,55555555555555555555,1.2, 555.555, 1e9]",
    "[n1=1 n1=2 n20=55555555555555555555 n3=1.2 n7=555.555 n3=1e9]"
  },
  {
    "[\"\\n\", \"\"]",
    "[s1=\n s0=]"
  },
  {
    "[true, false, null, 123]",
    "[T F N n3=123]"
  },
  {
    "123",
    "E{v=EXPECTED_STRUCTURED_VALUE}"
  },
  {
    "[1.2.3]",
    "[E{v=BAD_NUMBER}"
  },
  {
    "{asdf:1}",
    "{E{v=BAD_BAREWORD}"
  },
  {
    "[\"\n\"]",
    "[E{v=STRING_CONTROL_CHARACTER}"
  },
  {
    "[/*comment*/]",
    "[E{v=UNEXPECTED_CHAR}"
  },
  {
    "[\"\xc0\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  },
  {
    "[\"\xc8\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  },
  {
    "[\"\xe8\x80\x80\"]",
    "[s3=\xe8\x80\x80]",
  },
  {
    "[\"\xe8\xc0\x80\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"  //l993
  },
  {
    "[\"\xe0\x80\xbf\"]",
    "[E{v=UTF8_OVERLONG}"
  },
  {
    "[\"\xe8\x80\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  },
  {
    "[\"\xf0\xbf\x82\x84\"]",
    "[s4=\xf0\xbf\x82\x84]",
  },
  {
    "[\"\xf0\x80\x80\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  },
  {
    "[\"\\t\"]",
    "[s1=\t]",
  },
  {
    "[\"\\r\"]",
    "[s1=\r]",
  },
  {
    "[0,1,2,3,4,5,6,7,  8,\t9\t,10,-1,+1,+0,42,1.1]",
    "[n1=0 n1=1 n1=2 n1=3 n1=4 n1=5 n1=6 n1=7 n1=8 n1=9 n2=10 n2=-1 n2=+1 n2=+0 n2=42 n3=1.1]"
  },
  {
    "[-1e+42, 1.1212e-12]",
    "[n6=-1e+42 n10=1.1212e-12]"
  }
};

static size_t sizes[] = {
  1,
  2,
  3,
  4,
  5,
  6,
  8,
  10,
  12,
  13,
  17,
  20,
  32,
  64,
  65,
  1000
};
  

int main(void)
{
  JSON_CallbackParser_Options options = JSON_CALLBACK_PARSER_OPTIONS_INIT;
  unsigned nt = sizeof(tests) / sizeof(tests[0]);
  unsigned nx = sizeof(sizes) / sizeof(sizes[0]);

  for (size_t it = 0; it < nt; it++)
    for (size_t ix = 0; ix < nx; ix++)
      run_test (tests+it, sizes[ix], &options);

  fprintf(stderr, "Tests succeeded!\n");

  return 0;
}

