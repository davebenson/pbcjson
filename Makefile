all: test-json pbc-parser-json.o pbc-parser-length-prefixed.o test-pbcjson

CFLAGS := `pkg-config --cflags libprotobuf-c` -W -Wall -g

pbc-parser-json.o: pbc-parser-json.c pbc-parser.h pbc-parser-json.h json-cb-parser.h
	cc -c $(CFLAGS) -o pbc-parser-json.o pbc-parser-json.c

pbc-parser-length-prefixed.o: pbc-parser-length-prefixed.c pbc-parser.h pbc-parser-length-prefixed.h
	cc -c $(CFLAGS) -o pbc-parser-length-prefixed.o pbc-parser-length-prefixed.c

test-json: json-cb-parser.c json-cb-parser.h test-json.c
	cc -W -Wall -g -o test-json json-cb-parser.c test-json.c

test-pbcjson: json-cb-parser.c json-cb-parser.h pbc-parser-json.c pbc-parser-json.h test-pbcjson.c generated/test1.pb-c.c generated/test1.pb-c.h
	cc $(CFLAGS) -o test-pbcjson json-cb-parser.c pbc-parser-json.c test-pbcjson.c

generated/test1.pb-c.c generated/test1.pb-c.h: test1.proto
	@mkdir -p generated
	protoc_c --c_out=generated

test-json-p: json-cb-parser.c json-cb-parser.h test-json.c
	gcc-7 -W -Wall -pg -g -o test-json-p json-cb-parser.c test-json.c

clean:
	rm -f test-json test-json-p *.o
