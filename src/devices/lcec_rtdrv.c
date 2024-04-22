//
//    Copyright (C) 2024 Scott Laird <scott@sigkill.org>
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
/// @brief Driver RTelligent DRV-E series DC servo drives

#include "../lcec.h"
#include "lcec_class_cia402.h"
#include "lcec_class_din.h"
#include "lcec_class_dout.h"

// Constants for modparams.  These should have their last hex digit be
// a 0, to make handling per-channel modparams easier.  Keep these
// under 0x1000 to avoid collisions with params from class cia402.
//
// XXXX: You can probaably remove most if not ell of these, if you
// know how many channels, etc your device has and just hard-code
// their values in the _init function.
#define M_INPUT1FUNC  0x00
#define M_INPUT2FUNC  0x10
#define M_INPUT3FUNC  0x20
#define M_INPUT4FUNC  0x30
#define M_INPUT5FUNC  0x40
#define M_INPUT6FUNC  0x50
#define M_INPUT7FUNC  0x60
#define M_INPUT8FUNC  0x80
#define M_OUTPUT1FUNC 0x90
#define M_OUTPUT2FUNC 0xa0
#define M_OUTPUT3FUNC 0xb0
#define M_OUTPUT4FUNC 0xc0

/// @brief Device-specific modparam settings available via XML.
static const lcec_modparam_desc_t modparams_perchannel[] = {
    // XXXX, add per-channel device-specific modparams here.
    {NULL},
};

#define INDESCR  "general|servo_enable|positive_limit|negative_limit|home|estop, see docs for others"
#define OUTDESCR "general|brake|alarm|servo_ready, see docs for others"

static const lcec_modparam_desc_t modparams_base[] = {
    {"input1Func", M_INPUT1FUNC, MODPARAM_TYPE_STRING, "servo_enable", "Function for IN1: " INDESCR},
    {"input2Func", M_INPUT2FUNC, MODPARAM_TYPE_STRING, "positive_limit", "Function for IN2: " INDESCR},
    {"input3Func", M_INPUT3FUNC, MODPARAM_TYPE_STRING, "negative_limit", "Function for IN3: " INDESCR},
    {"input4Func", M_INPUT4FUNC, MODPARAM_TYPE_STRING, "home", "Function for IN4: " INDESCR},
    {"input5Func", M_INPUT5FUNC, MODPARAM_TYPE_STRING, "general", "Function for IN5: " INDESCR},
    {"input6Func", M_INPUT6FUNC, MODPARAM_TYPE_STRING, "general", "Function for IN6: " INDESCR},
    {"input7Func", M_INPUT7FUNC, MODPARAM_TYPE_STRING, "general", "Function for IN7: " INDESCR},
    {"input8Func", M_INPUT8FUNC, MODPARAM_TYPE_STRING, "general", "Function for IN8: " INDESCR},
    {"output1Func", M_OUTPUT1FUNC, MODPARAM_TYPE_STRING, "alarm", "Function for OUT1: " OUTDESCR},
    {"output2Func", M_OUTPUT2FUNC, MODPARAM_TYPE_STRING, "return_to_origin_completed", "Function for OUT2: " OUTDESCR},
    {"output3Func", M_OUTPUT3FUNC, MODPARAM_TYPE_STRING, "brake", "Function for OUT3: " OUTDESCR},
    {"output4Func", M_OUTPUT4FUNC, MODPARAM_TYPE_STRING, "general", "Function for OUT4: " OUTDESCR},
    {NULL},
};

static const lcec_modparam_doc_t chan_docs[] = {
    // XXXX, add documentation for per-channel settings here
    {NULL},
};

static const lcec_modparam_doc_t base_docs[] = {
    // XXXX, add documentation for device-specific settings here
    {NULL},
};

// According to page 64 of my "DRV Series Servo EtherCAT User Manaual
// V1.0", these are all of the options for input configuration.  A
// number of these seem better-suited to "manual" use, where a config
// is loaded into the drive and triggered via specific input lines.
// But, just listing them here is cheap enough, I guess.
static const lcec_lookuptable_int_t rtdrv_inputfunc[] = {
    {"general", 0},  // Default for IN5, IN6, IN7, IN8.
    {"normal", 0},
    {"gpio", 0},
    {"servo_enable", 1},  // Default for IN1
    {"alarm_clear", 2},
    {"pulse_command_prohibition", 3},
    {"clear_position_deviation", 4},
    {"positive_limit", 5},  // Default for IN2
    {"negative_limit", 6},  // Default for IN3
    {"gain_switching", 7},
    {"egear_switching", 8},
    {"zero-speed_clamp", 9},
    {"control_mode_selection_1", 10},
    {"estop", 11},
    {"position_command_prohibition", 12},
    {"step_position_trigger", 13},
    {"multi_segment_run_command_switching_1", 14},
    {"multi_segment_run_command_switching_2", 15},
    {"multi_segment_run_command_switching_3", 16},
    {"multi_segment_run_command_switching_4", 17},
    {"torque_command_direction_setting", 18},
    {"speed_command_direction_setting", 19},
    {"position_command_direction_setting", 20},
    {"multi_segment_position_command_enable", 21},
    {"back_to_home_input", 22},
    {"home", 23},  // Default for IN4
    {"user1", 24},
    {"user2", 25},
    {"user3", 26},
    {"user4", 27},
    {"user5", 28},
    {"control_mode_selection_2", 29},
    {"probe1", 30},
    {"probe2", 31},
    {NULL},
};

