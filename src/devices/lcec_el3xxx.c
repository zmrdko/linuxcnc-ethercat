//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
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
/// @brief Driver for Beckhoff EL3xxx Analog input modules

#include "../lcec.h"

// TODO(scottlaird): add support for additional EL31xx operating modes, as per
//   https://download.beckhoff.com/download/Document/io/ethercat-terminals/el31xxen.pdf#page=251
//   Specifically, support disabling the filter (0x8000:06) and DC mode.
//
// TODO(scottlaird): using generic support, add EL3255 pot input.
//
// TODO(scottlaird): Figure out what to do with older EL31xx devices
//   without 0x6000:e (the sync error PDO).  It looks like it was
//   added in r18.  Is there a point in keeping the sync error pin at all?

#define LCEC_EL3XXX_MODPARAM_SENSOR     0
#define LCEC_EL3XXX_MODPARAM_RESOLUTION 8
#define LCEC_EL3XXX_MODPARAM_WIRES      16

#define LCEC_EL3XXX_MAXCHANS 8  // for sizing arrays

static int lcec_el3xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

/// @brief Modparams settings available via XML.
static const lcec_modparam_desc_t modparams_temperature[] = {
    {"ch0Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 0, MODPARAM_TYPE_STRING},
    {"ch1Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 1, MODPARAM_TYPE_STRING},
    {"ch2Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 2, MODPARAM_TYPE_STRING},
    {"ch3Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 3, MODPARAM_TYPE_STRING},
    {"ch4Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 4, MODPARAM_TYPE_STRING},
    {"ch5Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 5, MODPARAM_TYPE_STRING},
    {"ch6Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 6, MODPARAM_TYPE_STRING},
    {"ch7Sensor", LCEC_EL3XXX_MODPARAM_SENSOR + 7, MODPARAM_TYPE_STRING},
    {"ch0Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 0, MODPARAM_TYPE_STRING},
    {"ch1Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 1, MODPARAM_TYPE_STRING},
    {"ch2Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 2, MODPARAM_TYPE_STRING},
    {"ch3Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 3, MODPARAM_TYPE_STRING},
    {"ch4Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 4, MODPARAM_TYPE_STRING},
    {"ch5Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 5, MODPARAM_TYPE_STRING},
    {"ch6Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 6, MODPARAM_TYPE_STRING},
    {"ch7Resolution", LCEC_EL3XXX_MODPARAM_RESOLUTION + 7, MODPARAM_TYPE_STRING},
    {"ch0Wires", LCEC_EL3XXX_MODPARAM_WIRES + 0, MODPARAM_TYPE_STRING},
    {"ch1Wires", LCEC_EL3XXX_MODPARAM_WIRES + 1, MODPARAM_TYPE_STRING},
    {"ch2Wires", LCEC_EL3XXX_MODPARAM_WIRES + 2, MODPARAM_TYPE_STRING},
    {"ch3Wires", LCEC_EL3XXX_MODPARAM_WIRES + 3, MODPARAM_TYPE_STRING},
    {"ch4Wires", LCEC_EL3XXX_MODPARAM_WIRES + 4, MODPARAM_TYPE_STRING},
    {"ch5Wires", LCEC_EL3XXX_MODPARAM_WIRES + 5, MODPARAM_TYPE_STRING},
    {"ch6Wires", LCEC_EL3XXX_MODPARAM_WIRES + 6, MODPARAM_TYPE_STRING},
    {"ch7Wires", LCEC_EL3XXX_MODPARAM_WIRES + 7, MODPARAM_TYPE_STRING},
    {NULL},
};

/// @brief Sensor types available for `<modParams name="sensor">`
typedef struct {
  char *name;       ///< Sensor type name
  uint16_t value;   ///< Which value needs to be set in 0x80x0:19 to enable this sensor
  int is_unsigned;  ///< Ohm measurements are unsigned, and need the extra bit to cover the documented range.
  double scale;     ///< Default conversion factor for this sensor type
} temp_sensor_t;

/// @brief List of known temperature sensor types.
///
/// From https://download.beckhoff.com/download/Document/io/ethercat-terminals/el32xxen.pdf#page=223
static const temp_sensor_t temp_sensors[] = {
    {"Pt100", 0, 0, 0.1},          // Pt100 sensor,
    {"Ni100", 1, 0, 0.1},          // Ni100 sensor, -60 to 250C
    {"Pt1000", 2, 0, 0.1},         // Pt1000 sensor, -200 to 850C
    {"Pt500", 3, 0, 0.1},          // Pt500 sensor, -200 to 850C
    {"Pt200", 4, 0, 0.1},          // Pt200 sensor, -200 to 850C
    {"Ni1000", 5, 0, 0.1},         // Ni1000 sensor, -60 to 250C
    {"Ni1000-TK5000", 6, 0, 0.1},  // Ni1000-TK5000, -30 to 160C
    {"Ni120", 7, 0, 0.1},          // Ni120 sensor, -60 to 320C
    {"Ohm/16", 8, 1, 1.0 / 16},    // no sensor, report Ohms directly.  0-4095 Ohms
    {"Ohm/64", 9, 1, 1.0 / 64},    // no sensor, report Ohms directly.  0-1023 Ohms
    {NULL},
};

/// @brief Resolutions available for `<modParams name="resolution"/>`
typedef struct {
  char *name;               ///< The name of the `resolution` modParam value, as found in `ethercat.xml`.
  uint16_t value;           ///< The value for this `resolution` setting that needs to be set in hardware.
  double scale_multiplier;  ///< The amount that the `scale` needs to be adjusted when this resolution is selected.
} temp_resolution_t;

/// @brief List of available values for the `resolutions` modParam setting.
///
/// From https://download.beckhoff.com/download/Document/io/ethercat-terminals/el32xxen.pdf#page=222
static const temp_resolution_t temp_resolutions[] = {
    {"Signed", 0, 1.0},    // 0.1C per bit, default on most devices
    {"Standard", 0, 1.0},  // Same as "signed", but easier to remember WRT "High".
                           // { "Absolute", 1, 1.0 }, // ones-compliment presentation, why?
    {"High", 2, 0.1},      // 0.01C per bit, default on "high precision" devices.
    {NULL},
};

/// @brief Wire settings available for `<modParams name="wires" value="..."/>`
typedef struct {
  char *name;      ///< Name of the 'wires' modParam value, as found in `ethercat.xml`.
  uint16_t value;  ///< The value for this `wires` setting that needs to be set in hardware.
} temp_wires_t;

/// @brief List of available values for the `wires` modParam setting.
static const temp_wires_t temp_wires[] = {
    {"2", 0},
    {"3", 1},
    {"4", 2},
    {NULL},
};

/// Flags for describing devices
#define F_CHANNELS(x) (x)      ///< Number of input channels
#define F_SYNC        1 << 14  ///< Device has `sync-error` PDO
#define F_TEMPERATURE 1 << 15  ///< Device is a temperature sensor
#define F_PRESSURE    1 << 16  ///< Device is a pressure sensor

#define INPORTS(flag) ((flag)&0xf)  // Number of input channels
#define PDOS(flag)    (((flag)&F_SYNC) ? (5 * INPORTS(flag)) : (4 * INPORTS(flag)))

/// Macro to avoid repeating all of the unchanging fields in
/// `lcec_typelist_t`.  Calculates the `pdo_count` based on total port
/// count and port types.
#define BECKHOFF_AIN_DEVICE(name, pid, flags) \
  { name, LCEC_BECKHOFF_VID, pid, PDOS(flags), 0, NULL, lcec_el3xxx_init, NULL, flags }

/// Macro for defining devices that take `<modParam>`s in the XML config.
#define BECKHOFF_AIN_DEVICE_PARAMS(name, pid, flags, modparams) \
  { name, LCEC_BECKHOFF_VID, pid, PDOS(flags), 0, NULL, lcec_el3xxx_init, modparams, flags }

/// @brief Devices supported by this driver.
static lcec_typelist_t types[] = {
    // 12-bit devices
    BECKHOFF_AIN_DEVICE("EL3001", 0x0bb93052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3002", 0x0bba3052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3004", 0x0bbc3052, F_CHANNELS(4)),
    BECKHOFF_AIN_DEVICE("EL3008", 0x0bc03052, F_CHANNELS(8)),
    BECKHOFF_AIN_DEVICE("EL3011", 0x0bc33052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3012", 0x0bc43052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3014", 0x0bc63052, F_CHANNELS(3)),
    BECKHOFF_AIN_DEVICE("EL3021", 0x0bcd3052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3022", 0x0bce3052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3024", 0x0bd03052, F_CHANNELS(4)),
    BECKHOFF_AIN_DEVICE("EL3041", 0x0be13052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3042", 0x0be23052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3044", 0x0be43052, F_CHANNELS(4)),
    BECKHOFF_AIN_DEVICE("EL3048", 0x0be83052, F_CHANNELS(8)),
    BECKHOFF_AIN_DEVICE("EL3051", 0x0beb3052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3052", 0x0bec3052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3054", 0x0bee3052, F_CHANNELS(4)),
    BECKHOFF_AIN_DEVICE("EL3058", 0x0bf23052, F_CHANNELS(8)),
    BECKHOFF_AIN_DEVICE("EL3061", 0x0bf53052, F_CHANNELS(1)),
    BECKHOFF_AIN_DEVICE("EL3062", 0x0bf63052, F_CHANNELS(2)),
    BECKHOFF_AIN_DEVICE("EL3064", 0x0bf83052, F_CHANNELS(4)),
    BECKHOFF_AIN_DEVICE("EL3068", 0x0bfc3052, F_CHANNELS(8)),
    BECKHOFF_AIN_DEVICE("EJ3004", 0x0bbc2852, F_CHANNELS(4)),

    // 16-bit devices.  These include a `sync-err` PDO that 12-bit devices lack.
    BECKHOFF_AIN_DEVICE("EL3101", 0x0c1d3052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3102", 0x0c1e3052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3104", 0x0c203052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3111", 0x0c273052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3112", 0x0c283052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3114", 0x0c2a3052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3121", 0x0c313052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3122", 0x0c323052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3124", 0x0c343052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3141", 0x0c453052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3142", 0x0c463052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3144", 0x0c483052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3151", 0x0c4f3052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3152", 0x0c503052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3154", 0x0c523052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3161", 0x0c593052, F_CHANNELS(1) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3162", 0x0c5a3052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3164", 0x0c5c3052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EL3182", 0x0c6e3052, F_CHANNELS(2) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EP3174", 0x0c664052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EP3184", 0x0c704052, F_CHANNELS(4) | F_SYNC),
    BECKHOFF_AIN_DEVICE("EPX3158", 0x9809ab69, F_CHANNELS(8) | F_SYNC),

    // Temperature devices.  They need to include `modparams_temperature` to allow for sensor-type settings.
    BECKHOFF_AIN_DEVICE_PARAMS("EJ3202", 0x0c822852, F_CHANNELS(2) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EJ3214", 0x0c8e2852, F_CHANNELS(4) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3201", 0x0c813052, F_CHANNELS(1) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3202", 0x0c823052, F_CHANNELS(2) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3204", 0x0c843052, F_CHANNELS(4) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3208", 0x0c883052, F_CHANNELS(8) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3214", 0x0c8e3052, F_CHANNELS(4) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EL3218", 0x0c923052, F_CHANNELS(8) | F_TEMPERATURE, modparams_temperature),
    BECKHOFF_AIN_DEVICE_PARAMS("EP3204", 0x0c844052, F_CHANNELS(4) | F_TEMPERATURE, modparams_temperature),

    // Pressure sensors.
    BECKHOFF_AIN_DEVICE("EM3701", 0x0e753452, F_CHANNELS(1) | F_PRESSURE),
    BECKHOFF_AIN_DEVICE("EM3702", 0x0e763452, F_CHANNELS(2) | F_PRESSURE),
    BECKHOFF_AIN_DEVICE("EM3712", 0x0e803452, F_CHANNELS(2) | F_PRESSURE),
    {NULL},
};
ADD_TYPES(types)

/// @brief Data for a single analog channel.
typedef struct {
  hal_bit_t *overrange;   ///< Device reading is over-range.
  hal_bit_t *underrange;  ///< Device reading is under-range.
  hal_bit_t *error;       ///< Device is in an error state.
  hal_bit_t *sync_err;    ///< Device has a sync error.
  hal_s32_t *raw_val;     ///< The raw value read from the device.
  hal_float_t *scale;     ///< The scale used to convert `raw_val` into `val`.
  hal_float_t *bias;      ///< The offset used to convert `raw_val` into `val`.
  hal_float_t *val;       ///< The final result returned to LinuxCNC.
  unsigned int ovr_pdo_os;
  unsigned int ovr_pdo_bp;
  unsigned int udr_pdo_os;
  unsigned int udr_pdo_bp;
  unsigned int error_pdo_os;
  unsigned int error_pdo_bp;
  unsigned int sync_err_pdo_os;
  unsigned int sync_err_pdo_bp;
  unsigned int val_pdo_os;
  unsigned int is_unsigned;  ///< Sensor type is unsigned, such as resistance or temperature sensors.
} lcec_el3xxx_chan_t;

/// @brief List of HAL pins for analog devices with sync support.
static const lcec_pindesc_t slave_pins_sync[] = {
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, error), "%s.%s.%s.ain-%d-error"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, sync_err), "%s.%s.%s.ain-%d-sync-err"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, overrange), "%s.%s.%s.ain-%d-overrange"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, underrange), "%s.%s.%s.ain-%d-underrange"},
    {HAL_S32, HAL_OUT, offsetof(lcec_el3xxx_chan_t, raw_val), "%s.%s.%s.ain-%d-raw"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, val), "%s.%s.%s.ain-%d-val"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, scale), "%s.%s.%s.ain-%d-scale"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, bias), "%s.%s.%s.ain-%d-bias"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

/// @brief List of HAL pins for analog devices without sync support.
static const lcec_pindesc_t slave_pins_nosync[] = {
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, error), "%s.%s.%s.ain-%d-error"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, overrange), "%s.%s.%s.ain-%d-overrange"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, underrange), "%s.%s.%s.ain-%d-underrange"},
    {HAL_S32, HAL_OUT, offsetof(lcec_el3xxx_chan_t, raw_val), "%s.%s.%s.ain-%d-raw"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, val), "%s.%s.%s.ain-%d-val"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, scale), "%s.%s.%s.ain-%d-scale"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, bias), "%s.%s.%s.ain-%d-bias"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

/// @brief List of HAL pins for temperature sensors.
static const lcec_pindesc_t slave_pins_temperature[] = {
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, error), "%s.%s.%s.temp-%d-error"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, overrange), "%s.%s.%s.temp-%d-overrange"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, underrange), "%s.%s.%s.temp-%d-underrange"},
    {HAL_S32, HAL_OUT, offsetof(lcec_el3xxx_chan_t, raw_val), "%s.%s.%s.temp-%d-raw"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, val), "%s.%s.%s.temp-%d-temperature"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, scale), "%s.%s.%s.temp-%d-scale"},  // deleteme
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

