#!/bin/bash

#
# This dumps all of the SDO values for one or more devices.  Call it with '-p 2' to show all of the SDOs from slave #2.

ethercat sdos $* | while IFS= read -r line; do
  if echo "$line" | egrep -q '^  0x....:..'; then
    val=$(ethercat $* upload $(echo "$line" | sed 's/:\(..\), .*/ 0x\1/') 2> /dev/null)
    printf "%-80s   %s\n" "$(echo "$line" | cat -vT)" "$val" 2> /dev/null
  else
    echo "$line"
  fi

done