static const lcec_lookuptable_int_t rtdrv_outputfunc[] = {
    {"brake", 0},  // Default for OUT3
    {"alarm", 1},  // Default for OUT1
    {"position_reached", 2},
    {"speed_reached", 3},
    {"servo_ready", 4},
    {"internal_position_command_stop", 5},
    {"return_to_origin_completed", 6},  // Default for OUT2
    {"user1", 7},
    {"user2", 8},
    {"user3", 9},
    {"user4", 10},
    {"user5", 11},
    {"torque_reached", 12},
    {"out_of_tolerance_output", 13},
    {"general", 31},  // Default for OUT4
    {"gpio", 31},
};

static int lcec_rtdrv_init(int comp_id, lcec_slave_t *slave);

// XXXX: macros like these are helpful if you're planning on
// supporting devices with varying numbers of axes and I/O ports in
// your device.  See lcec_leadshine_stepper.c and lece_rtec.c for
// examples of use.
//
//#define AXES(flags)  ((flags >> 60) & 0xf)
//#define DIN(flags) ((flags >> 56) & 0xf)
//#define DOUT(flags) ((flags >> 52) & 0xf)
//#define F_AXES(axes) ((uint64_t)axes << 60)
//#define F_DIN(din) ((uint64_t)din<<56)
//#define F_DOUT(dout) ((uint64_t)dout<<52)

// I'm guessing at the PID values for the 750E and 1500E.  RT tends to
// increment by one in this case, but I can't find actual
// documentation from them.
static lcec_typelist_t types[] = {
    {.name = "DRV400E", .vid = LCEC_RTELLIGENT_VID, .pid = 0x0a880042, .proc_init = lcec_rtdrv_init},
    {.name = "DRV750E", .vid = LCEC_RTELLIGENT_VID, .pid = 0x0a880043, .proc_init = lcec_rtdrv_init},
    {.name = "DRV1500E", .vid = LCEC_RTELLIGENT_VID, .pid = 0x0a880044, .proc_init = lcec_rtdrv_init},
    {NULL},
};
ADD_TYPES_WITH_CIA402_MODPARAMS(types, 1, modparams_perchannel, modparams_base, chan_docs, base_docs)

static void lcec_rtdrv_read(lcec_slave_t *slave, long period);
static void lcec_rtdrv_write(lcec_slave_t *slave, long period);

typedef struct {
  lcec_class_cia402_channels_t *cia402;
  // XXXX: Add pins and vars for PDO offsets here.
} lcec_rtdrv_data_t;

static const lcec_pindesc_t slave_pins[] = {
    // XXXX: add device-specific pins here.
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

static int set_inputfunc(lcec_slave_t *slave, lcec_slave_modparam_t *p, int port) {
  int val = lcec_lookupint_i(rtdrv_inputfunc, p->value.str, -1);
  if (val == -1) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "invalid value for <modparam name=\"%s\"> for slave %s.%s\n", p->name, slave->master->name,
        slave->name);
    return -1;
  }
  if (lcec_write_sdo16_modparam(slave, 0x2004, port * 2 + 1, val, p->name) < 0) return -1;
  return 0;
}

static int set_outputfunc(lcec_slave_t *slave, lcec_slave_modparam_t *p, int port) {
  int val = lcec_lookupint_i(rtdrv_outputfunc, p->value.str, -1);
  if (val == -1) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "invalid value for <modparam name=\"%s\"> for slave %s.%s\n", p->name, slave->master->name,
        slave->name);
    return -1;
  }
  if (lcec_write_sdo16_modparam(slave, 0x2005, port * 2 + 1, val, p->name) < 0) return -1;
  return 0;
}

static int handle_modparams(lcec_slave_t *slave, lcec_class_cia402_options_t *options) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;
  int v;

  for (p = slave->modparams; p != NULL && p->id >= 0; p++) {
    switch (p->id) {
        // XXXX: add device-specific modparam handlers here.
      case M_INPUT1FUNC:
        return set_inputfunc(slave, p, 0);
      case M_INPUT2FUNC:
        return set_inputfunc(slave, p, 1);
      case M_INPUT3FUNC:
        return set_inputfunc(slave, p, 2);
      case M_INPUT4FUNC:
        return set_inputfunc(slave, p, 3);
      case M_INPUT5FUNC:
        return set_inputfunc(slave, p, 4);
      case M_INPUT6FUNC:
        return set_inputfunc(slave, p, 5);
      case M_INPUT7FUNC:
        return set_inputfunc(slave, p, 6);
      case M_INPUT8FUNC:
        return set_inputfunc(slave, p, 7);
      case M_OUTPUT1FUNC:
        return set_outputfunc(slave, p, 0);
      case M_OUTPUT2FUNC:
        return set_outputfunc(slave, p, 1);
      case M_OUTPUT3FUNC:
        return set_outputfunc(slave, p, 2);
      case M_OUTPUT4FUNC:
        return set_outputfunc(slave, p, 3);
      default:
        // Handle cia402 generic modparams
        v = lcec_cia402_handle_modparam(slave, p, options);

        // If an error occured, then return the error.
        if (v < 0) {
          return v;
        }

        // if nothing handled this modparam, then something's wrong.  Return an error:
        if (v > 0) {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "unknown modparam %s for slave %s.%s\n", p->name, master->name, slave->name);
          return -1;
        }
        break;
    }
  }

  return 0;
}

