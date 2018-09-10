typedef struct PBCREP_Reader PBCREP_Reader;

struct PBCREP_Reader
{
  const ProtobufCMessageDescriptor *descriptor;
  ProtobufCMessage *message;

  PBCREP_ReadResult (*advance) (PBCREP_Reader *reader,
                                PBCREP_Error **error);
  void              (*destroy) (PBCREP_Reader *reader);

};

PBCREP_Reader    *pbcrep_reader_new_parser (PBCREP_BinaryDataReader *reader,
                                            PBCREP_Parser           *parser);
PBCREP_ReadResult pbcrep_reader_advance    (PBCREP_Reader           *reader,
                                            PBCREP_Error           **error);
void              pbcrep_reader_destroy    (PBCREP_Reader           *reader);

