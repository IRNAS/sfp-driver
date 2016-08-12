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
#include "sfp.h"
#include "util.h"

#include <libubox/avl-cmp.h>
#include <libubox/uloop.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SFP_I2C_PROBE_BUS_MAX 5
#define SFP_I2C_INFO_ADDRESS 0x50
#define SFP_I2C_DIAG_ADDRESS 0x51

#define SFP_MANUFACTURER_OFFSET 20
#define SFP_MANUFACTURER_LENGTH 16

#define SFP_REVISION_OFFSET 56
#define SFP_REVISION_LENGTH 4

#define SFP_SERIAL_NO_OFFSET 68
#define SFP_SERIAL_NO_LENGTH 16

#define SFP_TYPE_OFFSET 0
#define SFP_CONNECTOR_OFFSET 2
#define SFP_BITRATE_OFFSET 12
#define SFP_WAVELENGTH_OFFSET 60
#define SFP_CHECKSUM_OFFSET 63

#define SFP_VENDOR_SPECIFIC_OFFSET 96
#define SFP_VENDOR_SPECIFIC_LENGTH 32

#define SFP_DIAG_VALUE_OFFSET 96
#define SFP_DIAG_VALUE_STRIDE 2

#define SFP_DIAG_ERROR_UP_OFFSET 0
#define SFP_DIAG_ERROR_UP_STRIDE 8

#define SFP_DIAG_ERROR_LO_OFFSET 2
#define SFP_DIAG_ERROR_LO_STRIDE 8

#define SFP_DIAG_WARNING_UP_OFFSET 4
#define SFP_DIAG_WARNING_UP_STRIDE 8

#define SFP_DIAG_WARNING_LO_OFFSET 6
#define SFP_DIAG_WARNING_LO_STRIDE 8

// An AVL tree containing all the registered SFP modules.
static struct avl_tree module_registry;
// Timer for periodic SFP module autodiscovery.
struct uloop_timeout timer_autodiscovery;

void sfp_module_autodiscovery(struct uloop_timeout *timeout);
int sfp_init_module(const char *bus);
void sfp_free_module(struct sfp_module *module);
int sfp_update_module_diagnostics_item(struct sfp_diagnostics_item *item, uint8_t *buffer, size_t stride);
void sfp_copy_string(char **destination, uint8_t *buffer, size_t offset, size_t length);
void sfp_copy_data(uint8_t **destination, uint8_t *buffer, size_t offset, size_t length);
int i2c_open(const char *bus, int address);
int i2c_close(int i2c_bus);
int i2c_read_data(int i2c_bus, uint8_t *data, size_t size);

int sfp_init(struct uci_context *uci)
{
  syslog(LOG_INFO, "Initializing SFP modules.");

  // Initialize the module registry.
  avl_init(&module_registry, avl_strcmp, false, NULL);

  // Initialize timers.
  timer_autodiscovery.cb = sfp_module_autodiscovery;
  sfp_module_autodiscovery(&timer_autodiscovery);

  return 0;
}

struct avl_tree *sfp_get_modules()
{
  return &module_registry;
}

void sfp_module_autodiscovery(struct uloop_timeout *timeout)
{
  // Attempt to autodiscover SFP modules on all I2C buses.
  for (size_t bus = 0; bus < SFP_I2C_PROBE_BUS_MAX; bus++) {
    char bus_name[64];
    struct sfp_module *module;
    snprintf(bus_name, sizeof(bus_name), "/dev/i2c-%u", bus);

    // Skip bus if a module has already been discovered on it.
    int exists = 0;
    avl_for_each_element(&module_registry, module, avl) {
      if (strcmp(module->bus, bus_name) == 0) {
        exists = 1;
        break;
      }
    }

    if (!exists) {
      sfp_init_module(bus_name);
    }
  }

  uloop_timeout_set(timeout, 10000);
}