static int lcec_rtdrv_init(int comp_id, lcec_slave_t *slave) {
  lcec_rtdrv_data_t *hal_data;
  int err;

  // alloc hal memory
  hal_data = LCEC_HAL_ALLOCATE(lcec_rtdrv_data_t);
  slave->hal_data = hal_data;

  // initialize read/write
  slave->proc_read = lcec_rtdrv_read;
  slave->proc_write = lcec_rtdrv_write;

  // Apply default Distributed Clock settings if it's not already set.
  if (slave->dc_conf == NULL) {
    lcec_slave_dc_t *dc = LCEC_HAL_ALLOCATE(lcec_slave_dc_t);
    dc->assignActivate = 0x300;  // I'm guessing here, because I don't have an ESI, but all other RT devices use 0x300.
    dc->sync0Cycle = slave->master->app_time_period;

    slave->dc_conf = dc;
  }

  lcec_class_cia402_options_t *options = lcec_cia402_options();

  options->channels = 1;
  options->rxpdolimit = 12;
  options->txpdolimit = 12;

  for (int channel = 0; channel < options->channels; channel++) {
    options->channel[channel]->digital_in_channels = 8;
    options->channel[channel]->digital_out_channels = 4;
    options->channel[channel]->enable_pv = 1;
    options->channel[channel]->enable_pp = 1;
    options->channel[channel]->enable_csv = 1;
    options->channel[channel]->enable_csp = 1;
    options->channel[channel]->enable_cst = 1;
    options->channel[channel]->enable_actual_following_error = 1;
    options->channel[channel]->enable_actual_torque = 1;
    options->channel[channel]->enable_digital_input = 1;
    options->channel[channel]->enable_digital_output = 1;
    options->channel[channel]->enable_error_code = 1;
    options->channel[channel]->enable_following_error_timeout = 1;
    options->channel[channel]->enable_following_error_window = 1;
    options->channel[channel]->enable_home_accel = 1;
    options->channel[channel]->enable_maximum_current = 1;
    options->channel[channel]->enable_maximum_torque = 1;
    options->channel[channel]->enable_polarity = 1;
    options->channel[channel]->enable_position_demand = 1;
    options->channel[channel]->enable_positioning_time = 1;
    options->channel[channel]->enable_positioning_window = 1;
    options->channel[channel]->enable_probe_status = 1;
    options->channel[channel]->enable_profile_accel = 1;
    options->channel[channel]->enable_profile_decel = 1;
    options->channel[channel]->enable_profile_max_velocity = 1;
    options->channel[channel]->enable_profile_velocity = 1;
    options->channel[channel]->enable_target_torque = 1;
    options->channel[channel]->enable_torque_demand = 1;
    options->channel[channel]->enable_torque_slope = 1;
    options->channel[channel]->enable_velocity_error_time = 1;
    options->channel[channel]->enable_velocity_error_window = 1;
  }

  // Handle modparams
  if (handle_modparams(slave, options) != 0) {
    return -EIO;
  }

  if (options->channels > 1) {
    lcec_cia402_rename_multiaxis_channels(options);
  }

  lcec_syncs_t *syncs = lcec_cia402_init_sync(slave, options);
  lcec_cia402_add_output_sync(slave, syncs, options);
  lcec_cia402_add_input_sync(slave, syncs, options);

  slave->sync_info = &syncs->syncs[0];

  hal_data->cia402 = lcec_cia402_allocate_channels(options->channels);

  for (int channel = 0; channel < options->channels; channel++) {
    hal_data->cia402->channels[channel] = lcec_cia402_register_channel(slave, 0x6000 + 0x800 * channel, options->channel[channel]);
  }

  // export device-specific pins.  This shouldn't need edited, just edit `slave_pins` above.
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, slave->master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

static void lcec_rtdrv_read(lcec_slave_t *slave, long period) {
  lcec_rtdrv_data_t *hal_data = (lcec_rtdrv_data_t *)slave->hal_data;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  lcec_cia402_read_all(slave, hal_data->cia402);
}

static void lcec_rtdrv_write(lcec_slave_t *slave, long period) {
  lcec_rtdrv_data_t *hal_data = (lcec_rtdrv_data_t *)slave->hal_data;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  lcec_cia402_write_all(slave, hal_data->cia402);
}
