/*
 * A REPRESENTATION is kind-of the heart of this library.
 *
 * It amounts to being a factory for printer-parser pairs
 * that is can handle any ProtobufCMessageDescriptor.
 *
 * The Representation is independent of
 * the ProtobufCMessageDescriptor;
 * but it may limit the types of message-descriptors
 * that can use the representation.
 *
 * The Representation may itself be used as a factory
 * of Printers and Parsers, or you may explicitly fixate
 * on a particular MessageDescriptor and create a (slightly)
 * optimized constructor for that type of messsage.
 * (Along with pre-checking for errors,
 * which is nicer than waiting for
 * the first message to come in)
 */

typedef struct PBCREP_ParserFactory PBCREP_ParserFactory;
typedef struct PBCREP_PrinterFactory PBCREP_PrinterFactory;
typedef struct PBCREP_Representation PBCREP_Representation;

struct PBCREP_ParserFactory
{
  const ProtobufCMessageDescriptor *descriptor;
  PBCREP_Parser *(*make_parser)(PBCREP_ParserFactory *factory,
                                PBCREP_ParserTarget target);
  void (*destroy)(PBCREP_ParserFactory *factory);
};

struct PBCREP_PrinterFactory
{
  const ProtobufCMessageDescriptor *descriptor;
  PBCREP_Printer *(*make_printer)(PBCREP_PrinterFactory *factory,
                                PBCREP_PrinterTarget target);
  void (*destroy)(PBCREP_PrinterFactory *factory);
};

struct PBCREP_Representation
{
  PBCREP_ParserFactory *
      (*create_parser_factory) (PBCREP_Representation *rep,
                                const ProtobufCMessageDescriptor *desc);
  PBCREP_PrinterFactory *
      (*create_printer_factory)(PBCREP_Representation *rep,
                                const ProtobufCMessageDescriptor *desc);
  PBCREP_Parser *
      (*create_parser)         (PBCREP_Representation *rep,
                                const ProtobufCMessageDescriptor *desc);
  PBCREP_Printer *
      (*create_printer)        (PBCREP_Representation *rep,
                                const ProtobufCMessageDescriptor *desc);

  // NULL for static representations
  void (*ref)                  (PBCREP_Representation *rep);
  void (*unref)                (PBCREP_Representation *rep);
};

PBCREP_Representation *
pbcrep_representation_from_string (const char *repstr,
                                   PBCREP_Error *error);

PBCREP_Representation *
pbcrep_representation_ref (PBCREP_Representation *rep);
void
pbcrep_representation_unref (PBCREP_Representation *rep);
