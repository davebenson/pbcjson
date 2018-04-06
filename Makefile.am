AM_CFLAGS = -I$(top_srcdir)/include $(LPBC_CFLAGS)
test_programs = bin/t/json bin/t/pbcjson
TESTS = $(test_programs)
noinst_PROGRAMS = $(test_programs)


noinst_LIBRARIES = libpbcrep.a
libpbcrep_a_SOURCES = \
src/pbcrep/parser.c \
src/pbcrep/parsers/length-prefixed/pbc-parser-length-prefixed.c \
src/pbcrep/parsers/json/json-cb-parser.c \
src/pbcrep/parsers/json/pbc-parser-json.c

bin_t_json_SOURCES = src/t/test-json.c
bin_t_json_LDADD = libpbcrep.a $(LPBC_LIBS)
bin_t_pbcjson_SOURCES = src/t/test-pbcjson.c generated/test1.pb-c.c
bin_t_pbcjson_LDADD = libpbcrep.a $(LPBC_LIBS)
