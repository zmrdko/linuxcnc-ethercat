#include <stdio.h>

#include "../../src/lcec.h"
#include "../../src/devices/lcec_class_cia402.h"
#include "tests.h"

TESTGLOBALSETUP;

static const lcec_modparam_desc_t per_channel_mps[] = {
    {"aaa", 0x1000, MODPARAM_TYPE_S32},
    {"bbb", 0x1010, MODPARAM_TYPE_S32},
    {"ccc", 0x1020, MODPARAM_TYPE_S32},
    {NULL},
};

static const lcec_modparam_desc_t device_mps[] = {
    {"ddd", 0x1, MODPARAM_TYPE_S32},
    {NULL},
};

TESTFUNC(test_modparm_len) {
  TESTSETUP;
  lcec_modparam_desc_t *channelized_mps;
  
  TESTINT(lcec_modparam_desc_len(per_channel_mps), 3);
  TESTINT(lcec_modparam_desc_len(device_mps), 1);
  
  channelized_mps = lcec_cia402_channelized_modparams(per_channel_mps);
  TESTNOTNULL(channelized_mps);

  TESTINT(lcec_modparam_desc_len(channelized_mps), 15);

  lcec_modparam_desc_t *all_mps = lcec_modparam_desc_concat(channelized_mps, device_mps);
  TESTNOTNULL(all_mps);

  TESTINT(lcec_modparam_desc_len(all_mps), 16);

  TESTRESULTS;
}

TESTMAIN
