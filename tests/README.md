# Tests

What the various directories are for in this directory:

- `shlib/`: shell libraries for testing.  In general, I'm not a giant
  fan of writing anything complicated in shell scripts, but they're
  the most convienent way to run a bunch of HAL commands in a row.
- `scottlaird-lcectest1/`: tests for running on @scottlaird's first
  LCEC test machine (X86-64, many EtherCAT devices).  See `haltest.sh`
  for the script that does the actual testing.
- `scottlaird-lcectest2/`: tests for running on @scottlaird's second
  LCEC test machine (Raspberry Pi 4, only a few devices).

