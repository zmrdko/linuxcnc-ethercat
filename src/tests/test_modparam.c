#include <stdio.h>

#include "../../src/lcec.h"
#include "tests.h"

TESTGLOBALSETUP;

static const lcec_modparam_desc_t mp_3[] = {
    {"positionLimitMin", 5, MODPARAM_TYPE_S32},
    {"positionLimitMax", 6, MODPARAM_TYPE_S32},
    {"swPositionLimitMin", 7, MODPARAM_TYPE_S32},
    {NULL},
};

static const lcec_modparam_desc_t mp_1[] = {
    {"positionLimitMax", 8, MODPARAM_TYPE_S32},
    {NULL},
};

static const lcec_modparam_desc_t mp_0[] = {
    {NULL},
};

TESTFUNC(test_modparm_len) {
  TESTSETUP;

  TESTINT(lcec_modparam_desc_len(mp_3), 3);
  TESTINT(lcec_modparam_desc_len(mp_1), 1);
  TESTINT(lcec_modparam_desc_len(mp_0), 0);

  TESTRESULTS;
}

TESTMAIN
