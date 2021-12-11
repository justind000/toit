// Copyright (C) 2018 Toitware ApS.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; version
// 2.1 only.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// The license can be found in the file `LICENSE` in the top level
// directory of this repository.

#include "top.h"

#ifdef TOIT_FREERTOS

#include "process.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>
#include <freertos/event_groups.h>
#include "event_sources/system_esp32.h"

bool provisioned = false;

enum Provision_Scheme
{
  BLE,
  SOFT_AP,
  CONSOLE
};
wifi_sta_config_t *wifi_sta_cfg;
wifi_config_t config;

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

namespace toit
{
  static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
  {
    if (event_base == WIFI_PROV_EVENT)
    {
      switch (event_id)
      {
      case WIFI_PROV_CRED_RECV:
      {
        // save the credentials passed into a wifi_config_t object
        memset(&config, 0, sizeof(config));
        wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        strncpy((char *)config.sta.ssid, (char *)wifi_sta_cfg->ssid, sizeof(config.sta.ssid));
        strncpy((char *)config.sta.password, (char *)wifi_sta_cfg->password, sizeof(config.sta.password));
        break;
      }
      case WIFI_PROV_CRED_FAIL:
      {
        // failed to connect with given WiFi information, notify failure, deinit and return
        provisioned = false;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
        break;
      }
      case WIFI_PROV_CRED_SUCCESS:
        // success, system will send event WIFI_PROV_END soon
        // save the credentials
        // TODO how to save them for Toit to see, it doesn't seem to use the ESP32 system
        esp_wifi_set_config(WIFI_IF_STA, &config);
        provisioned = true;
        break;
      case WIFI_PROV_END:
        // De-initialize manager once provisioning is finished
        wifi_prov_mgr_deinit();
        break;
      default:
        break;
      }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
      esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
      // Signal main application to continue execution
      provisioned = true;
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
      // catch this event to trigger the application flow to continue, failing to connect
      provisioned = false;
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
  }

  MODULE_IMPLEMENTATION(provision, MODULE_PROVISION)
  PRIMITIVE(init)
  {
    ARGS(int, provision_scheme, int, provision_security, cstring, proof_of_possession, cstring, service_name, cstring, service_key);

    // needed to save credentials
    // TODO probably not needed in Toit system
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
      nvs_flash_erase();
      nvs_flash_init();
    }
    // start WiFi
    esp_netif_init();
    esp_netif_create_default_wifi_sta();
    if (provision_scheme == SOFT_AP)
    {
      esp_netif_create_default_wifi_ap();
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // configure scheme for BLE or AP
    wifi_prov_mgr_config_t config;
    if (provision_scheme == BLE)
    {
      config = {
          .scheme = wifi_prov_scheme_ble,
          .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};
    }
    else if (provision_scheme == SOFT_AP)
    {
      config = {
          .scheme = wifi_prov_scheme_softap,
          .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};
    }

    // Create an event loop to catch events
    esp_event_loop_create_default();
    wifi_event_group = xEventGroupCreate();

    // Register event handler for Wi-Fi, IP and Provisioning related events
    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);
    wifi_prov_mgr_init(config);

    // TODO not used in Toit 
    wifi_prov_mgr_is_provisioned(&provisioned);

    // The device name in BLE mode
    // The WiFi network name in AP mode
    // PROVO_ prefix used to work easily with Espressif sample apps
    if (strlen(service_name) == 0)
    {
      service_name = "PROV_TOIT";
    }

    // WIFI_PROV_SECURITY_0 plain text communication.
    // WIFI_PROV_SECURITY_1 secure communication which consists of secure handshake
    //           using X25519 key exchange and proof of possession (pop) and AES-CTR
    //           for encryption/decryption of messages.

    wifi_prov_security_t security = (wifi_prov_security_t)provision_security;

    // Proof-of-possession can be used in conjunction with a display.
    // The device will send a string (abcd1234 for example)
    // The app will ask the user to verify the string which is displayed on the device
    // Not valid when used in AP mode with a service-key
    if (strlen(proof_of_possession) == 0)
    {
      proof_of_possession = NULL;
    }

    // service_key is used for AP mode only and is the password to connect
    // unused for BLE mode
    if (strlen(service_key) == 0)
    {
      service_key = NULL;
    }

    if (provision_scheme == BLE)
    {
      uint8_t custom_service_uuid[] = {
          0xb4,
          0xdf,
          0x5a,
          0x1c,
          0x3f,
          0x6b,
          0xf4,
          0xbf,
          0xea,
          0x4a,
          0x82,
          0x03,
          0x04,
          0x90,
          0x1a,
          0x02,
      };
      wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
    }

    // give the app time to receive the connected status before system deinits
    wifi_prov_mgr_disable_auto_stop(5000);

    // start service
    wifi_prov_mgr_start_provisioning(security, proof_of_possession, service_name, service_key);

    // wait for WIFI_CONNECTED_EVENT
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);

    if (provisioned)
    {
      return process->program()->true_object();
    }
    else
    {
      return process->program()->false_object();
    }
  }

} // namespace toit
#endif // TOIT_FREERTOS
