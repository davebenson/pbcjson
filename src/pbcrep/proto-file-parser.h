
struct PBCREP_ProtoFileParseResults {
  size_t n_message_descriptors;
  ProtobufCMessageDescriptor **message_descriptors;
  ProtobufCMessageDescriptor **message_descriptors_by_name;
  size_t n_enum_descriptors;
  ProtobufCEnumDescriptor **enum_descriptors;
  ProtobufCEnumDescriptor **enum_descriptors_by_name;
  size_t n_service_descriptors;
  ProtobufCServiceDescriptor **service_descriptors;
  ProtobufCServiceDescriptor **service_descriptors_by_name;
};

