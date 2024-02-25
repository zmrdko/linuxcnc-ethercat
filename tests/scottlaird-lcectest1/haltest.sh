#!/bin/bash

OUT=/tmp/testbench-1.out

# Load assorted test functions.
source $(dirname "$0")/../shlib/haltests.sh

echo "*** Test #1: invalid device type in XML"
echo "=== Killing old halrun"
halrun -U

echo "=== Starting halrun"
halrun -f missingtype.hal &> /tmp/testbench-init.out

echo "=== Verifying clean failure"
if ! grep "ERROR: Cannot find slave type" /tmp/testbench-init.out > /dev/null; then
    echo "ERROR: expected error not found in output."
    halrun -U; exit 1
fi
echo "*** Test #1 passes"
echo

echo "*** Test #2: Exercise I/O interfaces"
echo "=== Killing old halrun"
halrun -U
sleep 1

echo "=== Starting halrun"
halrun -f fulltest.hal > /tmp/testbench-init.out &

# It seems to take around 200ms per device to init, with a 2s delay, we only make it through D12 or so.
echo "... Sleeping for 5s to allow ethercat to finish initializing"
sleep 5 # let ethercat states settle

update

echo "=== Testing for failed devices"
if ! grep 'lcec.state-op' $OUT > /dev/null; then
    echo "ERROR: lcec is not loaded correctly."
    halrun -U; exit 1
fi

if ! grep 'lcec.link-up' $OUT > /dev/null; then
    echo "ERROR: lcec Ethernet link is not up."
    halrun -U; exit 1
fi

if grep 'FALSE  lcec.0.D[0-9]+.slave-state-op' $OUT > /dev/null; then
    echo "ERROR: not all slaves in state 'OP'"
    halrun -U; exit 1
fi


# This doesn't really *do* anything, but it's reasonable to verify
# that it's in a sane state.

echo "=== Testing initial configs"
echo "... Testing initial config of D0 (EK1100)"
test-pin-true D0 slave-online
test-slave-oper D0

echo "... Testing initial config of D1 (EL1008)"
test-slave-oper D1
test-pin-exists D1 din-7
test-pin-count D1 22

echo "... Testing initial config of D2 (EL1008, generic)"
test-slave-oper D2
test-pin-exists D2 din-7
test-pin-count D2 14

echo "... Testing initial config of D3 (EL2008)"
test-slave-oper D3
test-pin-exists D3 dout-7
test-pin-count D3 22

echo "... Testing initial config of D4 (EL2008, generic)"
test-slave-oper D4
test-pin-exists D4 dout-7
test-pin-count D4 14

echo "... Testing initial config of D5 (EL2084)"
test-slave-oper D5
test-pin-exists D5 dout-3
test-pin-count D5 14

echo "... Testing initial config of D6 (EL2022)"
test-slave-oper D6
test-pin-exists D6 dout-1
test-pin-count D6 10

echo "... Testing initial config of D7 (EL2022)"
test-slave-oper D7
test-pin-exists D7 dout-1
test-pin-count D7 10

echo "... Testing initial config of D8 (EL2034)"
test-slave-oper D8
test-pin-exists D8 dout-3
test-pin-count D8 14

echo "... Testing initial config of D9 (EL2798)"
test-slave-oper D9
test-pin-exists D9 dout-7
test-pin-count D9 22

echo "... Testing initial config of D10 (EL3068)"
test-slave-oper D10
test-pin-exists D10 ain-7-val
test-pin-notexists D10 ain-7-sync-err
test-pin-count D10 62

#echo "=== Testing initial config of D11 (EL6001)"
#if ! grep 'lcec.0.D11.dout-7' $OUT > /dev/null; then
#    echo "ERROR: device D11 (EL6001) does not have pin 'dout-7'"
#    halrun -U; exit 1
#fi
#
#if [ $(fgrep 'lcec.0.D11.' $OUT | wc -l) != 22 ]; then
#    echo "ERROR: device D11 (EL6001) has the wrong number of pins"
#    halrun -U; exit 1
#fi

echo "... Testing initial config of D12 (EL4004)"
test-slave-oper D12

#echo "... Testing initial config of D13 (EL3681)"
#test-slave-oper D13

#echo "... Testing initial config of D14 (EL2904)"
#test-slave-oper D14
#test-pin-exists D14 out-3
#test-pin-exists D14 fsoe-out-3
#test-pin-count D14 20

echo "... Testing initial config of D15 (EL3403)"
test-slave-oper D15
test-pin-exists D15 l2.cosphi
test-pin-count D15 44

echo "... Testing initial config of D16 (EL3204)"
test-slave-oper D16
test-pin-exists D16 temp-3-temperature
test-pin-count D16 30

