


typedef struct PBC_ParserFactory PBC_ParserFactory;
struct PBC_ParserFactory {
  PBC_Parser *(*create_parser)(PBC_ParserFactory *factory, 
                               ProtobufCMessageDescriptor *desc);
  void        (*destroy)      (PBC_ParserFactory *factory, 
                               ProtobufCMessageDescriptor *desc);
};

