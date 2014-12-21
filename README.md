crcspeed
========

CRC be slow.

This make CRC be fast.

No original ideas, but original adaptations.  Lots of shoulder standing.

This started out as a modified version of comment at http://stackoverflow.com/questions/20562546
then got made more extensible.

Features
--------

  - CRC processing in 8-byte steps for CRC64 and CRC16.
  - Little endian and big endian support
    - big endian support hasn't been tested yet.
  - Test suite compares regular byte-by-byte CRC calcuation
  with 8-byte step calculation and reports resulting CRCs with speed comparison.

Usage
-----

  - Use included parameters (see included `crc64.c` and `crc16.c`) for little endian CRCs:
    - `crc64speed_init();`
    - `crc64speed(old_crc, new_data, new_data_length);`
    - `crc16speed_init();`
    - `crc16speed(old_crc, new_data, new_data_length);`
  - Use included parameters for native architecture CRCs:
    - `crc64speed_init_native();`
    - `crc64speed_native(old_crc, new_data, new_data_length);`
    - `crc16speed_init_native();`
    - `crc16speed_native(old_crc, new_data, new_data_length);`
  - Use custom CRC64 parameters:
    - `crcspeed64native_init(crc_calculation_function, uint64_t populate[8][256]);
      - crc calculation function takes (old_crc, data, data_len) and returns crc64 as uint64_t.
      - `populate` is a lookup table _init populates for future lookups.
    - `crcspeed64native(populated_lookup_table, old_crc, new_data, new_data_length);`
  - Use custom CRC16 parameters:
    - `crcspeed16native_init(crc_calculation_function, uint16_t populate[8][256]);`
      - crc calculation function takes (old_crc, data, data_len) and returns crc16 as uint16_t.
    - `crcspeed16native(populated_lookup_table, old_crc, new_data, new_data_length);`

Additionally, there are specific functions for forcing little or big endian calculations:
`crcspeed64little_int()`, `crcspeed64little()`, `crc64big_init()`, `crcspeed64big()`,
`crcspeed16little_int()`, `crcspeed16little()`, `crc16big_init()`, `crcspeed16big()`.

Architecture
------------

  - `crcspeed.c` is a _framework_ for bootstrapping a lookup table using an existing CRC function then using
  the lookup table to calculate fast CRCs.
  - `crc64speed.c` is a ready-to-use fast CRC64 implementation bootstrapped by the lookup table in `crc64.c`.
  - `crc16speed.c` is a ready-to-use fast CRC16 implementation bootstrapped by the lookup table in `crc16.c`.
  - when in a multithreaded environment, make sure you only run the initialization function(s) once

Benchmark
---------
```haskell
% make
    CC crcspeed-test
% ./crcspeed-test ~/Downloads/John\ Mayer\ -\ Live\ At\ Austin\ City\ Limits\ PBS\ -\ Full\ Concert-gcdUz12FkdQ.mp4 
Comparing CRCs against 730.72 MB file...

crc64
CRC = ee43263b0a2b6c60
1.868655 seconds at 391.04 MB/s (6.33 CPU cycles per byte)

crc64speed
CRC = ee43263b0a2b6c60
0.493085 seconds at 1481.93 MB/s (1.67 CPU cycles per byte)

crc16
CRC = 000000000000490f
2.060112 seconds at 354.70 MB/s (6.97 CPU cycles per byte)

crc16speed
CRC = 000000000000490f
0.463500 seconds at 1576.52 MB/s (1.57 CPU cycles per byte)
```
