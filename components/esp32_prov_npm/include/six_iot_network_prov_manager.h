/*
 * six_iot_network_prov_manager.h
 *
 *  Created on: 2023年7月22日
 *      Author: Stephen Yu
 */

#ifndef __SIX_IOT_NETWORK_PROV_MANAGER_H__
#define __SIX_IOT_NETWORK_PROV_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_wifi_types.h>
#include "six_iot_config.h"

// set device in network provision manager mode, then the user can use the QR code to provisoin the device
void six_iot_start_network_prov_manager_mode(six_iot_config_t *iot_config, wifi_ap_config_t *config,
										esp_event_loop_handle_t loop_handle);

#ifdef __cplusplus
}
#endif

#endif /* __SIX_IOT_NETWORK_PROV_MANAGER_H__ */
