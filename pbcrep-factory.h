
struct PBCREP_Factory {
  PBCREP_Parser *(*create_parser) (PBCREP_Factory *factory,
                                   const ProtobufCMessageDescriptor *desc);
  PBCREP_Printer*(*create_printer)(PBCREP_Factory *factory,
                                   const ProtobufCMessageDescriptor *desc);
};
