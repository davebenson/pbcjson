
typedef struct PBCREP_BinaryDataWriter PBCREP_BinaryDataWriter;

typedef bool (*PBCREP_BinaryDataCallback) (size_t     max_length,
                                           uint8_t   *data,
                                           void      *callback_data);

struct PBCREP_BinaryDataWriter
{
  bool (*write)            (PBCREP_BinaryDataWriter *writer,
                            size_t                   max_length,
                            uint8_t                 *data,
                            PBCREP_Error           **error);
  void (*destroy)          (PBCREP_BinaryDataWriter *reader);
};

PBCREP_BinaryDataWriter *pbcrep_binary_data_writer_to_fileno (int fd, bool do_close);
PBCREP_BinaryDataWriter *pbcrep_binary_data_writer_to_file (const char *filename, PBCREP_Error **error);
PBCREP_BinaryDataWriter *pbcrep_binary_data_writer_to_data_max (size_t len, const uint8_t *data);
PBCREP_BinaryDataWriter *pbcrep_binary_data_writer_to_buffer_cb (PBCREP_BinaryDataCallback callback, void *callback_data);