/// @brief List of HAL pins for pressure sensors.
static const lcec_pindesc_t slave_pins_pressure[] = {
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, error), "%s.%s.%s.press-%d-error"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, overrange), "%s.%s.%s.press-%d-overrange"},
    {HAL_BIT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, underrange), "%s.%s.%s.press-%d-underrange"},
    {HAL_S32, HAL_OUT, offsetof(lcec_el3xxx_chan_t, raw_val), "%s.%s.%s.press-%d-raw"},
    {HAL_FLOAT, HAL_OUT, offsetof(lcec_el3xxx_chan_t, val), "%s.%s.%s.press-%d-pressure"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, scale), "%s.%s.%s.press-%d-scale"},
    {HAL_FLOAT, HAL_IO, offsetof(lcec_el3xxx_chan_t, bias), "%s.%s.%s.press-%d-bias"},
    {HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL},
};

/// @brief Data for an analog input device.
typedef struct {
  uint32_t channels;                               ///< The number of channels this device supports.
  lcec_el3xxx_chan_t chans[LCEC_EL3XXX_MAXCHANS];  ///< Data for each channel.
} lcec_el3xxx_data_t;

static void lcec_el3xxx_read_temp(struct lcec_slave *slave, long period);
static void lcec_el3xxx_read(struct lcec_slave *slave, long period);
static const temp_sensor_t *sensor_type(char *sensortype);
static const temp_resolution_t *sensor_resolution(char *sensorresolution);
static const temp_wires_t *sensor_wires(char *sensorwires);

