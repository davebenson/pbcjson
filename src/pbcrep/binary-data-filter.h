/* A filter of binary-data.  Example implementations provided:
 *    * to- and from- hex
 *          pbcrep_binary_data_filter_to_hex_new ();
 *          pbcrep_binary_data_filter_from_hex_new ();
 *    * to- and from- base64
 *          pbcrep_binary_data_filter_to_base64_new (options);
 *          pbcrep_binary_data_filter_from_base64_new (options);
 *    * compresstion [requires parameters]
 *    * decompresstion [may take some paramters, most come from compressed data]
 */

struct PBCREP_BinaryDataFilter {
  bool (*write)(PBCREP_BinaryDataFilter *filter, 
                size_t                   data,
                PBCREP_Error           **error);
  bool (*end)  (PBCREP_BinaryDataFilter *filter, 
                PBCREP_Error           **error);
  PBCREP_Buffer target;
  void (*destroy) (PBCREP_BinaryDataFilter *filter);
};


PBCREP_BinaryDataReader *pbcrep_binary_data_reader_new_filtered (PBCREP_BinaryDataReader *underlying,
                                                                 PBCREP_BinaryDataFilter *filter);
PBCREP_BinaryDataWriter *pbcrep_binary_data_writer_new_filtered (PBCREP_BinaryDataWriter *underlying,
                                                                 PBCREP_BinaryDataFilter *filter);

void pbcrep_binary_data_filter_destroy (PBCREP_BinaryDataFilter *filter);
