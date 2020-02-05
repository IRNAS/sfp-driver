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
#include <stdint.h>

// SFP module autodiscovery interval (in milliseconds).
#define SFP_AUTODISCOVERY_INTERVAL 10000
// SFP module diagnostic update interval (in milliseconds).
#define SFP_UPDATE_INTERVAL 1000
// SFP statistics window size (in number of samples).
#define SFP_STATISTICS_BUFFER_SIZE 600

struct sfp_statistics_item {
  float sum;
  float average;
  float variance;
  float maximum;
  float minimum;

  float buffer[SFP_STATISTICS_BUFFER_SIZE];
  size_t samples;
  size_t index;
};

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

struct sfp_statistics {
  struct sfp_statistics_item temperature;
  struct sfp_statistics_item vcc;
  struct sfp_statistics_item tx_bias;
  struct sfp_statistics_item tx_power;
  struct sfp_statistics_item rx_power;
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
  struct sfp_statistics statistics;

  // Module registry AVL tree node.
  struct avl_node avl;
};

int sfp_init(const int bus_min, const int bus_max);
int sfp_update_module_statistics(struct sfp_module *module);
struct avl_tree *sfp_get_modules();

#endif
