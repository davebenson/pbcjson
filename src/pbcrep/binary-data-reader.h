
typedef struct PBCREP_BinaryDataReader PBCREP_BinaryDataReader;
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

