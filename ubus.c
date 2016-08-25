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
#include "ubus.h"
#include "sfp.h"

#include <libubox/blobmsg.h>

// Ubus reply buffer.
static struct blob_buf reply_buf;

// Ubus attributes.
enum {
  SFP_D_MODULE,
  __SFP_D_MAX,
};

static const struct blobmsg_policy sfp_module_policy[__SFP_D_MAX] = {
  [SFP_D_MODULE] = { .name = "module", .type = BLOBMSG_TYPE_STRING },
};

static inline void blobmsg_add_sfp_module_info(struct blob_buf *buffer, struct sfp_module *module)
{
  blobmsg_add_string(buffer, "bus", module->bus);
  blobmsg_add_string(buffer, "manufacturer", module->manufacturer);
  blobmsg_add_string(buffer, "revision", module->revision);
  blobmsg_add_string(buffer, "serial_number", module->serial_number);
  blobmsg_add_u16(buffer, "type", module->type);
  blobmsg_add_u16(buffer, "connector", module->connector);
  blobmsg_add_u16(buffer, "bitrate", module->bitrate);
  blobmsg_add_u16(buffer, "wavelength", module->wavelength);
}

static inline void blobmsg_add_float(struct blob_buf *buffer, const char *name, float value)
{
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%.4f", value);
  blobmsg_add_string(buffer, name, tmp);
}

static inline void blobmsg_add_sfp_module_diagnostics_item(struct blob_buf *buffer,
                                                           const char *name,
                                                           struct sfp_diagnostics_item *item)
{
  void *c = blobmsg_open_table(buffer, name);
  blobmsg_add_float(buffer, "temperature", item->temperature);
  blobmsg_add_float(buffer, "vcc", item->vcc);
  blobmsg_add_float(buffer, "tx_bias", item->tx_bias);
  blobmsg_add_float(buffer, "tx_power", item->tx_power);
  blobmsg_add_float(buffer, "rx_power", item->rx_power);
  blobmsg_close_table(buffer, c);
}

static inline void blobmsg_add_sfp_module_diagnostics(struct blob_buf *buffer, struct sfp_module *module)
{
  sfp_update_module_diagnostics(module);
  blobmsg_add_sfp_module_diagnostics_item(buffer, "value", &module->diagnostics.value);
  blobmsg_add_sfp_module_diagnostics_item(buffer, "error_upper", &module->diagnostics.error_upper);
  blobmsg_add_sfp_module_diagnostics_item(buffer, "error_lower", &module->diagnostics.error_lower);
  blobmsg_add_sfp_module_diagnostics_item(buffer, "warning_upper", &module->diagnostics.warning_upper);
  blobmsg_add_sfp_module_diagnostics_item(buffer, "warning_lower", &module->diagnostics.warning_lower);
}

static int ubus_get_modules(struct ubus_context *ctx, struct ubus_object *obj,
                            struct ubus_request_data *req, const char *method,
                            struct blob_attr *msg)
{
  struct blob_attr *tb[__SFP_D_MAX];
  void *c;
  struct sfp_module *module;

  blobmsg_parse(sfp_module_policy, __SFP_D_MAX, tb, blob_data(msg), blob_len(msg));

  blob_buf_init(&reply_buf, 0);

  if (tb[SFP_D_MODULE]) {
    // Filter to a specific module.
    module = avl_find_element(sfp_get_modules(), blobmsg_data(tb[SFP_D_MODULE]), module, avl);
    if (!module) {
      return UBUS_STATUS_NOT_FOUND;
    }

    c = blobmsg_open_table(&reply_buf, module->serial_number);

    if (strcmp(method, "get_modules") == 0) {
      blobmsg_add_sfp_module_info(&reply_buf, module);
    } else if (strcmp(method, "get_diagnostics") == 0) {
      blobmsg_add_sfp_module_diagnostics(&reply_buf, module);
    }

    blobmsg_close_table(&reply_buf, c);
  } else {
    // Iterate through all modules.
    avl_for_each_element(sfp_get_modules(), module, avl) {
      c = blobmsg_open_table(&reply_buf, module->serial_number);

      if (strcmp(method, "get_modules") == 0) {
        blobmsg_add_sfp_module_info(&reply_buf, module);
      } else if (strcmp(method, "get_diagnostics") == 0) {
        blobmsg_add_sfp_module_diagnostics(&reply_buf, module);
      }

      blobmsg_close_table(&reply_buf, c);
    }
  }

  ubus_send_reply(ctx, req, reply_buf.head);

  return UBUS_STATUS_OK;
}

static int ubus_get_vendor_specific_data(struct ubus_context *ctx, struct ubus_object *obj,
                                         struct ubus_request_data *req, const char *method,
                                         struct blob_attr *msg)
{
  struct blob_attr *tb[__SFP_D_MAX];
  struct sfp_module *module;

  blobmsg_parse(sfp_module_policy, __SFP_D_MAX, tb, blob_data(msg), blob_len(msg));

  blob_buf_init(&reply_buf, 0);

  if (!tb[SFP_D_MODULE]) {
    return UBUS_STATUS_INVALID_ARGUMENT;
  }

  // Filter to a specific module.
  module = avl_find_element(sfp_get_modules(), blobmsg_data(tb[SFP_D_MODULE]), module, avl);
  if (!module) {
    return UBUS_STATUS_NOT_FOUND;
  }

  blobmsg_add_field(&reply_buf, BLOBMSG_TYPE_UNSPEC, "vendor_specific",
    module->vendor_specific, module->vendor_specific_length);

  ubus_send_reply(ctx, req, reply_buf.head);

  return UBUS_STATUS_OK;
}

static const struct ubus_method sfp_methods[] = {
  UBUS_METHOD("get_modules", ubus_get_modules, sfp_module_policy),
  UBUS_METHOD("get_diagnostics", ubus_get_modules, sfp_module_policy),
  UBUS_METHOD("get_vendor_specific_data", ubus_get_vendor_specific_data, sfp_module_policy),
};

static struct ubus_object_type sfp_type =
  UBUS_OBJECT_TYPE("sfp", sfp_methods);

static struct ubus_object sfp_object = {
  .name = "sfp",
  .type = &sfp_type,
  .methods = sfp_methods,
  .n_methods = ARRAY_SIZE(sfp_methods),
};

int ubus_init(struct ubus_context *ubus)
{
  return ubus_add_object(ubus, &sfp_object);
}

