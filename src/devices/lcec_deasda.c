//
//    Copyright (C) 2014 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

/// @file
/// @brief Driver for Delta ASDA servo controllers

#include "lcec_deasda.h"

#include "../lcec.h"
#include "lcec_class_dout.h"
#include "lcec_class_enc.h"

#define FLAG_LOWRES_ENC  1 << 0  // Device uses low res encoder as default
#define FLAG_HIGHRES_ENC 1 << 1  // Device uses high res encoder as default x3 series
#define FLAG_DOUT        1 << 2  // Device has digital outputs
#define FLAG_HOMING      (1 << 3)  // Device supports drive-internal CiA 402 homing (B3)

#define M_OPERATIONMODE 0
#define M_DIGITALOUT 1
#define M_ENABLE_HOMING 2
#define M_HOME_METHOD 3
#define M_HOME_OFFSET 4
#define M_HOME_SPEED_FAST 5
#define M_HOME_SPEED_SLOW 6
#define M_HOME_ACCEL 7
#define M_HOME_FAKE_FB 8
#define M_EGEAR_NUM 9
#define M_EGEAR_DEN 10

#define DEASDA_EGEAR_PULSES_PER_REV 16777216ULL  // internal drive resolution (24 bit)

#define DEASDA_PULSES_PER_REV_DEFLT_LOWRES  (1280000)   // this is the default value for A2 (default value)
#define DEASDA_PULSES_PER_REV_DEFLT_HIGHRES (16777216)  // this is the default value for A3

#define DEASDA_RPM_FACTOR (0.1)
#define DEASDA_RPM_RCPT   (1.0 / DEASDA_RPM_FACTOR)
#define DEASDA_RPM_MUL    (60.0)
#define DEASDA_RPM_DIV    (1.0 / 60.0)

#define DEASDA_FAULT_AUTORESET_CYCLES  100
#define DEASDA_FAULT_AUTORESET_RETRIES 3

#define DEASDA_OPMODE_HM  6
#define DEASDA_OPMODE_CSP 8
#define DEASDA_OPMODE_CSV 9

// Homing (ASDA-B3, CiA 402 homing mode, see B3 manual chapter 12.3.4).
// Defaults match the drive defaults for objects 6098h/6099h/609Ah.
#define DEASDA_HOME_METHOD_DEFLT     35   // 35 = current position is the origin (no motion)
#define DEASDA_HOME_SPEED_FAST_DEFLT 100  // 6099h:01, unit 0.1 rpm
#define DEASDA_HOME_SPEED_SLOW_DEFLT 20   // 6099h:02, unit 0.1 rpm
#define DEASDA_HOME_ACCEL_DEFLT      100  // 609Ah, ms from 0 to 3000 rpm

static int lcec_deasda_init(int comp_id, lcec_slave_t *slave);

static const lcec_modparam_desc_t lcec_deasda_modparams[] = {
  {"opmode", M_OPERATIONMODE, MODPARAM_TYPE_STRING, "CSP", "Operation mode, CSV or CSP"},
  {"enableDigitalOutput", M_DIGITALOUT, MODPARAM_TYPE_BIT, "true", "Enable digital output ports"},
    {NULL},
};

static const lcec_modparam_desc_t lcec_deasda_modparams_b3[] = {
  {"opmode", M_OPERATIONMODE, MODPARAM_TYPE_STRING, "CSP", "Operation mode, CSV or CSP"},
  {"enableHoming", M_ENABLE_HOMING, MODPARAM_TYPE_BIT, "false", "Enable drive-internal CiA 402 homing (CSP mode only)"},
  {"homeMethod", M_HOME_METHOD, MODPARAM_TYPE_S32, "35", "Homing method (OD 6098h): 1-35, or -1..-4 for hard stop"},
  {"homeOffset", M_HOME_OFFSET, MODPARAM_TYPE_S32, "0", "Home offset in PUU (OD 607Ch)"},
  {"homeSpeedFast", M_HOME_SPEED_FAST, MODPARAM_TYPE_U32, "100", "Speed searching for switch, 0.1 rpm (OD 6099h:01)"},
  {"homeSpeedSlow", M_HOME_SPEED_SLOW, MODPARAM_TYPE_U32, "20", "Speed searching for Z pulse, 0.1 rpm (OD 6099h:02)"},
  {"homeAccel", M_HOME_ACCEL, MODPARAM_TYPE_U32, "100", "Homing accel/decel time 0-3000 rpm in ms (OD 609Ah)"},
  {"eGearNumerator", M_EGEAR_NUM, MODPARAM_TYPE_U32, "1", "E-Gear ratio numerator N (OD 6093h:01 / P1.044); set together with eGearDenominator"},
  {"eGearDenominator", M_EGEAR_DEN, MODPARAM_TYPE_U32, "1", "E-Gear ratio denominator M (OD 6093h:02 / P1.045); PUU per rev = 16777216 * M / N"},
  {"homeFakeFeedback", M_HOME_FAKE_FB, MODPARAM_TYPE_BIT, "true", "While srv-home is set, report enc-pos = srv-pos-cmd (avoids following error)"},
    {NULL},
};

typedef struct {
  const char *name;  // Mode type name
  uint16_t value;    // Which value needs to be set in 0x6060:00 to enable this mode
} drive_operationmodes_t;

static const drive_operationmodes_t drive_operationmodes[] = {
    {"CSV", DEASDA_OPMODE_CSV},
    {"CSP", DEASDA_OPMODE_CSP},
    {NULL},
};

