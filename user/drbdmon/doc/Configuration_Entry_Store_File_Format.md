# DRBDmon - Configuration entry store file format

## Endianness
All numeric fields in a configuration entry store file are stored in big endian format. Note that this excludes the values of entries with an ENTRY_TYPE of Binary (9). The format of the data stored as the value of such an entry is application-defined.


## Version compatibility
Configuration entry store files with the same major version are backwards compatible.


## Configuration file header

### CES_UUID
CES file identifier – UUID Variant 1 Version 4
16 bytes

The UUID value that identifies a configuration entry store file is:
`6A454A5D-347A-498D-8E0A-6AD5F5014133`

This value is stored in binary format as a 128 bit unsigned integer.

### CES_VERSION
CES version
2 bytes – major version, 16 bit unsigned integer
2 bytes – minor version, 16 bit unsigned integer


## Configuration file entries

Following the header, the configuration entry store file contains a list of configuration entries. The list may have an arbitrary number of entries, including the possibility of containing no entries.


### Configuration entry format

A configuration entry consists of the following 4 fields:

**ENTRY_TYPE**
Data type of the configuration entry
Field size: 4 bytes

**KEY_LENGTH**
Length of the configuration entry key in bytes
Field size: 1 byte

**ENTRY_KEY**
Identifier of the configuration entry
Field size: `KEY_LENGTH`
The maximum length of `ENTRY_KEY` is 40 bytes.

**ENTRY_VALUE**
Value of the configuration entry.
Field size depends on the entry type.

### Configuration entry types

#### Boolean
Boolean value
ENTRY_TYPE value: `0`

Format:
`00 00 00 00 KEY_LENGTH ENTRY_KEY XX`

Valid values are:
`0`	false
`1`	true


#### UnsgInt8
8 bit wide unsigned integer
`ENTRY_TYPE` value: `1`

Format:
`00 00 00 01 KEY_LENGTH ENTRY_KEY XX`

Range:
[0, 255]


#### UnsgInt16
16 bit wide unsigned integer
`ENTRY_TYPE` value: `2`

Format:
`00 00 00 02 KEY_LENGTH ENTRY_KEY XX XX`

Range:
[0, 65535]


#### UnsgInt32
32 bit wide unsigned integer
`ENTRY_TYPE` value: `3`

Format:
`00 00 00 03 KEY_LENGTH ENTRY_KEY XX XX XX XX`

Range:
[0, 4294967295]


#### UnsgInt64
64 bit wide unsigned integer
`ENTRY_TYPE` value: `4`

Format:
`00 00 00 04 KEY_LENGTH ENTRY_KEY XX XX XX XX XX XX XX XX`

Range:
[0, 264 - 1]


#### Int8
8 bit wide signed integer
`ENTRY_TYPE` value: `5`

Format:
`00 00 00 05 KEY_LENGTH ENTRY_KEY XX`

Range:
[-128, 127]


#### Int16
16 bit wide signed integer
`ENTRY_TYPE` value: `6`

Format:
`00 00 00 06 KEY_LENGTH ENTRY_KEY XX XX`

Range:
[-32768, 32767]


#### Int32
32 bit wide signed integer
`ENTRY_TYPE` value: `7`

Format:
`00 00 00 07 KEY_LENGTH ENTRY_KEY XX XX XX XX`

Range:
[-2147483648, 2147483647]


#### Int64
64 bit wide signed integer
`ENTRY_TYPE` value: `8`

Format:
`00 00 00 08 KEY_LENGTH ENTRY_KEY XX XX XX XX XX XX XX XX`

Range:
[-(263), 263 - 1]


#### Binary
Binary data
`ENTRY_TYPE` value: `9`

Format:
`00 00 00 09 KEY_LENGTH ENTRY_KEY DATA_LENGTH DATA`

Format of `DATA_LENGTH`:
32 bit unsigned integer (4 bytes)

Format of `DATA`:
Byte sequence of `DATA_LENGTH` bytes

Range:
[-(263), 263 - 1]
