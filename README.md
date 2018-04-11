= PBCREP: Overview =

This library handles parsing and printing of
ProtobufC messages as well as "framing" them, meaning
a way to separate the messages from each other, outside the protobuf spec,
which doesn't include framing recommendations.

It provides the following types of objects:
  * `PBCREP_Parser`: takes binary-data and yields a stream of ProtobufCMessages of a certain type.
  * `PBCREP_Printer`: takes a stream of ProtobufCMessages of a certain type, and renders binary-data.

Subclasses of `PBCREP_Parser`:
  * `PBCREP_Parser_JSON`
    * `repstr`: "`json`"
  * `PBCREP_Parser_LengthPrefixed_Protobuf`
    * `repstr`: "`length_prefixed_u8`" (etc)

Subclasses of `PBCREP_Printer`:
  * `PBCREP_Printer_JSON`
  * `PBCREP_Printer_LengthPrefixed_Protobuf`

= Details about various representations =

== PBCREP: JSON Parsing ==

== PBCREP: JSON Printing ==

== PBCREP: Length-Prefixed Parsing ==

== PBCREP: Length-Prefixed Printing ==