// Note that DeASDA refers to A2-E series of drives and is deliberatly not refering to A2 in its name to ensure compatability with legace
// configurations.
static lcec_typelist_t types[] = {
    {"DeASDA", LCEC_DELTA_VID, 0x10305070, 0, NULL, lcec_deasda_init, lcec_deasda_modparams, FLAG_LOWRES_ENC | FLAG_DOUT},
    {"DeASDA3", LCEC_DELTA_VID, 0x00006010, 0, NULL, lcec_deasda_init, lcec_deasda_modparams, FLAG_HIGHRES_ENC | FLAG_DOUT},
    {"DeASDB3", LCEC_DELTA_VID, 0x00006080, 0, NULL, lcec_deasda_init, lcec_deasda_modparams_b3, FLAG_HIGHRES_ENC | FLAG_HOMING},
    {NULL},
};

ADD_TYPES(types);

typedef struct {
  hal_float_t *vel_fb;
  hal_float_t *vel_fb_rpm;
  hal_float_t *vel_fb_rpm_abs;
  hal_float_t *vel_rpm;
  hal_bit_t *ready;
  hal_bit_t *switched_on;
  hal_bit_t *oper_enabled;
  hal_bit_t *fault;
  hal_bit_t *volt_enabled;
  hal_bit_t *quick_stoped;
  hal_bit_t *on_disabled;
  hal_bit_t *warning;
  hal_bit_t *remote;
  hal_bit_t *at_speed;
  hal_bit_t *limit_active;
  hal_bit_t *zero_speed;
  hal_bit_t *switch_on;
  hal_bit_t *enable_volt;
  hal_bit_t *quick_stop;
  hal_bit_t *enable;
  hal_bit_t *fault_reset;
  hal_bit_t *halt;
  hal_u32_t *operation_mode;
  hal_float_t *cmd_value;

  // Homing pins, only exported when enableHoming is set (B3, CSP).
  hal_bit_t *home;
  hal_bit_t *homed;
  hal_bit_t *homing;
  hal_bit_t *home_error;

  lcec_param_float_t pos_scale;
  lcec_param_float_t extenc_scale;
  lcec_param_u32_t pprev;
  lcec_param_u32_t fault_autoreset_cycles;
  lcec_param_u32_t fault_autoreset_retries;

  hal_float_t *torque;
  hal_bit_t *neg_lim_switch;
  hal_bit_t *pos_lim_switch;
  hal_bit_t *home_switch;
  hal_bit_t *di_1;
  hal_bit_t *di_2;
  hal_bit_t *di_3;
  hal_bit_t *di_4;
  hal_bit_t *di_5;
  hal_bit_t *di_6;
  hal_bit_t *di_7;

  lcec_class_enc_data_t enc;
  lcec_class_enc_data_t extenc;

  hal_float_t pos_scale_old;
  double pos_scale_rcpt;

  unsigned int status_pdo_os;
  unsigned int currpos_pdo_os;
  unsigned int currvel_pdo_os;
  unsigned int extenc_pdo_os;
  unsigned int control_pdo_os;
  unsigned int cmdvalue_pdo_os;
  unsigned int divalue_pdo_os;
  unsigned int torque_pdo_os;
  unsigned int opmode_cmd_pdo_os;   // 0x6060, only mapped when homing is enabled
  unsigned int opmode_disp_pdo_os;  // 0x6061, only mapped when homing is enabled

  int homing_enabled;
  int home_fake_fb;         // report enc-pos = command while srv-home is set
  uint16_t last_status;    // last statusword (0x6041)
  int32_t last_pos_raw;     // last position actual value (0x6064)
  int8_t opmode_disp;       // last modes of operation display (0x6061)
  int home_start_sent;      // controlword bit 4 is being asserted
  int home_seen_unhomed;    // drive reported "not attained" after start (ignores stale bits)
  int home_start_cycles;    // cycles since the start edge was first sent

  hal_bit_t last_switch_on;
  hal_bit_t internal_fault;

  hal_u32_t fault_reset_retry;
  hal_u32_t fault_reset_state;
  hal_u32_t fault_reset_cycle;

  lcec_class_dout_channels_t *dout;
} lcec_deasda_data_t;

static const lcec_pindesc_t slave_pins[] = {
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb), "%s.%s.%s.srv-vel-fb"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm), "%s.%s.%s.srv-vel-fb-rpm"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_fb_rpm_abs), "%s.%s.%s.srv-vel-fb-rpm-abs"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, vel_rpm), "%s.%s.%s.srv-vel-rpm"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, ready), "%s.%s.%s.srv-ready"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, switched_on), "%s.%s.%s.srv-switched-on"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, oper_enabled), "%s.%s.%s.srv-oper-enabled"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, fault), "%s.%s.%s.srv-fault"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, volt_enabled), "%s.%s.%s.srv-volt-enabled"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, quick_stoped), "%s.%s.%s.srv-quick-stoped"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, on_disabled), "%s.%s.%s.srv-on-disabled"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, warning), "%s.%s.%s.srv-warning"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, remote), "%s.%s.%s.srv-remote"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, at_speed), "%s.%s.%s.srv-at-speed"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, limit_active), "%s.%s.%s.srv-limit-active"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, zero_speed), "%s.%s.%s.srv-zero-speed"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, switch_on), "%s.%s.%s.srv-switch-on"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, enable_volt), "%s.%s.%s.srv-enable-volt"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, quick_stop), "%s.%s.%s.srv-quick-stop"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, enable), "%s.%s.%s.srv-enable"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, fault_reset), "%s.%s.%s.srv-fault-reset"},
    {HAL_BIT, HAL_IN, offsetof(lcec_deasda_data_t, halt), "%s.%s.%s.srv-halt"},
    {HAL_U32, HAL_OUT, offsetof(lcec_deasda_data_t, operation_mode), "%s.%s.%s.srv-operation-mode"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_deasda_data_t, torque),
        "%s.%s.%s.srv-torque-rel"},  // relative value (5) - hence current would be redundant
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_1), "%s.%s.%s.din-1"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_2), "%s.%s.%s.din-2"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_3), "%s.%s.%s.din-3"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_4), "%s.%s.%s.din-4"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_5), "%s.%s.%s.din-5"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_6), "%s.%s.%s.din-6"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, di_7), "%s.%s.%s.din-7"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, neg_lim_switch), "%s.%s.%s.din-neg-lim"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, pos_lim_switch), "%s.%s.%s.din-pos-lim"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, home_switch), "%s.%s.%s.din-home"},

    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

