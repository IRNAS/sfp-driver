/*
 * sfp-driver - SFP driver
 *
 * Copyright (C) 2016 Jernej Kos <jernej@kos.mx>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SFP_DRIVER_SFP_H
#define SFP_DRIVER_SFP_H

#include <libubox/avl.h>
#include <uci.h>

struct sfp_diagnostics_item {
  float temperature;
  float vcc;
  float tx_bias;
  float tx_power;
  float rx_power;
};

struct sfp_diagnostics {
  struct sfp_diagnostics_item value;
  struct sfp_diagnostics_item error_upper;
  struct sfp_diagnostics_item error_lower;
  struct sfp_diagnostics_item warning_upper;
  struct sfp_diagnostics_item warning_lower;
};

struct sfp_module {
  char *bus;
  char *manufacturer;
  char *revision;
  char *serial_number;

  unsigned int type;
  unsigned int connector;
  unsigned int bitrate;
  unsigned int wavelength;

  uint8_t *vendor_specific;
  size_t vendor_specific_length;

  struct sfp_diagnostics diagnostics;

  // Module registry AVL tree node.
  struct avl_node avl;
};

int sfp_init(struct uci_context *uci);
int sfp_update_module_diagnostics(struct sfp_module *module);
struct avl_tree *sfp_get_modules();

#endif