echo "... Testing initial config of D18 (EL1859)"
test-slave-oper D18
test-pin-exists D18 din-7
test-pin-exists D18 dout-7
test-pin-count D18 38

echo "... Testing initial config of D19 (EL4032)"
test-slave-oper D19
test-pin-exists D19 aout-1-value
test-pin-exists D19 aout-1-min-dc
test-pin-count D19 28

echo "... Testing initial config of D20 (EL7041)"
test-slave-oper D20

echo "... Testing initial config of D21 (EK1110)"
test-slave-oper D21

echo "... Testing initial config of D22 (ECT60)"
test-slave-oper D22
test-pin-count D22 61

echo "... Testing initial config of D23 (EP2308)"
test-slave-oper D23
test-pin-exists D23 din-0
test-pin-exists D23 dout-1
test-pin-count D23 22

echo "=== Initial config tests pass"


echo "=== Testing Digital I/O"


echo "... Verifying initial state"
update
test-all-din-false

echo "... Checking D3.1->D1.1"
set-pin D3 dout-1 true
test-pin-true D3 dout-1
test-pin-true D1 din-1
test-all-din-true-count 1
set-pin D3 dout-1 false
test-all-din-false

echo "... Checking D4.3->D2.3"
set-pin D4 dout-3 true
test-pin-true D4 dout-3
test-pin-true D2 din-3
test-all-din-true-count 1
set-pin D4 dout-3 false
test-all-din-false

echo "... Checking D4.4->D1.5"
set-pin D4 dout-5 true
test-pin-true D4 dout-5
test-pin-true D1 din-5
test-all-din-true-count 1
set-pin D4 dout-5 false
test-all-din-false

echo "... checking D18 cross-connect"
set-pin D18 dout-0 true
test-pin-true D18 din-0
test-all-din-true-count 1
set-pin D18 dout-0 false
test-all-din-false

echo "... checking D18 to D1"
set-pin D18 dout-6 true
test-pin-true D1 din-7
test-all-din-true-count 1
set-pin D18 dout-6 false
test-all-din-false

echo "... checking D18 to D2"
set-pin D18 dout-7 true
test-pin-true D2 din-7
test-all-din-true-count 1
set-pin D18 dout-7 false
test-all-din-false

echo "... Checking D22 to D1"
#test-all-din-false
#set-pin D22 dout-1 true
#test-all-din-true-count 1
#test-pin-true D1 din-2
#set-pin D22 dout-1 false
#test-all-din-false
#
#set-pin D22 dout-2 true
#test-all-din-true-count 1
#test-pin-true D1 din-3
#set-pin D22 dout-2 false
#test-all-din-false

echo "... Checking D3 to D22"
#test-all-din-false
#set-pin D3 dout-2 true
#set-pin D3 dout-3 true
#set-pin D3 dout-4 true
#set-pin D3 dout-5 true
#test-all-din-true-count 1
#test-pin-true D22 din-1

echo "=== Testing Analog I/O"
echo "... Checking D10 current input"
test-pin-greater D10 ain-0-val 0.1

echo "... Checking D15 power measurements"
test-pin-greater D15 l0.frequency 59
test-pin-less D15 l0.frequency 61
test-pin-greater D15 l0.voltage 110
test-pin-less D15 l0.voltage 125


echo "... Checking D16 temperatures"
test-pin-greater D16 temp-0-temperature 15
test-pin-less D16 temp-0-temperature 30
test-pin-greater D16 temp-1-temperature 950
test-pin-less D16 temp-1-temperature 1050
test-pin-greater D16 temp-2-temperature 15
test-pin-less D16 temp-2-temperature 30
test-pin-greater D16 temp-3-temperature 950
test-pin-less D16 temp-3-temperature 1050

echo "... Checking D19.0->D10.1"
test-pin-less D10 ain-1-val 0.01
set-pin D19 aout-0-value 1
set-pin D19 aout-0-enable 1
test-pin-greater D10 ain-1-val 0.99
set-pin D19 aout-0-value 0
test-pin-less D10 ain-1-val 0.01


echo "=== Testing Steppers"
echo "... EL7041"
set-pin D20 enc-reset true
set-pin D20 enc-reset false
test-pin-equal D20 enc-pos 0
set-pin D20 srv-enable true
set-pin D20 srv-cmd 0.1
sleep 0.5  # Let the servo move a bit.
set-pin D20 srv-cmd 0
update
test-pin-greater D20 enc-pos 7000  # Currently hitting ~7500.

echo "... ECT60"
halcmd setp cia402.0.enable true
halcmd setp cia402.0.velocity-cmd 20000
sleep 1
update
test-pin-greater D22 current-rpm 100

echo "=== ALL TESTS PASS ==="
halrun -U -Q
