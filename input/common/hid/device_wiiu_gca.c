/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hid_device_driver.h"

#ifdef WII
static uint8_t activation_packet[] = { 0x01, 0x13 };
#else
static uint8_t activation_packet[] = { 0x13 };
#endif

#define GCA_PORT_INITIALIZING 0x00
#define GCA_PORT_EMPTY        0x04
#define GCA_PORT_CONNECTED    0x14

typedef struct wiiu_gca_instance {
  void *handle;
  bool online;
  uint8_t device_state[37];
  joypad_connection_t *pads[4];
} wiiu_gca_instance_t;

static void update_pad_state(wiiu_gca_instance_t *instance);
static joypad_connection_t *register_pad(wiiu_gca_instance_t *instance);
static void unregister_pad(wiiu_gca_instance_t *instance, int slot);

extern pad_connection_interface_t wiiu_gca_pad_connection;

static void *wiiu_gca_init(void *handle)
{
  RARCH_LOG("[gca]: allocating driver instance...\n");
  wiiu_gca_instance_t *instance = calloc(1, sizeof(wiiu_gca_instance_t));
  if(instance == NULL) goto error;
  RARCH_LOG("[gca]: zeroing memory...\n");
  memset(instance, 0, sizeof(wiiu_gca_instance_t));
  instance->handle = handle;

  RARCH_LOG("[gca]: sending activation packet to device...\n");
  hid_instance.os_driver->send_control(handle, activation_packet, sizeof(activation_packet));
  RARCH_LOG("[gca]: reading initial state packet...\n");
  hid_instance.os_driver->read(handle, instance->device_state, sizeof(instance->device_state));
  instance->online = true;

  RARCH_LOG("[gca]: init done\n");
  return instance;

  error:
    RARCH_ERR("[gca]: init failed\n");
    if(instance)
       free(instance);
    return NULL;
}

static void wiiu_gca_free(void *data) {
   wiiu_gca_instance_t *instance = (wiiu_gca_instance_t *)data;
   int i;

   if(instance) {
      instance->online = false;

      for(i = 0; i < 4; i++)
        unregister_pad(instance, i);

      free(instance);
  }
}

static void wiiu_gca_handle_packet(void *data, uint8_t *buffer, size_t size)
{
  wiiu_gca_instance_t *instance = (wiiu_gca_instance_t *)data;
  if(!instance || !instance->online)
    return;

  if(size > sizeof(instance->device_state))
    return;

  memcpy(instance->device_state, buffer, size);
  update_pad_state(instance);
}

static void update_pad_state(wiiu_gca_instance_t *instance)
{
   int i, pad;
   if(!instance || !instance->online)
      return;

   /* process each pad */
   for(i = 1; i < 37; i += 9)
   {
      pad = i / 9;
      switch(instance->device_state[i])
      {
         case GCA_PORT_INITIALIZING:
         case GCA_PORT_EMPTY:
            if(instance->pads[pad] != NULL)
               unregister_pad(instance, pad);
            break;
         case GCA_PORT_CONNECTED:
            if(instance->pads[pad] == NULL)
               instance->pads[pad] = register_pad(instance);
      }
   }
}

static joypad_connection_t *register_pad(wiiu_gca_instance_t *instance) {
   int slot;
   joypad_connection_t *result;

   if(!instance || !instance->online)
      return NULL;

   slot = pad_connection_find_vacant_pad(hid_instance.pad_list);
   if(slot < 0)
      return NULL;

   result = &(hid_instance.pad_list[slot]);
   result->iface = &wiiu_gca_pad_connection;
   result->data = result->iface->init(instance, slot, hid_instance.os_driver);
   result->connected = true;
   input_pad_connect(slot, hid_instance.pad_driver);

   return result;
}

static void unregister_pad(wiiu_gca_instance_t *instance, int slot)
{
   if(!instance || slot < 0 || slot >= 4 || instance->pads[slot] == NULL)
      return;

   joypad_connection_t *pad = instance->pads[slot];
   instance->pads[slot] = NULL;
   pad->iface->deinit(pad->data);
   pad->data = NULL;
   pad->connected = false;
}

static bool wiiu_gca_detect(uint16_t vendor_id, uint16_t product_id) {
   return vendor_id == VID_NINTENDO && product_id == PID_NINTENDO_GCA;
}

hid_device_t wiiu_gca_hid_device = {
   wiiu_gca_init,
   wiiu_gca_free,
   wiiu_gca_handle_packet,
   wiiu_gca_detect,
   "Wii U Gamecube Adapter"
};

pad_connection_interface_t wiiu_gca_pad_connection = {
/*
   wiiu_gca_pad_init,
   wiiu_gca_pad_deinit,
   wiiu_gca_packet_handler,
   wiiu_gca_set_rumble,
   wiiu_gca_get_buttons,
   wiiu_gca_get_axis,
   wiiu_gca_get_name
*/
};
