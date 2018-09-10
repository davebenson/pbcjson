
typedef struct PBCREP_Writer PBCREP_Writer;


struct PBCREP_Writer
{
  bool (*write)                    (PBCREP_Writer *writer,
                                    const ProtobufCMessage *message,
                                    PBCREP_Error **error);
  bool (*end_write)                (PBCREP_Writer *writer,
                                    PBCREP_Error **error);
  void (*destroy)                  (PBCREP_Writer *writer);
  const ProtobufCMessageDescriptor *descriptor;
};

PBCREP_Writer    *pbcrep_writer_new_printer (PBCREP_BinaryDataWriter *writer,
                                             PBCREP_Printer *printer);
bool              pbcrep_writer_write       (PBCREP_Writer *reader,
                                             const ProtobufCMessage *message,
                                             PBCREP_Error **error);
bool              pbcrep_writer_end_write   (PBCREP_Writer *writer,
                                             PBCREP_Error **error);
void              pbcrep_writer_destroy     (PBCREP_Writer *reader);