int sfp_init_module(const char *bus)
{
  int result = 0;
  int i2c_bus = i2c_open(bus, SFP_I2C_INFO_ADDRESS);
  if (i2c_bus < 0) {
    return -1;
  }

  uint8_t buffer[256];
  i2c_read_data(i2c_bus, buffer, sizeof(buffer));

  // Verify checksum.
  uint8_t checksum = 0;
  for (size_t i = 0; i < SFP_CHECKSUM_OFFSET; i++) {
    checksum += buffer[i];
  }

  if (checksum != buffer[SFP_CHECKSUM_OFFSET]) {
    i2c_close(i2c_bus);
    return -1;
  }

  struct sfp_module *module = (struct sfp_module*) malloc(sizeof(struct sfp_module));
  module->bus = strdup(bus);
  sfp_copy_string(&module->manufacturer, buffer, SFP_MANUFACTURER_OFFSET, SFP_MANUFACTURER_LENGTH);
  sfp_copy_string(&module->revision, buffer, SFP_REVISION_OFFSET, SFP_REVISION_LENGTH);
  sfp_copy_string(&module->serial_number, buffer, SFP_SERIAL_NO_OFFSET, SFP_SERIAL_NO_LENGTH);
  module->type = (unsigned int) buffer[SFP_TYPE_OFFSET];
  module->connector = (unsigned int) buffer[SFP_CONNECTOR_OFFSET];
  module->bitrate = (unsigned int) buffer[SFP_BITRATE_OFFSET] * 100;
  module->wavelength = (unsigned int) buffer[SFP_WAVELENGTH_OFFSET] * 256 + buffer[SFP_WAVELENGTH_OFFSET + 1];

  sfp_copy_data(&module->vendor_specific, buffer, SFP_VENDOR_SPECIFIC_OFFSET, SFP_VENDOR_SPECIFIC_LENGTH);
  module->vendor_specific_length = SFP_VENDOR_SPECIFIC_LENGTH;

  // Insert discovered module into AVL tree.
  module->avl.key = module->serial_number;
  if (avl_insert(&module_registry, &module->avl) != 0) {
    sfp_free_module(module);
    result = -1;
  }

  i2c_close(i2c_bus);

  // Output some information about the newly discovered SFP module.
  syslog(LOG_INFO, "Discovered new SFP module on bus '%s':", bus);
  syslog(LOG_INFO, "  Manufacturer: %s", module->manufacturer);
  syslog(LOG_INFO, "  Serial number: %s", module->serial_number);
  syslog(LOG_INFO, "  Type: 0x%02X", module->type);
  syslog(LOG_INFO, "  Connector: 0x%02X", module->connector);
  syslog(LOG_INFO, "  Bitrate: %u MBd", module->bitrate);
  syslog(LOG_INFO, "  Wavelength: %u nm", module->wavelength);

  // Update diagnostics.
  sfp_update_module_diagnostics(module);

  return result;
}

void sfp_free_module(struct sfp_module *module)
{
  free(module->bus);
  free(module->manufacturer);
  free(module->serial_number);
  free(module->vendor_specific);
  free(module);
}

static inline float convert_number(uint8_t *data, uint16_t divisor, int is_signed)
{
  if (is_signed) {
    return ((int16_t) ((data[0] << 8) + data[1])) / (float) divisor;
  } else {
    return ((uint16_t) ((data[0] << 8) + data[1])) / (float) divisor;
  }
}

int sfp_update_module_diagnostics_item(struct sfp_diagnostics_item *item, uint8_t *buffer, size_t stride)
{
  item->temperature = convert_number(&buffer[0], 256, 1);
  item->vcc = convert_number(&buffer[stride], 10000, 0);
  item->tx_bias = convert_number(&buffer[2 * stride], 500, 0);
  item->tx_power = convert_number(&buffer[3 * stride], 10000, 0);
  item->rx_power = convert_number(&buffer[4 * stride], 10000, 0);
  return 0;
}

int sfp_update_module_diagnostics(struct sfp_module *module)
{
  int i2c_bus = i2c_open(module->bus, SFP_I2C_DIAG_ADDRESS);
  if (i2c_bus < 0) {
    syslog(LOG_ERR, "Failed to read diagnostic data from module on bus '%s'.", module->bus);
    return -1;
  }

  uint8_t buffer[256];
  i2c_read_data(i2c_bus, buffer, sizeof(buffer));

  sfp_update_module_diagnostics_item(&module->diagnostics.value, &buffer[SFP_DIAG_VALUE_OFFSET], SFP_DIAG_VALUE_STRIDE);
  sfp_update_module_diagnostics_item(&module->diagnostics.error_upper, &buffer[SFP_DIAG_ERROR_UP_OFFSET], SFP_DIAG_ERROR_UP_STRIDE);
  sfp_update_module_diagnostics_item(&module->diagnostics.error_lower, &buffer[SFP_DIAG_ERROR_LO_OFFSET], SFP_DIAG_ERROR_LO_STRIDE);
  sfp_update_module_diagnostics_item(&module->diagnostics.warning_upper, &buffer[SFP_DIAG_WARNING_UP_OFFSET], SFP_DIAG_WARNING_UP_STRIDE);
  sfp_update_module_diagnostics_item(&module->diagnostics.warning_lower, &buffer[SFP_DIAG_WARNING_LO_OFFSET], SFP_DIAG_WARNING_LO_STRIDE);

  i2c_close(i2c_bus);
  return 0;
}

void sfp_copy_string(char **destination, uint8_t *buffer, size_t offset, size_t length)
{
  *destination = (char*) malloc(length + 1);
  memcpy(*destination, buffer + offset, length);
  (*destination)[length] = 0;
  trim(*destination);
}

void sfp_copy_data(uint8_t **destination, uint8_t *buffer, size_t offset, size_t length)
{
  *destination = (uint8_t*) malloc(length);
  memcpy(*destination, buffer + offset, length);
}

int i2c_open(const char *bus, int address)
{
  int i2c_bus = open(bus, O_RDWR);
  if (i2c_bus < 0) {
    return -1;
  }

  if (ioctl(i2c_bus, I2C_SLAVE, address) < 0) {
    close(i2c_bus);
    return -1;
  }

  return i2c_bus;
}

int i2c_read_data(int i2c_bus, uint8_t *data, size_t size)
{
  char buffer[1] = {0x00};

  write(i2c_bus, buffer, 1);

  size_t bytes = 0;
  for (; bytes <= 255 && bytes < size; bytes++){
    read(i2c_bus, buffer, 1);
    data[bytes] = buffer[0];
  }

  return bytes;
}

int i2c_close(int i2c_bus)
{
  close(i2c_bus);
  return 0;
}