static const lcec_pindesc_t slave_pins_csv[] = {
    {HAL_FLOAT, HAL_IN, offsetof(lcec_deasda_data_t, cmd_value), "%s.%s.%s.srv-vel-cmd"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

static const lcec_pindesc_t slave_pins_csp[] = {
    {HAL_FLOAT, HAL_IN, offsetof(lcec_deasda_data_t, cmd_value), "%s.%s.%s.srv-pos-cmd"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

static const lcec_pindesc_t slave_pins_homing[] = {
    {HAL_BIT, HAL_IO, offsetof(lcec_deasda_data_t, home), "%s.%s.%s.srv-home"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, homed), "%s.%s.%s.srv-homed"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, homing), "%s.%s.%s.srv-homing"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_deasda_data_t, home_error), "%s.%s.%s.srv-home-error"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

// Exposed parameters are identical for both drives and modes
static const lcec_paramdesc_t slave_params[] = {
    {HAL_FLOAT, HAL_RW, offsetof(lcec_deasda_data_t, pos_scale), "%s.%s.%s.pos-scale"},
    {HAL_FLOAT, HAL_RW, offsetof(lcec_deasda_data_t, extenc_scale), "%s.%s.%s.extenc-scale"},
    {HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, pprev), "%s.%s.%s.srv-pulses-per-rev"},
    {HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_cycles), "%s.%s.%s.srv-fault-autoreset-cycles"},
    {HAL_U32, HAL_RW, offsetof(lcec_deasda_data_t, fault_autoreset_retries), "%s.%s.%s.srv-fault-autoreset-retries"},
    {HAL_TYPE_UNSPECIFIED},
};

static void lcec_deasda_check_scales(lcec_deasda_data_t *hal_data);

static void lcec_deasda_read(lcec_slave_t *slave, long period);
static void lcec_deasda_write_csv(lcec_slave_t *slave, long period);
static void lcec_deasda_write_csp(lcec_slave_t *slave, long period);

static const drive_operationmodes_t *drive_opmode(char *drivemode);

static int lcec_deasda_init(int comp_id, lcec_slave_t *slave) {
  lcec_master_t *master = slave->master;
  lcec_deasda_data_t *hal_data;
  int err;
  uint32_t tu;
  int8_t ti;
  drive_operationmodes_t const *driveopmode;
  static uint16_t operationmode;
  lcec_syncs_t *syncs;
  uint64_t flags;
  int enable_dout = slave->flags & FLAG_DOUT;
  flags = slave->flags;

  syncs = LCEC_HAL_ALLOCATE(lcec_syncs_t);

  // Determine Operation Mode (modParam opmode) as this defines everything else
  LCEC_CONF_MODPARAM_VAL_T *pval;
  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "  - checking modparam opmode for %s \n", slave->name);
  pval = lcec_modparam_get(slave, M_OPERATIONMODE);
  if (pval != NULL) {
    rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - found opmode param for %s \n", slave->name);

    driveopmode = drive_opmode(pval->str);

    if (driveopmode != NULL) {
      rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - setting mode for %s to %d\n", slave->name, driveopmode->value);
      operationmode = driveopmode->value;
    } else {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "unknown mode \"%s\" for slave %s.%s !\n", pval->str, master->name, slave->name);
      return -1;
    }
  } else {
    // This would be the case when modparam mode has not been set ==> back to CSV
    rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - no opmode param for %s \n found. Defaulting to CSV.", slave->name);
    operationmode = DEASDA_OPMODE_CSV;
  }

  pval = lcec_modparam_get(slave, M_DIGITALOUT);
  if (pval) {
    enable_dout = pval->bit;
  }

  // Homing is only available on the B3 (the only type that registers the homing modparams)
  // and only together with CSP: the drive needs a position target to resume after homing.
  int enable_homing = 0;
  if (slave->flags & FLAG_HOMING) {
    pval = lcec_modparam_get(slave, M_ENABLE_HOMING);
    if (pval) enable_homing = pval->bit;
  }
  if (enable_homing && operationmode != DEASDA_OPMODE_CSP) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "enableHoming requires opmode CSP for slave %s.%s !\n", master->name, slave->name);
    return -1;
  }

  // Set up PDO sync configuration
  lcec_syncs_init(slave, syncs);
  lcec_syncs_add_sync(syncs, EC_DIR_OUTPUT, EC_WD_DEFAULT);
  lcec_syncs_add_sync(syncs, EC_DIR_INPUT, EC_WD_DEFAULT);

  // Set up output PDO syncs
  lcec_syncs_add_sync(syncs, EC_DIR_OUTPUT, EC_WD_DEFAULT);
  lcec_syncs_add_pdo_info(syncs, 0x1602);
  lcec_syncs_add_pdo_entry(syncs, 0x6040, 0, 16);  // Control word

  // We could actually map both of these at the same time without
  // problems, but for compabilities's sake, I don't want to change it
  // right now.
  if (operationmode == DEASDA_OPMODE_CSV) {
    lcec_syncs_add_pdo_entry(syncs, 0x60ff, 0, 32);  // Target Velocity
  } else {
    lcec_syncs_add_pdo_entry(syncs, 0x607a, 0, 32);  // Target Position
  }

  if (enable_homing) {
    lcec_syncs_add_pdo_entry(syncs, 0x6060, 0, 8);  // Modes of operation (switched to homing at runtime)
  }

  // Only add digital outs on models that actually have the hardware.
  if (enable_dout) {
    lcec_syncs_add_pdo_entry(syncs, 0x60fe, 1, 32);  // Digital outputs
  }

  // Set up input PDO syncs
  lcec_syncs_add_sync(syncs, EC_DIR_INPUT, EC_WD_DEFAULT);
  lcec_syncs_add_pdo_info(syncs, 0x1a02);
  lcec_syncs_add_pdo_entry(syncs, 0x6041, 0, 16);  // Status word
  lcec_syncs_add_pdo_entry(syncs, 0x606c, 0, 32);  // Current velocity
  lcec_syncs_add_pdo_entry(syncs, 0x6064, 0, 32);  // Current position
  lcec_syncs_add_pdo_entry(syncs, 0x2511, 0, 32);  // External encoder
  lcec_syncs_add_pdo_entry(syncs, 0x6077, 0, 16);  // Current torque
  lcec_syncs_add_pdo_entry(syncs, 0x60fd, 0, 32);  // Digital inputs
  if (enable_homing) {
    lcec_syncs_add_pdo_entry(syncs, 0x6061, 0, 8);  // Modes of operation display
  }
  slave->sync_info = &syncs->syncs[0];

  // initialize callbacks
  slave->proc_read = lcec_deasda_read;

  if (operationmode == DEASDA_OPMODE_CSV) {
    slave->proc_write = lcec_deasda_write_csv;
  } else if (operationmode == DEASDA_OPMODE_CSP) {
    slave->proc_write = lcec_deasda_write_csp;
  }
  // alloc hal memory
  hal_data = LCEC_HAL_ALLOCATE(lcec_deasda_data_t);
  slave->hal_data = hal_data;

  // Set up digital outputs.  These names should match the A3, unclear about other models.
  //
  // TODO(scottlaird): It appears that various A2 and A3 models have
  // different numbers of digital out (and in?) ports.  We'll probably
  // want to make this configurable, one way or another.
  if (enable_dout) {
    hal_data->dout = lcec_dout_allocate_channels(4);
    hal_data->dout->channels[0] = lcec_dout_register_channel_packed(slave, 0x60fe, 0x01, 16, "dout-d01");
    hal_data->dout->channels[1] = lcec_dout_register_channel_packed(slave, 0x60fe, 0x01, 17, "dout-d02");
    hal_data->dout->channels[2] = lcec_dout_register_channel_packed(slave, 0x60fe, 0x01, 18, "dout-d03");
    hal_data->dout->channels[3] = lcec_dout_register_channel_packed(slave, 0x60fe, 0x01, 19, "dout-d04");

    if (lcec_write_sdo32(slave, 0x60fe, 0x02, 0x000f0000) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to configure slave %s.%s sdo for enabling digital output ports 1-4\n",
          master->name, slave->name);
      return -1;
    }
  }

  // set to 0x6060 to requested mode (CSV, CSP)
  if (lcec_write_sdo8(slave, 0x6060, 0x00, operationmode) != 0) {
    rtapi_print_msg(
        RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo to op mode %d\n", master->name, slave->name, operationmode);
    return -1;
  }

  // homing configuration (OD 6098h, 6099h, 609Ah, 607Ch); only the values are
  // written here, homing itself is started at runtime via srv-home.
  if (enable_homing) {
    int32_t home_method = DEASDA_HOME_METHOD_DEFLT;
    int32_t home_offset = 0;
    uint32_t home_fast = DEASDA_HOME_SPEED_FAST_DEFLT;
    uint32_t home_slow = DEASDA_HOME_SPEED_SLOW_DEFLT;
    uint32_t home_accel = DEASDA_HOME_ACCEL_DEFLT;

    if ((pval = lcec_modparam_get(slave, M_HOME_METHOD)) != NULL) home_method = pval->s32;
    if ((pval = lcec_modparam_get(slave, M_HOME_OFFSET)) != NULL) home_offset = pval->s32;
    if ((pval = lcec_modparam_get(slave, M_HOME_SPEED_FAST)) != NULL) home_fast = pval->u32;
    if ((pval = lcec_modparam_get(slave, M_HOME_SPEED_SLOW)) != NULL) home_slow = pval->u32;
    if ((pval = lcec_modparam_get(slave, M_HOME_ACCEL)) != NULL) home_accel = pval->u32;

    // B3 supports 1..35 and -1..-4 (hard stop); 15/16 are reserved.
    if (home_method < -4 || home_method > 35 || home_method == 0 || home_method == 15 || home_method == 16) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "invalid homeMethod %d for slave %s.%s (valid: 1-14, 17-35, -1..-4)\n", home_method,
          master->name, slave->name);
      return -1;
    }

    if (lcec_write_sdo8(slave, 0x6098, 0x00, (uint8_t)(int8_t)home_method) != 0 ||
        lcec_write_sdo32(slave, 0x607c, 0x00, (uint32_t)home_offset) != 0 || lcec_write_sdo32(slave, 0x6099, 0x01, home_fast) != 0 ||
        lcec_write_sdo32(slave, 0x6099, 0x02, home_slow) != 0 || lcec_write_sdo32(slave, 0x609a, 0x00, home_accel) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure homing sdos for slave %s.%s\n", master->name, slave->name);
      return -1;
    }
  }

  // E-Gear (OD 6093h = P1.044 / P1.045).  Optional; without it the drive keeps its
  // EtherCAT default of 1:1.  Both values must be given together.
  uint64_t egear_pprev = 0;  // PUU per motor revolution implied by the E-Gear, 0 = not set
  if (slave->flags & FLAG_HOMING) {
    LCEC_CONF_MODPARAM_VAL_T *pnum = lcec_modparam_get(slave, M_EGEAR_NUM);
    LCEC_CONF_MODPARAM_VAL_T *pden = lcec_modparam_get(slave, M_EGEAR_DEN);
    if (pnum || pden) {
      uint32_t egear_num, egear_den;
      if (!pnum || !pden || pnum->u32 == 0 || pden->u32 == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "eGearNumerator and eGearDenominator must both be set and non-zero for slave %s.%s\n",
            master->name, slave->name);
        return -1;
      }
      egear_num = pnum->u32;
      egear_den = pden->u32;
      // PUU per revolution = 16777216 * M / N, rounded
      egear_pprev = (DEASDA_EGEAR_PULSES_PER_REV * egear_den + egear_num / 2) / egear_num;
      if (egear_pprev == 0 || egear_pprev > 0xffffffffULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "E-Gear %u/%u gives an unusable PUU per revolution for slave %s.%s\n", egear_num,
            egear_den, master->name, slave->name);
        return -1;
      }
      if (lcec_write_sdo32(slave, 0x6093, 0x01, egear_num) != 0 || lcec_write_sdo32(slave, 0x6093, 0x02, egear_den) != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure E-Gear sdo 6093h for slave %s.%s\n", master->name, slave->name);
        return -1;
      }
    }
  }

  // set interpolation time period
  tu = master->app_time_period;
  ti = -9;

  while ((tu % 10) == 0 || tu > 255) {
    tu /= 10;
    ti++;
  }
  if (lcec_write_sdo8(slave, 0x60C2, 0x01, (uint8_t)tu) != 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ipol time period units\n", master->name, slave->name);
    return -1;
  }
  if (lcec_write_sdo8(slave, 0x60C2, 0x02, ti) != 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ipol time period index\n", master->name, slave->name);
    return -1;
  }

  if (operationmode == DEASDA_OPMODE_CSV) {
    // initialize POD entries
    lcec_pdo_init(slave, 0x6041, 0x00, &hal_data->status_pdo_os, NULL);
    lcec_pdo_init(slave, 0x606C, 0x00, &hal_data->currvel_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6064, 0x00, &hal_data->currpos_pdo_os, NULL);
    lcec_pdo_init(slave, 0x2511, 0x00, &hal_data->extenc_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6040, 0x00, &hal_data->control_pdo_os, NULL);
    lcec_pdo_init(slave, 0x60FF, 0x00, &hal_data->cmdvalue_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6077, 0x00, &hal_data->torque_pdo_os, NULL);
    lcec_pdo_init(slave, 0x60FD, 0x00, &hal_data->divalue_pdo_os, NULL);

    // export pins common
    if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;

    // export pins specific
    if ((err = lcec_pin_newf_list(hal_data, slave_pins_csv, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;

  } else if (operationmode == DEASDA_OPMODE_CSP) {
    // initialize POD entries
    lcec_pdo_init(slave, 0x6041, 0x00, &hal_data->status_pdo_os, NULL);
    lcec_pdo_init(slave, 0x606C, 0x00, &hal_data->currvel_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6064, 0x00, &hal_data->currpos_pdo_os, NULL);
    lcec_pdo_init(slave, 0x2511, 0x00, &hal_data->extenc_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6040, 0x00, &hal_data->control_pdo_os, NULL);
    lcec_pdo_init(slave, 0x607A, 0x00, &hal_data->cmdvalue_pdo_os, NULL);
    lcec_pdo_init(slave, 0x6077, 0x00, &hal_data->torque_pdo_os, NULL);
    lcec_pdo_init(slave, 0x60FD, 0x00, &hal_data->divalue_pdo_os, NULL);

    // export pins common
    if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;
    // export pins specific
    if ((err = lcec_pin_newf_list(hal_data, slave_pins_csp, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;

    if (enable_homing) {
      lcec_pdo_init(slave, 0x6060, 0x00, &hal_data->opmode_cmd_pdo_os, NULL);
      lcec_pdo_init(slave, 0x6061, 0x00, &hal_data->opmode_disp_pdo_os, NULL);
      if ((err = lcec_pin_newf_list(hal_data, slave_pins_homing, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;
    }
  }
  hal_data->homing_enabled = enable_homing;
  hal_data->home_fake_fb = 1;
  if (enable_homing && (pval = lcec_modparam_get(slave, M_HOME_FAKE_FB)) != NULL) hal_data->home_fake_fb = pval->bit;

  LCEC_PIN_U32_SET(hal_data->operation_mode, operationmode);
  // export parameters
  if ((err = lcec_param_newf_list(hal_data, slave_params, LCEC_MODULE_NAME, master->name, slave->name)) != 0) return err;

  // init subclasses for encoders
  if ((err = class_enc_init(slave, &hal_data->enc, 32, "enc")) != 0) return err;
  if ((err = class_enc_init(slave, &hal_data->extenc, 32, "extenc")) != 0) return err;

  // initialize variables
  LCEC_PARAM_FLOAT_SET(hal_data->pos_scale, 1.0);
  LCEC_PARAM_FLOAT_SET(hal_data->extenc_scale, 1.0);
  LCEC_PARAM_U32_SET(hal_data->fault_autoreset_cycles, DEASDA_FAULT_AUTORESET_CYCLES);
  LCEC_PARAM_U32_SET(hal_data->fault_autoreset_retries, DEASDA_FAULT_AUTORESET_RETRIES);
  hal_data->pos_scale_old = LCEC_PARAM_FLOAT_GET(hal_data->pos_scale) + 1.0;
  hal_data->pos_scale_rcpt = 1.0;

  // change based on FLAG_LOWRES_ENC/FLAG_HIGHRES_ENC
  if (flags & FLAG_LOWRES_ENC) {
    LCEC_PARAM_U32_SET(hal_data->pprev, DEASDA_PULSES_PER_REV_DEFLT_LOWRES);
    rtapi_print_msg(
        RTAPI_MSG_DBG, LCEC_MSG_PFX "Setting pprev to Low Res Encoder (1,280,000) for device %s.%s.\n", master->name, slave->name);
  } else if (flags & FLAG_HIGHRES_ENC) {
    LCEC_PARAM_U32_SET(hal_data->pprev, DEASDA_PULSES_PER_REV_DEFLT_HIGHRES);
    rtapi_print_msg(
        RTAPI_MSG_DBG, LCEC_MSG_PFX "Setting pprev to High Res Encoder (16,777,216) for device %s.%s.\n", master->name, slave->name);
  }

  // an E-Gear set via modParams defines the PUU per revolution; srv-pulses-per-rev can still be overridden in HAL
  if (egear_pprev != 0) {
    LCEC_PARAM_U32_SET(hal_data->pprev, (hal_u32_t)egear_pprev);
    rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "Setting pprev to %llu from E-Gear for device %s.%s.\n", (unsigned long long)egear_pprev,
        master->name, slave->name);
  }

  // TODO: Add additional registers here if avialalbe: e.g. DIDO based on servo type FLAG_SERVO_X2/FLAG_SERVO_X3

  hal_data->last_switch_on = 0;
  hal_data->internal_fault = 0;

  hal_data->last_status = 0;
  hal_data->last_pos_raw = 0;
  hal_data->opmode_disp = 0;
  hal_data->home_start_sent = 0;
  hal_data->home_seen_unhomed = 0;
  hal_data->home_start_cycles = 0;

  hal_data->fault_reset_retry = 0;
  hal_data->fault_reset_state = 0;
  hal_data->fault_reset_cycle = 0;

  return 0;
}

void lcec_deasda_check_scales(lcec_deasda_data_t *hal_data) {
  // check for change in scale value
  if (LCEC_PARAM_FLOAT_GET(hal_data->pos_scale) != hal_data->pos_scale_old) {
    // scale value has changed, test and update it
    if ((LCEC_PARAM_FLOAT_GET(hal_data->pos_scale) < 1e-20) && (LCEC_PARAM_FLOAT_GET(hal_data->pos_scale) > -1e-20)) LCEC_PARAM_FLOAT_SET(hal_data->pos_scale, 1.0);

    // save new scale to detect future changes
    hal_data->pos_scale_old = LCEC_PARAM_FLOAT_GET(hal_data->pos_scale);
    // we actually want the reciprocal
    hal_data->pos_scale_rcpt = 1.0 / LCEC_PARAM_FLOAT_GET(hal_data->pos_scale);
  }
}

static void lcec_deasda_read(lcec_slave_t *slave, long period) {
  lcec_deasda_data_t *hal_data = (lcec_deasda_data_t *)slave->hal_data;
  uint8_t *pd = lcec_slave_pd(slave);
  uint16_t status;
  uint32_t status_di;
  int32_t speed_raw;
  double rpm;
  uint32_t pos_cnt;
  // wait for slave to be operational
  if (!slave->state.operational) {
    LCEC_PIN_BIT_SET(hal_data->ready, 0);
    LCEC_PIN_BIT_SET(hal_data->switched_on, 0);
    LCEC_PIN_BIT_SET(hal_data->oper_enabled, 0);
    LCEC_PIN_BIT_SET(hal_data->fault, 1);
    LCEC_PIN_BIT_SET(hal_data->volt_enabled, 0);
    LCEC_PIN_BIT_SET(hal_data->quick_stoped, 0);
    LCEC_PIN_BIT_SET(hal_data->on_disabled, 0);
    LCEC_PIN_BIT_SET(hal_data->warning, 0);
    LCEC_PIN_BIT_SET(hal_data->remote, 0);
    LCEC_PIN_BIT_SET(hal_data->at_speed, 0);
    LCEC_PIN_BIT_SET(hal_data->limit_active, 0);
    LCEC_PIN_BIT_SET(hal_data->zero_speed, 0);
    return;
  }

  // check for change in scale value
  lcec_deasda_check_scales(hal_data);

  // read status word
  status = EC_READ_U16(&pd[hal_data->status_pdo_os]);
  hal_data->last_status = status;
  if (hal_data->homing_enabled) {
    hal_data->opmode_disp = (int8_t)EC_READ_U8(&pd[hal_data->opmode_disp_pdo_os]);
    LCEC_PIN_U32_SET(hal_data->operation_mode, (hal_u32_t)(uint8_t)hal_data->opmode_disp);
  }
  LCEC_PIN_BIT_SET(hal_data->ready, (status >> 0) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->switched_on, (status >> 1) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->oper_enabled, (status >> 2) & 0x01);
  hal_data->internal_fault = (status >> 3) & 0x01;
  LCEC_PIN_BIT_SET(hal_data->volt_enabled, (status >> 4) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->quick_stoped, !((status >> 5) & 0x01));
  LCEC_PIN_BIT_SET(hal_data->on_disabled, (status >> 6) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->warning, (status >> 7) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->remote, (status >> 9) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->at_speed, (status >> 10) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->limit_active, (status >> 11) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->zero_speed, (status >> 12) & 0x01);

  // clear pending fault reset if no fault
  if (!hal_data->internal_fault) hal_data->fault_reset_retry = 0;

  // generate gated fault
  if (hal_data->fault_reset_retry > 0) {
    if (hal_data->fault_reset_cycle < LCEC_PARAM_U32_GET(hal_data->fault_autoreset_cycles)) {
      hal_data->fault_reset_cycle++;
    } else {
      hal_data->fault_reset_cycle = 0;
      hal_data->fault_reset_state = !hal_data->fault_reset_state;
      if (hal_data->fault_reset_state) {
        hal_data->fault_reset_retry--;
      }
    }
    LCEC_PIN_BIT_SET(hal_data->fault, 0);
  } else {
    LCEC_PIN_BIT_SET(hal_data->fault, hal_data->internal_fault);
  }

  // read current speed
  speed_raw = EC_READ_S32(&pd[hal_data->currvel_pdo_os]);
  rpm = (double)speed_raw * DEASDA_RPM_FACTOR;
  LCEC_PIN_FLOAT_SET(hal_data->vel_fb_rpm, rpm);
  LCEC_PIN_FLOAT_SET(hal_data->vel_fb_rpm_abs, fabs(rpm));
  LCEC_PIN_FLOAT_SET(hal_data->vel_fb, rpm * DEASDA_RPM_DIV * LCEC_PARAM_FLOAT_GET(hal_data->pos_scale));

  // update raw position counter
  pos_cnt = EC_READ_U32(&pd[hal_data->currpos_pdo_os]);
  hal_data->last_pos_raw = (int32_t)pos_cnt;
  class_enc_update(&hal_data->enc, LCEC_PARAM_U32_GET(hal_data->pprev), LCEC_PARAM_FLOAT_GET(hal_data->pos_scale), pos_cnt, 0, 0);

  // During drive-internal homing LinuxCNC does not command the motion the drive
  // performs, so the real feedback would look like a following error.  Report the
  // command as feedback instead.  The encoder tracking above keeps running on the
  // real position; real feedback resumes in the cycle after srv-home is cleared,
  // which is when LinuxCNC latches its home offset.
  if (hal_data->homing_enabled && hal_data->home_fake_fb && LCEC_PIN_BIT_GET(hal_data->home)) {
    LCEC_PIN_FLOAT_SET(hal_data->enc.pos, LCEC_PIN_FLOAT_GET(hal_data->cmd_value));
  }

  // update external encoder counter
  pos_cnt = EC_READ_U32(&pd[hal_data->extenc_pdo_os]);
  class_enc_update(&hal_data->extenc, 1, LCEC_PARAM_FLOAT_GET(hal_data->extenc_scale), pos_cnt, 0, 0);

  // read current
  LCEC_PIN_FLOAT_SET(hal_data->torque, (double)EC_READ_S16(&pd[hal_data->torque_pdo_os]) * 0.1);

  // read DI status word
  status_di = EC_READ_U32(&pd[hal_data->divalue_pdo_os]);

  LCEC_PIN_BIT_SET(hal_data->neg_lim_switch, (status_di >> 0) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->pos_lim_switch, (status_di >> 1) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->home_switch, (status_di >> 2) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_1, (status_di >> 16) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_2, (status_di >> 17) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_3, (status_di >> 18) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_4, (status_di >> 19) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_5, (status_di >> 20) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_6, (status_di >> 21) & 0x01);
  LCEC_PIN_BIT_SET(hal_data->di_7, (status_di >> 22) & 0x01);
}

static void lcec_deasda_write_csv(lcec_slave_t *slave, long period) {
  lcec_deasda_data_t *hal_data = (lcec_deasda_data_t *)slave->hal_data;
  uint8_t *pd = lcec_slave_pd(slave);
  uint16_t control;
  double speed_raw;
  int switch_on_edge;

  // do digital outputs
  if (hal_data->dout) lcec_dout_write_all(slave, hal_data->dout);

  // check for enable edge
  switch_on_edge = LCEC_PIN_BIT_GET(hal_data->switch_on) && !hal_data->last_switch_on;
  hal_data->last_switch_on = LCEC_PIN_BIT_GET(hal_data->switch_on);

  // check for autoreset
  if (LCEC_PARAM_U32_GET(hal_data->fault_autoreset_retries) > 0 && LCEC_PARAM_U32_GET(hal_data->fault_autoreset_cycles) > 0 && switch_on_edge && hal_data->internal_fault) {
    hal_data->fault_reset_retry = LCEC_PARAM_U32_GET(hal_data->fault_autoreset_retries);
    hal_data->fault_reset_state = 1;
    hal_data->fault_reset_cycle = 0;
  }

  // check for change in scale value
  lcec_deasda_check_scales(hal_data);

  // write dev ctrl
  control = 0;

  if (LCEC_PIN_BIT_GET(hal_data->enable_volt)) control |= (1 << 1);
  if (!LCEC_PIN_BIT_GET(hal_data->quick_stop)) control |= (1 << 2);
  if (LCEC_PIN_BIT_GET(hal_data->fault_reset)) control |= (1 << 7);
  if (LCEC_PIN_BIT_GET(hal_data->halt)) control |= (1 << 8);

  if (hal_data->fault_reset_retry > 0) {
    if (hal_data->fault_reset_state) control |= (1 << 7);
  } else {
    if (LCEC_PIN_BIT_GET(hal_data->switch_on)) control |= (1 << 0);
    if (LCEC_PIN_BIT_GET(hal_data->enable) && LCEC_PIN_BIT_GET(hal_data->switched_on)) control |= (1 << 3);
  }
  EC_WRITE_U16(&pd[hal_data->control_pdo_os], control);

  // all of this is depeding on CSV/CSP
  // calculate rpm command
  LCEC_PIN_FLOAT_SET(hal_data->vel_rpm, LCEC_PIN_FLOAT_GET(hal_data->cmd_value) * hal_data->pos_scale_rcpt * DEASDA_RPM_MUL);

  // set RPM
  speed_raw = LCEC_PIN_FLOAT_GET(hal_data->vel_rpm) * DEASDA_RPM_RCPT;
  if (speed_raw > (double)0x7fffffff) speed_raw = (double)0x7fffffff;
  if (speed_raw < (double)-0x7fffffff) speed_raw = (double)-0x7fffffff;

  EC_WRITE_S32(&pd[hal_data->cmdvalue_pdo_os], (int32_t)speed_raw);
}

// Drive-internal homing (B3, CiA 402 homing mode), called every cycle from the
// CSP write function.  Sequence, as required by the B3 manual (12.3.4):
//   1. srv-home set -> request modes of operation 6 (homing) via 0x6060
//   2. wait until 0x6061 reports 6 and the drive is operation enabled
//   3. assert controlword bit 4 (rising edge starts homing)
//   4. statusword bit 12 (homing attained) + bit 10 (target reached) = done
//      (bit 13 = homing error)
//   5. clear srv-home, request mode 8 (CSP) again
// While the drive is not in CSP the position target follows the actual position,
// so the drive does not jump when CSP resumes.
// Returns the controlword bits to OR in; *hold_position is set while the CSP
// target must not be taken from srv-pos-cmd.
#define DEASDA_HOME_ACCEPT_CYCLES 20  // accept "attained" this many cycles after start even if never seen cleared

static uint16_t lcec_deasda_homing(lcec_slave_t *slave, lcec_deasda_data_t *hal_data, uint8_t *pd, int *hold_position) {
  uint16_t status = hal_data->last_status;
  uint16_t control = 0;
  int home = LCEC_PIN_BIT_GET(hal_data->home);
  int in_hm = (hal_data->opmode_disp == DEASDA_OPMODE_HM);
  int attained = in_hm && ((status >> 10) & 1) && ((status >> 12) & 1);
  int error = in_hm && ((status >> 13) & 1);

  // never keep a homing request across a fault or a lost bus
  if (home && (!slave->state.operational || hal_data->internal_fault)) {
    home = 0;
    LCEC_PIN_BIT_SET(hal_data->home, 0);
  }

  if (!home) {
    hal_data->home_seen_unhomed = 0;
    hal_data->home_start_cycles = 0;
  }

  // note whether the drive reported "not attained" after the start edge, so that
  // stale status bits can not complete a request that has not run yet
  if (home && hal_data->home_start_sent && !attained) hal_data->home_seen_unhomed = 1;

  if (home && attained && !error && (hal_data->home_seen_unhomed || hal_data->home_start_cycles >= DEASDA_HOME_ACCEPT_CYCLES)) {
    home = 0;
    LCEC_PIN_BIT_SET(hal_data->home, 0);
    hal_data->home_seen_unhomed = 0;
    hal_data->home_start_cycles = 0;
  }

  // request the mode first; assert the start edge only once the drive confirms
  // homing mode and is operation enabled
  hal_data->home_start_sent = 0;
  if (home && in_hm && ((status >> 2) & 1) && !error) {
    control |= (1 << 4);
    hal_data->home_start_sent = 1;
    if (hal_data->home_start_cycles < 0x7fffffff) hal_data->home_start_cycles++;
  }
  EC_WRITE_S8(&pd[hal_data->opmode_cmd_pdo_os], home ? DEASDA_OPMODE_HM : DEASDA_OPMODE_CSP);

  LCEC_PIN_BIT_SET(hal_data->homed, attained);
  LCEC_PIN_BIT_SET(hal_data->homing, hal_data->home_start_sent && !attained && !error);
  LCEC_PIN_BIT_SET(hal_data->home_error, error);

  *hold_position = home || hal_data->opmode_disp != DEASDA_OPMODE_CSP;
  return control;
}

static void lcec_deasda_write_csp(lcec_slave_t *slave, long period) {
  lcec_deasda_data_t *hal_data = (lcec_deasda_data_t *)slave->hal_data;
  uint8_t *pd = lcec_slave_pd(slave);
  uint16_t control;
  int32_t pos_puu;
  int switch_on_edge;
  int hold_position = 0;

  // do digital outputs
  if (hal_data->dout) lcec_dout_write_all(slave, hal_data->dout);

  // check for enable edge
  switch_on_edge = LCEC_PIN_BIT_GET(hal_data->switch_on) && !hal_data->last_switch_on;
  hal_data->last_switch_on = LCEC_PIN_BIT_GET(hal_data->switch_on);

  // check for autoreset
  if (LCEC_PARAM_U32_GET(hal_data->fault_autoreset_retries) > 0 && LCEC_PARAM_U32_GET(hal_data->fault_autoreset_cycles) > 0 && switch_on_edge && hal_data->internal_fault) {
    hal_data->fault_reset_retry = LCEC_PARAM_U32_GET(hal_data->fault_autoreset_retries);
    hal_data->fault_reset_state = 1;
    hal_data->fault_reset_cycle = 0;
  }

  // check for change in scale value
  lcec_deasda_check_scales(hal_data);

  // write dev ctrl
  control = 0;
  if (LCEC_PIN_BIT_GET(hal_data->enable_volt)) control |= (1 << 1);
  if (!LCEC_PIN_BIT_GET(hal_data->quick_stop)) control |= (1 << 2);
  if (LCEC_PIN_BIT_GET(hal_data->fault_reset)) control |= (1 << 7);
  if (LCEC_PIN_BIT_GET(hal_data->halt)) control |= (1 << 8);

  if (hal_data->fault_reset_retry > 0) {
    if (hal_data->fault_reset_state) control |= (1 << 7);
  } else {
    if (LCEC_PIN_BIT_GET(hal_data->switch_on)) control |= (1 << 0);
    if (LCEC_PIN_BIT_GET(hal_data->enable) && LCEC_PIN_BIT_GET(hal_data->switched_on)) control |= (1 << 3);
  }
  if (hal_data->homing_enabled) control |= lcec_deasda_homing(slave, hal_data, pd, &hold_position);
  EC_WRITE_U16(&pd[hal_data->control_pdo_os], control);

  // ASDA Drives expect target Position in PUU (Pulse per User Unit)
  // See https://www.deltaww.com/en-US/FAQ/228
  // Calculation accordingly based on pprev and pos_scale (i.e. pitch of ball screw)
  if (hold_position) {
    pos_puu = hal_data->last_pos_raw;  // homing: keep the target on the actual position
  } else {
    pos_puu = (int32_t)(LCEC_PIN_FLOAT_GET(hal_data->cmd_value) * LCEC_PARAM_U32_GET(hal_data->pprev) / LCEC_PARAM_FLOAT_GET(hal_data->pos_scale));
  }
  EC_WRITE_S32(&pd[hal_data->cmdvalue_pdo_os], pos_puu);
}

// Match the drive mode configuration in modparams and return the settings for that particular operational mode.
// the value is then used both for setting the mode and to differnetiate between CSV (0) and CSP
static const drive_operationmodes_t *drive_opmode(char *drivemode) {
  drive_operationmodes_t const *modes;

  for (modes = drive_operationmodes; modes != NULL; modes++) {
    if (!strcasecmp(drivemode, modes->name)) return modes;
  }

  return NULL;
}
