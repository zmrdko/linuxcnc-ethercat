#include <stdio.h>

#include "../../src/lcec.h"
#include "tests.h"

static const lcec_lookuptable_int_t table1[] = {
    {"Pt100", 0},          // Pt100 sensor,
    {"Ni100", 1},          // Ni100 sensor, -60 to 250C
    {"Pt1000", 2},         // Pt1000 sensor, -200 to 850C
    {"Pt500", 3},          // Pt500 sensor, -200 to 850C
    {"Pt200", 4},          // Pt200 sensor, -200 to 850C
    {"Ni1000", 5},         // Ni1000 sensor, -60 to 250C
    {"Ni1000-TK5000", 6},  // Ni1000-TK5000, -30 to 160C
    {"Ni120", 7},          // Ni120 sensor, -60 to 320C
    {"Ohm/16", 8},         // no sensor, report Ohms directly.  0-4095 Ohms
    {"Ohm/64", 9},         // no sensor, report Ohms directly.  0-1023 Ohms
    {NULL},
};

static const lcec_lookuptable_int_t table2[] = {
    {"Ohm/16", 1},
    {"Ohm/64", 1},
    {NULL},
};

static const lcec_lookuptable_double_t table3[] = {
    {"Ohm/16", 1.0 / 16},
    {"Ohm/64", 1.0 / 64},
    {NULL},
};

int test_lookupint(void) {
  TESTSETUP;

  // Looking up "Pt100" should return 0.
  TESTINT(lcec_lookupint(table1, "Pt100", -1), 0);
  // Looking up "Pt101" (which isn't in the table) should return -1.
  TESTINT(lcec_lookupint(table1, "Pt101", -1), -1);
  // Looking up "Pt101" (which isn't in the table) should return -10, if we use -10 as the default value for lookups.
  TESTINT(lcec_lookupint(table1, "Pt101", -10), -10);
  // Looking up "pt100" (which isn't in the table) should return -1.
  TESTINT(lcec_lookupint(table1, "pt100", -1), -1);

  TESTRESULTS;
}

int test_lookupint_i(void) {
  TESTSETUP;

  // Same as test_lookupint, except...
  TESTINT(lcec_lookupint_i(table1, "Pt100", -1), 0);
  TESTINT(lcec_lookupint_i(table1, "Pt101", -1), -1);
  TESTINT(lcec_lookupint_i(table1, "Pt101", -10), -10);

  // Do a case-insensitive lookup.  Unlike test_lookupint() above, this should return 0, not -1.
  TESTINT(lcec_lookupint_i(table1, "pt100", -1), 0);

  TESTRESULTS;
}

int test_sparsedefault(void) {
  TESTSETUP;

  // Verify that throwing random things at a table with a 0 (or
  // non-negative) defaults behaves sanely.
  TESTINT(lcec_lookupint(table2, "Pt100", 0), 0);
  TESTINT(lcec_lookupint(table2, "Pt102", 0), 0);
  TESTINT(lcec_lookupint(table2, "Ohm/16", 0), 1);
  TESTINT(lcec_lookupint(table2, "Ohm/64", 0), 1);

  TESTRESULTS;
}

int test_lookupdouble(void) {
  TESTSETUP;

  // I should really create a TESTDOUBLE() macro, but this works well enough for now.
  // Test that lookups via lcec_lookupdouble() work correctly.
  TESTINT(lcec_lookupdouble(table3, "Pt100", 1.0), 1);
  TESTINT(lcec_lookupdouble(table3, "Ohm/16", 1.0), 0);
  TESTINT(lcec_lookupdouble(table3, "Ohm/64", 1.0), 0);

  // This isn't present, unless we're doing case-insensitive matching, which we shouldn't be here.
  TESTINT(lcec_lookupdouble(table3, "ohm/16", 1.0), 1);

  TESTRESULTS;
}

int test_lookupdouble_i(void) {
  TESTSETUP;

  // Same as test_lookupdouble, except...
  TESTINT(lcec_lookupdouble_i(table3, "Pt100", 1.0), 1);
  TESTINT(lcec_lookupdouble_i(table3, "Ohm/16", 1.0), 0);
  TESTINT(lcec_lookupdouble_i(table3, "Ohm/64", 1.0), 0);

  // Unlike test_lookupdouble, this should succeed.
  TESTINT(lcec_lookupdouble_i(table3, "ohm/16", 1.0), 0);

  TESTRESULTS;
}

int main(int argc, char **argv) {
  TESTMAINSETUP;

  TESTMAIN(test_lookupint);
  TESTMAIN(test_lookupint_i);
  TESTMAIN(test_sparsedefault);
  TESTMAIN(test_lookupdouble);
  TESTMAIN(test_lookupdouble_i);

  TESTMAINRESULTS;
}
