typedef struct PBCREP_BinaryDataReader PBCREP_BinaryDataReader;
typedef struct PBCREP_Reader PBCREP_Reader;

typedef enum
{
  PBCREP_READ_RESULT_OK,
  PBCREP_READ_RESULT_EOF,
  PBCREP_READ_RESULT_BLOCKED,
  PBCREP_READ_RESULT_ERROR
} PBCREP_ReadResult;

struct PBCREP_BinaryDataReader
{
  PBCREP_ReadResult (*read)(PBCREP_BinaryDataReader *reader,
                            size_t                   max_length,
                            uint8_t                 *data,
                            size_t                  *amt_read,
                            PBCREP_Error           **error);
  void (*destroy)(PBCREP_BinaryDataReader *reader);
};

PBCREP_BinaryDataReader *pbcrep_binary_data_reader_from_fileno (int fd, bool do_close);
PBCREP_BinaryDataReader *pbcrep_binary_data_reader_from_file (const char *filename, PBCREP_Error **error);
PBCREP_BinaryDataReader *pbcrep_binary_data_reader_from_data (size_t len, const uint8_t *data);
// data will be free()d
PBCREP_BinaryDataReader *pbcrep_binary_data_reader_take_data (size_t len, uint8_t *data);


struct PBCREP_Reader
{
  PBCREP_ReadResult (*next) (PBCREP_Reader *reader);
  void (*destroy) (PBCREP_Reader *reader);
  const ProtobufCMessageDescriptor *descriptor;
  ProtobufCMessage *message;
  PBCREP_Error *error;
};

PBCREP_Reader    *pbcrep_reader_new_parser (PBCREP_BinaryDataReader *reader,
                                            PBCREP_Parser *parser);
PBCREP_ReadResult pbcrep_reader_read    (PBCREP_Reader *reader);
void              pbcrep_reader_destroy (PBCREP_Reader *reader);