/// @brief Initialize an EL3xxx device.
static int lcec_el3xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el3xxx_data_t *hal_data;
  lcec_el3xxx_chan_t *chan;
  int i;
  int err;
  const lcec_pindesc_t *slave_pins;
  uint64_t flags;

  flags = slave->flags;

  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "initing device as %s, flags %lx\n", slave->name, flags);
  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "- slave is %p\n", slave);
  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "- pdo_entry_regs is %p\n", pdo_entry_regs);

  // initialize settings that vary per bitdepth
  if (flags & F_TEMPERATURE) {
    slave->proc_read = lcec_el3xxx_read_temp;
  } else {
    slave->proc_read = lcec_el3xxx_read;
  }

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el3xxx_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el3xxx_data_t));
  slave->hal_data = hal_data;
  hal_data->channels = INPORTS(slave->flags);

  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "- setting up pins\n");

  // initialize pins
  for (i = 0; i < hal_data->channels; i++) {
    chan = &hal_data->chans[i];
    rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "- setting up channel %d (%p)\n", i, chan);

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->udr_pdo_os, &chan->udr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->ovr_pdo_os, &chan->ovr_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x07, &chan->error_pdo_os, &chan->error_pdo_bp);
    if (flags & F_SYNC) {
      // Only EL31xx devices have this PDO, if we try to initialize it
      // with 30xxs, then we get a PDO error and fail out.
      LCEC_PDO_INIT(
          pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0E, &chan->sync_err_pdo_os, &chan->sync_err_pdo_bp);
    }
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->val_pdo_os, NULL);

    // export pins
    if (flags & F_TEMPERATURE) {
      slave_pins = slave_pins_temperature;
    } else if (flags & F_PRESSURE) {
      slave_pins = slave_pins_pressure;
    } else if (flags & F_SYNC)
      slave_pins = slave_pins_sync;
    else
      slave_pins = slave_pins_nosync;

    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "lcec_pin_newf_list() for slave %s.%s failed: %d\n", master->name, slave->name, err);
      return err;
    }

    // initialize scale
    if (flags & F_TEMPERATURE) {
      *(chan->scale) = 0.1;
    } else {
      *(chan->scale) = 1.0;
    }

    chan->is_unsigned = 0;

    // Handle modparams
    if (flags & F_TEMPERATURE) {
      LCEC_CONF_MODPARAM_VAL_T *pval;

      // Handle <modParam> entries from the XML.

      // <modParam name="chXSensor" value="???"/>
      rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "  - checking modparam sensor for %s %d\n", slave->name, i);
      pval = lcec_modparam_get(slave, LCEC_EL3XXX_MODPARAM_SENSOR + i);
      if (pval != NULL) {
        rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - found sensor param\n");
        temp_sensor_t const *sensor;

        sensor = sensor_type(pval->str);
        if (sensor != NULL) {
          rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - setting sensor for %s %d to %d\n", slave->name, i, sensor->value);
          *(chan->scale) = sensor->scale;
          chan->is_unsigned = sensor->is_unsigned;

          if (lcec_write_sdo16(slave, 0x8000 + (i << 4), 0x19, sensor->value) != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to configure slave %s.%s sdo sensor!\n", master->name, slave->name);
            return -1;
          }
        } else {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "unknown sensor type \"%s\" for slave %s.%s channel %d!\n", pval->str, master->name,
              slave->name, i);
          return -1;
        }
      }

      // <modParam name="chXResolution" value="???"/>
      rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "  - checking modparam resolution for %s %d\n", slave->name, i);
      pval = lcec_modparam_get(slave, LCEC_EL3XXX_MODPARAM_RESOLUTION + i);
      if (pval != NULL) {
        rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - found resolution param\n");
        temp_resolution_t const *resolution;

        resolution = sensor_resolution(pval->str);
        if (resolution != NULL) {
          rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "    - setting resolution for %s %d to %d\n", slave->name, i, resolution->value);
          *(chan->scale) = *(chan->scale) * resolution->scale_multiplier;

          if (lcec_write_sdo8(slave, 0x8000 + (i << 4), 0x2, resolution->value) != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to configure slave %s.%s sdo resolution!\n", master->name, slave->name);
            return -1;
          }
        } else {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "unknown resolution \"%s\" for slave %s.%s channel %d!\n", pval->str, master->name,
              slave->name, i);
          return -1;
        }
      }

      // <modParam name="chXWires", value="???"/>
      rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "   - checking modparam wires for %s %d\n", slave->name, i);
      pval = lcec_modparam_get(slave, LCEC_EL3XXX_MODPARAM_WIRES + i);
      if (pval != NULL) {
        rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "     - found wires param\n");
        temp_wires_t const *wires;

        wires = sensor_wires(pval->str);
        if (wires != NULL) {
          rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "      - setting wires for %s %d to %d\n", slave->name, i, wires->value);

          if (lcec_write_sdo16(slave, 0x8000 + (i << 4), 0x1a, wires->value) != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "failed to configure slave %s.%s sdo wires!\n", master->name, slave->name);
            return -1;
          }
        } else {
          rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "unknown wire setting \"%s\" for slave %s.%s channel %d!\n", pval->str, master->name,
              slave->name, i);
          return -1;
        }
      }
    }
  }
  rtapi_print_msg(RTAPI_MSG_DBG, LCEC_MSG_PFX "done\n");

  return 0;
}

