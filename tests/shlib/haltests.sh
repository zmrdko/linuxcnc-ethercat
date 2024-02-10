update() {
    halcmd show > $OUT
}

set-pin() {
    var="lcec.0.$1.$2"
    halcmd setp $var $3
    sleep 0.1
    update
}

test-pin-exists() {
    var="lcec.0.$1.$2"
    if ! egrep " +$var$" $OUT > /dev/null; then
	echo "ERROR: $var does not exist"
	halrun -U
	exit 1
    fi
}


test-pin-notexists() {
    var="lcec.0.$1.$2"
    if egrep " +$var$" $OUT > /dev/null; then
	echo "ERROR: $var exists but should not"
	halrun -U
	exit 1
    fi
}


test-pin-true() {
    var="lcec.0.$1.$2"
    if ! egrep "TRUE +$var$" $OUT > /dev/null; then
	echo "ERROR: $var is not true"
	halrun -U
	exit 1
    fi
}

test-pin-false() {
    var="lcec.0.$1.$2"
    if ! egrep "FALSE +$var$" $OUT > /dev/null; then
	echo "ERROR: $var is not true"
	halrun -U
	exit 1
    fi
}

test-pin-equal() {
    var="lcec.0.$1.$2"
    val=$(halcmd getp $var)
    if [ $val != $3 ]; then
	echo "ERROR: $var ($val) is not equal to $3"
	halrun -U
	exit 1
    fi
}

test-pin-greater() {
    var="lcec.0.$1.$2"
    val=$(halcmd getp $var)
    result=$(echo "$val > $3" | bc -l)
    if [ $result == 0 ]; then
	echo "ERROR: $var ($val) is not greater than $3"
	halrun -U
	exit 1
    fi
}

test-pin-less() {
    var="lcec.0.$1.$2"
    val=$(halcmd getp $var)
    result=$(echo "$val < $3" | bc -l)
    if [ $result == 0 ]; then
	echo "ERROR: $var ($val) is not less than $3"
	halrun -U
	exit 1
    fi
}

test-pin-count() {
    var=" lcec\.0\.$1"
    pincount=$(egrep " +$var\." $OUT | wc -l)
    if [ $pincount -ne $2 ]; then
	echo "ERROR: slave $1 has $pincount pins, expected $2"
	halrun -U
	exit 1
    fi
}

test-all-din-false() {
    if grep 'TRUE  lcec.0.D[0-9]+.din-[0-9]+$' $OUT ; then
	echo "ERROR: some digital input pins are already true"
	halrun -U; exit 1
    fi
}

test-all-din-true-count() {
    val=$1
    if [ $(egrep 'TRUE  lcec.0\.D[0-9]+\.din-[0-9]+$' $OUT | egrep -v 'D22.din-[12]' | wc -l) != $val ]; then
	echo "ERROR: wrong number of 'true' pins, that should have only flipped $val bit(s)."
	echo "True bits (if any):"
	egrep 'TRUE  lcec.0\.D[0-9]+\.din-[0-9]+$' $OUT
	echo "exiting."
	halrun -U; exit 1
    fi
}


test-slave-oper() {
    test-pin-true $1 slave-oper
}
