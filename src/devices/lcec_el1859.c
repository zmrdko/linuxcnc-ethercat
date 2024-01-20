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
/// @brief Driver for Beckhoff EL1859 and related digital input/output modules

#include "lcec_el1859.h"

#include "../lcec.h"
#include "lcec_class_din.h"
#include "lcec_class_dout.h"

// This driver should support any Beckhoff digital in/out board with
// equal numbers of in and out ports, with input PDOs on 0x60n0:01 and
// output PDOs on 0x70n0:01.  This covers most devices in Beckhoff's
// catalog, but not all.  Some (like the EP2316-0008) have PDOs on
// 0x6000:0n and 0x7000:0n instead; these will need a different
// driver.

static int lcec_el1859_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs);

#define F_IN4       4
#define F_IN8       8
#define F_IN16      16
#define F_OUT4      4 * 32
#define F_OUT8      8 * 32
#define F_OUT16     16 * 32
#define F_DENSEPDOS 1 << 10  // PDOs are 0x6000:01, 0x6000:02, ... instead of 0x6000:01, 0x6010:01
#define F_OUTOFFSET 1 << 11  // Out PDOs start at 0x7080 instead of 0x7000

#define INPORTS(flag)  ((flag)&31)           // Number of input channels
#define OUTPORTS(flag) (((flag) >> 5) & 31)  // Number of output channels

/// Macro to avoid repeating all of the unchanging fields in
/// `lcec_typelist_t`.  Calculates the `pdo_count` based on total port
/// count.
#define TYPE(name, pid, flags) \
  { name, LCEC_BECKHOFF_VID, pid, INPORTS(flags) + OUTPORTS(flags), 0, NULL, lcec_el1859_init, NULL, flags }

static lcec_typelist_t types[] = {
    TYPE("EL1852", 0x73c3052, F_IN8 | F_OUT8 | F_OUTOFFSET),
    TYPE("EL1859", 0x07433052, F_IN8 | F_OUT8 | F_OUTOFFSET),
    TYPE("EJ1859", 0x07432852, F_IN8 | F_OUT8 | F_OUTOFFSET),
    TYPE("EK1814", 0x07162c52, F_IN4 | F_OUT4),
    TYPE("EP2308", 0x09044052, F_IN4 | F_OUT4),
    TYPE("EP2318", 0x090E4052, F_IN4 | F_OUT4),
    TYPE("EP2328", 0x09184052, F_IN4 | F_OUT4),
    TYPE("EP2338", 0x09224052, F_IN8 | F_OUT8),
    TYPE("EP2339", 0x09234052, F_IN16 | F_OUT16),
    TYPE("EP2349", 0x092d4052, F_IN16 | F_OUT16),
    TYPE("EQ2339", 0x092d4052, F_IN16 | F_OUT16),
    TYPE("EPP2308", 0x64765649, F_IN4 | F_OUT4),
    TYPE("EPP2318", 0x647656e9, F_IN4 | F_OUT4),
    TYPE("EPP2328", 0x64765789, F_IN4 | F_OUT4),
    TYPE("EPP2334", 0x647657e9, F_IN4 | F_OUT4),
    TYPE("EPP2338", 0x09224052, F_IN8 | F_OUT8),
    TYPE("EPP2339", 0x64765839, F_IN8 | F_OUT8),
    TYPE("EPP2349", 0x647658d9, F_IN8 | F_OUT8),
    {NULL},
};
ADD_TYPES(types)

typedef struct {
  lcec_class_din_pins_t *pins_in;
  lcec_class_dout_pins_t *pins_out;
} lcec_el1859_data_t;

static void lcec_el1859_read(struct lcec_slave *slave, long period);
static void lcec_el1859_write(struct lcec_slave *slave, long period);

static int lcec_el1859_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_el1859_data_t *hal_data;
  int i;
  int in_channels = INPORTS(slave->flags);
  int out_channels = OUTPORTS(slave->flags);
  int idx, sidx;

  // initialize callbacks
  slave->proc_read = lcec_el1859_read;
  slave->proc_write = lcec_el1859_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el1859_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", slave->master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el1859_data_t));
  slave->hal_data = hal_data;

  hal_data->pins_in = lcec_din_allocate_pins(in_channels);
  hal_data->pins_out = lcec_dout_allocate_pins(out_channels);

  // initialize pins
  for (i = 0; i < in_channels; i++) {
    if (slave->flags & F_DENSEPDOS) {
      idx = 0x6000;
      sidx = 1 + i;
    } else {
      idx = 0x6000 + (i << 4);
      sidx = 1;
    }
    hal_data->pins_in->pins[i] = lcec_din_register_pin(&pdo_entry_regs, slave, i, idx, sidx);
  }

  for (i = 0; i < out_channels; i++) {
    if (slave->flags & F_DENSEPDOS) {
      idx = 0x7000;
      sidx = 1 + i;
    } else {
      idx = 0x7000 + (i << 4);
      sidx = 1;
      if (slave->flags & F_OUTOFFSET) {
        idx += 0x80;
      }
    }
    hal_data->pins_out->pins[i] = lcec_dout_register_pin(&pdo_entry_regs, slave, i, idx, sidx);
  }
  return 0;
}

static void lcec_el1859_read(struct lcec_slave *slave, long period) {
  lcec_el1859_data_t *hal_data = (lcec_el1859_data_t *)slave->hal_data;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  lcec_din_read_all(slave, hal_data->pins_in);
}

static void lcec_el1859_write(struct lcec_slave *slave, long period) {
  lcec_el1859_data_t *hal_data = (lcec_el1859_data_t *)slave->hal_data;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  lcec_dout_write_all(slave, hal_data->pins_out);
}
