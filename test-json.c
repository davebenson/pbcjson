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
  const char *source_filename;
  unsigned source_lineno;

  const char *json;
  const char *expected_callbacks_encoded;
} Test;

#define TEST(json, expected_callbacks_encoded) \
  { __FILE__, __LINE__, json, expected_callbacks_encoded }


typedef  struct TestInfo {
  Test *test;
  const char *expected_callbacks_at;
  bool failed;
} TestInfo;

#define TI_ASSERT(test_info, assertion)     \
  do{                                       \
    if (!(assertion)) {                     \
      fprintf(stderr, "TI_ASSERT:\n"        \
                      "  Assertion: %s\n"   \
                      "  Test File: %s\n"   \
                      "  Test Line: %u\n"   \
                      "  Assert File: %s\n" \
                      "  Assert Line: %u\n",\
              #assertion,                   \
              (test_info)->test->source_filename, \
              (test_info)->test->source_lineno,   \
              __FILE__, __LINE__);          \
      abort();                              \
    }                                       \
  }while(0)
static bool
test_json_cb__start_object  (void *callback_data)
{
  D(fprintf(stderr, "test_json_cb__start_object\n"));
  TestInfo *t = callback_data;
  TI_ASSERT(t, *t->expected_callbacks_at == '{');
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
  TI_ASSERT(t, *t->expected_callbacks_at == '}');
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
  TI_ASSERT(t, *t->expected_callbacks_at == '[');
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
  TI_ASSERT(t, *t->expected_callbacks_at == ']');
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
  TI_ASSERT(t, *t->expected_callbacks_at == 'k');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  TI_ASSERT(t, t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  TI_ASSERT(t, n == key_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      TI_ASSERT(t, memcmp (key, t->expected_callbacks_at, key_length) == 0);
      t->expected_callbacks_at += key_length;
    }
  TI_ASSERT(t, *t->expected_callbacks_at == ' ');
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
  TI_ASSERT(t, *t->expected_callbacks_at == 'n');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  TI_ASSERT(t, t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  D(fprintf(stderr, "expected num length=%u actual=%u\n",(unsigned)n, (unsigned)number_length));
  TI_ASSERT(t, n == number_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      TI_ASSERT(t, memcmp (number, t->expected_callbacks_at, number_length) == 0);
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
  TI_ASSERT(t, *t->expected_callbacks_at == 's');
  t->expected_callbacks_at++;
  char *end;
  unsigned long n = strtoul (t->expected_callbacks_at, &end, 0);
  TI_ASSERT(t, t->expected_callbacks_at < end);
  t->expected_callbacks_at = end;
  TI_ASSERT(t, n == string_length);
  if (*t->expected_callbacks_at == '=')
    {
      t->expected_callbacks_at += 1;
      TI_ASSERT(t, memcmp (string, t->expected_callbacks_at, string_length) == 0);
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
  TI_ASSERT(t, *t->expected_callbacks_at == boolean_value ? 'T' : 'F');
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
  TI_ASSERT(t, *t->expected_callbacks_at == 'N');
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
  if (t->expected_callbacks_at[0] != 'E')
    {
      fprintf(stderr, "Unexpected error (%s): %s", error->code_str, error->message);
      if (error->message2)
        fprintf(stderr, ": %s", error->message2);
      fprintf(stderr, "\n");
    }
  TI_ASSERT(t, t->expected_callbacks_at[0] == 'E');
  TI_ASSERT(t, t->expected_callbacks_at[1] == '{');
  t->expected_callbacks_at += 2;
  while (*t->expected_callbacks_at != '}')
    {
      switch (*t->expected_callbacks_at)
        {
          case '}':
            break;
          case 'v':
            TI_ASSERT(t, t->expected_callbacks_at[1] == '=');
            t->expected_callbacks_at += 2;
            const char *start = t->expected_callbacks_at;
            const char *at = start;
            while (('A' <= *at && *at <= 'Z') || *at == '_'
             ||    ('0' <= *at && *at <= '9'))
              at++;
            const char *errstr = error->code_str;
            TI_ASSERT(t, strlen (errstr) == (size_t)(at - start));
            TI_ASSERT(t, memcmp (errstr, start, at - start) == 0);
            while (*at == ' ')
              at++;
            t->expected_callbacks_at = at;
            break;
          default:
            TI_ASSERT(t, 0);  //TODO
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
            TI_ASSERT(&info, 0);
          break;
        }
      json_rem -= amt;
      json_at += amt;
    }
  TI_ASSERT(&info, info.expected_callbacks_at[0] == 0);
}


static Test json__tests[] = {
  TEST(
    "{\"a\": 42, \"b\": \"c\", \"d\":[1,3], \"eee\":true, \"f\":false, \"g\":null}",
    "{k1=a n2=42 k1=b s1=c k1=d [n1=1 n1=3] k3=eee T k1=f F k1=g N}"
  ),
  TEST(
    "[\"\"]",
    "[s0=]"
  ),
  TEST(
    "[\"\u00b3\"]", /* U+000B3: SUPERSCRIPT THREE (³) */
    "[s2=" DSK_HTML_ENTITY_UTF8_sup3 "]"
  ),
  TEST(
    "[\"" DSK_HTML_ENTITY_UTF8_sup3 "\"]", /* U+000B3: SUPERSCRIPT THREE (³) */
    "[s2=" DSK_HTML_ENTITY_UTF8_sup3 "]"
  ),
  TEST(
    "[\"\u2a90\"]", /* U+2A90 */
    "[s3=" DSK_HTML_ENTITY_UTF8_gsiml "]"
  ),
  TEST(
    "[\"" DSK_HTML_ENTITY_UTF8_gsiml "\"]",
    "[s3=" DSK_HTML_ENTITY_UTF8_gsiml "]"
  ),
  TEST(
    "[\"\\ud83e\\udd10\"]", /* U+1F910: ZIPPER-MOUTH FACE (🤐) */
    "[s4=\xf0\x9f\xa4\x90 ]"
  ),
  TEST(
    "{\"x123\": \"" FIVETHOUCHARS "\"}",
    "{k4=x123 s5000=" FIVETHOUCHARS "}"
  ),
  TEST(
    "[1,2,55555555555555555555,1.2, 555.555, 1e9]",
    "[n1=1 n1=2 n20=55555555555555555555 n3=1.2 n7=555.555 n3=1e9]"
  ),
  TEST(
    "[\"\\n\", \"\"]",
    "[s1=\n s0=]"
  ),
  TEST(
    "[true, false, null, 123]",
    "[T F N n3=123]"
  ),
  TEST(
    "123",
    "E{v=EXPECTED_STRUCTURED_VALUE}"
  ),
  TEST(
    "[1.2.3]",
    "[E{v=BAD_NUMBER}"
  ),
  TEST(
    "{asdf:1}",
    "{E{v=BAD_BAREWORD}"
  ),
  TEST(
    "[\"\n\"]",
    "[E{v=STRING_CONTROL_CHARACTER}"
  ),
  TEST(
    "[/*comment*/]",
    "[E{v=UNEXPECTED_CHAR}"
  ),
  TEST(
    "[\"\xc0\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[\"\xc8\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  ),
  TEST(
    "[\"\xe8\x80\x80\"]",
    "[s3=\xe8\x80\x80]"
  ),
  TEST(
    "[\"\xe8\xc0\x80\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"  //l993
  ),
  TEST(
    "[\"\xe0\x80\xbf\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[\"\xe8\x80\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  ),
  TEST(
    "[\"\xf0\xbf\x82\x84\"]",
    "[s4=\xf0\xbf\x82\x84]"
  ),
  TEST(
    "[\"\xf0\x80\x80\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[\"\\t\"]",
    "[s1=\t]"
  ),
  TEST(
    "[\"\\r\"]",
    "[s1=\r]"
  ),
  TEST(
    "[truth]",                  //l1324
    "[E{v=BAD_BAREWORD}"
  ),
  TEST(
    "[0,1,2,3,4,5,6,7,  8,\t9\t,10,-1,+1,+0,42,1.1]",
    "[n1=0 n1=1 n1=2 n1=3 n1=4 n1=5 n1=6 n1=7 n1=8 n1=9 n2=10 n2=-1 n2=+1 n2=+0 n2=42 n3=1.1]"
  ),
  TEST(
    "[-1e+42, 1.1212e-12]",
    "[n6=-1e+42 n10=1.1212e-12]"
  ),
  TEST(
    "{]",
    "{E{v=UNEXPECTED_CHAR}"
  ),
  TEST(
    "[}",
    "[E{v=UNEXPECTED_CHAR}"
  ),
  TEST(
    "[\"\\0\"]",
    "[s1=0]"                    // Is this really the correct interpretation?
  ),
  TEST(
    "[\"\\\n\"]",               // a formatting line-continuation isn't allowed in standard JSON?
    "[E{v=QUOTED_NEWLINE}"      // l1456
  ),
  TEST(
    "[\"\\\r\n\"]",             // a formatting line-continuation isn't allowed in standard JSON?
    "[E{v=QUOTED_NEWLINE}"      // l1456
  ),
  // TODO: is \x really disallowed in strings?
  TEST(
    "[\"\\ug000\"]",
    "[E{v=EXPECTED_HEX_DIGIT}"
  ),
  TEST(
    "[\"\\u0_00\"]",
    "[E{v=EXPECTED_HEX_DIGIT}"
  ),
  TEST(
    "[\"\\u00x0\"]",
    "[E{v=EXPECTED_HEX_DIGIT}"
  ),
  TEST(
    "[\"\\u000z\"]",
    "[E{v=EXPECTED_HEX_DIGIT}"
  ),
  TEST(
    "[\"\\udd10\"]",
    "[E{v=UTF16_BAD_SURROGATE_PAIR}"
  ),
  TEST(
    "[\"\\ud83e\\uf000\"]",
    "[E{v=UTF16_BAD_SURROGATE_PAIR}"
  ),
  TEST(
    "[\"\\ud83e\\ud83f\"]",
    "[E{v=UTF16_BAD_SURROGATE_PAIR}"           //l1563
  ),
  TEST(
    "[\"\\ud83ex \"]",
    "[E{v=UTF16_BAD_SURROGATE_PAIR}"           //l1563
  ),

  /// TODO: repeat all UTF-8 tests w/ a backslash
  TEST(
    "[\"\\\xc0\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[\"\\\xc8\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  ),
  TEST(
    "[\"\\\xe8\x80\x80\"]",
    "[s3=\xe8\x80\x80]"
  ),
  TEST(
    "[\"\\\xe8\xc0\x80\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"  //l993
  ),
  TEST(
    "[\"\\\xe0\x80\xbf\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[\"\\\xe8\x80\xc0\"]",
    "[E{v=UTF8_BAD_TRAILING_BYTE}"
  ),
  TEST(
    "[\"\\\xf0\xbf\x82\x84\"]",
    "[s4=\xf0\xbf\x82\x84]"
  ),
  TEST(
    "[\"\\\xf0\x80\x80\x80\"]",
    "[E{v=UTF8_OVERLONG}"
  ),
  TEST(
    "[0xff]",
    "[E{v=BAD_NUMBER}"
  ),
  TEST(
    "[-0e+0]",  // somewhat pathological case
    "[n5=-0e+0]"
  ),
  TEST(
    "[0E+0]",  // somewhat pathological case
    "[n4=0E+0]"
  ),
  TEST(
    "[0.000]",
    "[n5=0.000]"
  ),
  TEST(
    "{3:14}",
    "{E{v=UNEXPECTED_CHAR}"                     //l2307
  ),
  TEST(
    "{\"\\u031q\":0}",
    "{E{v=EXPECTED_HEX_DIGIT}"
  ),
  TEST(
    "{\"a\"}",
    "{k1=a E{v=EXPECTED_COLON}"
  ),
  TEST(
    "{\"a\"}",
    "{k1=a E{v=EXPECTED_COLON}"                 //l2366
  ),
  TEST(
    "{\"a\":q}",
    "{k1=a E{v=UNEXPECTED_CHAR}"                 //l2394
  ),
  TEST(
    "{\"a\":0q}",
    "{k1=a E{v=BAD_NUMBER}"                 //l2412
  ),
  TEST(
    "{\"a\":0 \"b\"}",
    "{k1=a n1=0 E{v=EXPECTED_COMMA}"                 //l2431
  ),
  TEST(
    "[1,]",
    "[n1=1 E{v=TRAILING_COMMA}"
  ),
  TEST(
    "{\"a\":1,}",
    "{k1=a n1=1 E{v=TRAILING_COMMA}"
  ),
  TEST(
    "[1.]",
    "[E{v=BAD_NUMBER}"
  ),
  TEST(
    "[.1]",
    "[E{v=BAD_NUMBER}"
  ),
  TEST(
    "[1e+a]",
    "[E{v=BAD_NUMBER}"
  ),
};

struct TestSuite {
  const char *suite_name;

  JSON_CallbackParser_Options *base_options;
  void (*suite_options_setup)(JSON_CallbackParser_Options *inout);

  size_t n_tests;
  Test *tests;
};
#define DEFINE_TEST_SUITE_FROM_TESTS(basename) \
static struct TestSuite basename##__test_suite = \
{ \
  #basename, \
  &basename##__base_options, \
  basename##__suite_options_setup, \
  sizeof(basename##__tests) / sizeof (Test), \
  basename##__tests \
};
static JSON_CallbackParser_Options json__base_options =
  JSON_CALLBACK_PARSER_OPTIONS_INIT;
#define json__suite_options_setup NULL
DEFINE_TEST_SUITE_FROM_TESTS(json);


static Test json5__tests[] = {
  TEST(
    "{a: 0x42, b: .5, /* huh */ 'c': 'hi' }",
    "{k1=a n4=0x42 k1=b n2=.5 k1=c s2=hi}"
  ),
  TEST(
    "{a: 1,}",
    "{k1=a n1=1}"
  ),
  TEST(
    "[1,]",
    "[n1=1]"
  ),
  TEST(
    "['adfa\\\nqwer']",
    "[s8=adfaqwer]"
  ),
  TEST(
    "[10,{a:null, bbbb:100.000}]",
    "[n2=10 {k1=a N k4=bbbb n7=100.000}]"
  ),
  TEST(
    "[[1],[2,3,],[4,5,6,],]\n\n",
    "[[n1=1] [n1=2 n1=3] [n1=4 n1=5 n1=6]]"
  ),
  TEST(
    "[1.]",
    "[n2=1.]"
  ),
  TEST(
    "[.1]",
    "[n2=.1]"
  ),
  TEST(
    "[1e+a]",
    "[E{v=BAD_NUMBER}"
  ),
};
static JSON_CallbackParser_Options json5__base_options =
  JSON_CALLBACK_PARSER_OPTIONS_INIT_JSON5;
#define json5__suite_options_setup NULL
DEFINE_TEST_SUITE_FROM_TESTS(json5);

#define bare_value__base_options json__base_options
static void
bare_value__suite_options_setup (JSON_CallbackParser_Options *opts)
{
  opts->permit_bare_values = 1;
  opts->permit_toplevel_commas = 1;
}
static Test bare_value__tests[] = {
  TEST(
    "1,2,3,\"hi\",null,true,false,0.1,{},[1]",
    "n1=1 n1=2 n1=3 s2=hi N T F n3=0.1 {} [ n1=1 ]"
  ),
  TEST(
    "1 2 3 \"hi\" null true false 0.1 {} [1]",
    "n1=1 n1=2 n1=3 s2=hi N T F n3=0.1 {} [ n1=1 ]"
  )
};
DEFINE_TEST_SUITE_FROM_TESTS(bare_value);

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
  
static void
run_test_suite (const struct TestSuite *suite)
{
  unsigned nx = sizeof(sizes) / sizeof(sizes[0]);
  JSON_CallbackParser_Options options = JSON_CALLBACK_PARSER_OPTIONS_INIT;
  if (suite->base_options != NULL)
    options = *suite->base_options;
  if (suite->suite_options_setup != NULL)
    suite->suite_options_setup (&options);

  fprintf(stderr, "Running test-suite %s:\n", suite->suite_name);
  for (size_t it = 0; it < suite->n_tests; it++)
    for (size_t ix = 0; ix < nx; ix++)
      run_test (suite->tests+it, sizes[ix], &options);
  fprintf(stderr, "Ran %u tests.\n", (unsigned) suite->n_tests);
}

struct TestSuite *suites[] = {
  &json__test_suite,
  &json5__test_suite,
  &bare_value__test_suite
};

int main(void)
{
  for (unsigned suite = 0;
       suite < sizeof(suites)/sizeof(suites[0]);
       suite++)
    run_test_suite (suites[suite]);

  fprintf(stderr, "Tests succeeded!\n");

  return 0;
}