/// @brief Read values from the device.
static void lcec_el3xxx_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el3xxx_data_t *hal_data = (lcec_el3xxx_data_t *)slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el3xxx_chan_t *chan;
  int16_t value;
  int mask = 0x7fff;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i = 0; i < hal_data->channels; i++) {
    chan = &hal_data->chans[i];

    // update state
    // update state
    *(chan->overrange) = EC_READ_BIT(&pd[chan->ovr_pdo_os], chan->ovr_pdo_bp);
    *(chan->underrange) = EC_READ_BIT(&pd[chan->udr_pdo_os], chan->udr_pdo_bp);
    *(chan->error) = EC_READ_BIT(&pd[chan->error_pdo_os], chan->error_pdo_bp);
    if (slave->flags & F_SYNC) *(chan->sync_err) = EC_READ_BIT(&pd[chan->sync_err_pdo_os], chan->sync_err_pdo_bp);

    // update value
    value = EC_READ_S16(&pd[chan->val_pdo_os]) & mask;
    *(chan->raw_val) = value;
    *(chan->val) = *(chan->bias) + *(chan->scale) * (double)value * ((double)1 / (double)mask);
  }
}

/// @brief Read data from a 16-bit analog temperature sensor.
///
/// This applies different transform logic to return a temperature rather than a generic floating point value.
static void lcec_el3xxx_read_temp(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el3xxx_data_t *hal_data = (lcec_el3xxx_data_t *)slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el3xxx_chan_t *chan;
  int32_t value;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i = 0; i < hal_data->channels; i++) {
    chan = &hal_data->chans[i];

    // update state
    // update state
    *(chan->overrange) = EC_READ_BIT(&pd[chan->ovr_pdo_os], chan->ovr_pdo_bp);
    *(chan->underrange) = EC_READ_BIT(&pd[chan->udr_pdo_os], chan->udr_pdo_bp);
    *(chan->error) = EC_READ_BIT(&pd[chan->error_pdo_os], chan->error_pdo_bp);

    // update value
    if (chan->is_unsigned) {
      value = EC_READ_U16(&pd[chan->val_pdo_os]);
    } else {
      value = EC_READ_S16(&pd[chan->val_pdo_os]);
    }
    *(chan->raw_val) = value;
    *(chan->val) = (double)value * *(chan->scale);
  }
}

/// @brief Match the sensor_type in modparams and return the definition
/// associated with that sensor.
///
/// From https://download.beckhoff.com/download/Document/io/ethercat-terminals/el32xxen.pdf#page=223
static const temp_sensor_t *sensor_type(char *sensortype) {
  temp_sensor_t const *type;

  for (type = temp_sensors; type != NULL; type++) {
    if (!strcasecmp(sensortype, type->name)) {
      return type;
    }
  }

  return NULL;
}

/// @brief Match the sensor resolutiuon in modparams and return the settings for that resolution.
static const temp_resolution_t *sensor_resolution(char *sensorresolution) {
  temp_resolution_t const *res;

  for (res = temp_resolutions; res != NULL; res++) {
    if (!strcasecmp(sensorresolution, res->name)) {
      return res;
    }
  }

  return NULL;
}

/// @brief Match the sensor wire configuration in modparams and return the settings for that number of wires.
static const temp_wires_t *sensor_wires(char *sensorwires) {
  temp_wires_t const *wires;

  for (wires = temp_wires; wires != NULL; wires++) {
    if (!strcasecmp(sensorwires, wires->name)) {
      return wires;
    }
  }

  return NULL;
}
