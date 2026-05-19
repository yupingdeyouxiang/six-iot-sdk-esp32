/*
 * six_iot_prov_softap.h
 *
 *  Created on: 2023年7月22日
 *      Author: Stephen Yu
 */

#ifndef __SIX_IOT_PROV_SOFTAP_H__
#define __SIX_IOT_PROV_SOFTAP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "six_iot_config.h"
#include <esp_wifi_types.h>

// set device in soft_ap mode, then it can be used to receive the SSID and
// PASSWORD of other AP()
void six_iot_start_softap_mode(six_iot_config_t *iot_config, wifi_ap_config_t *config,
										  esp_event_loop_handle_t loop_handle);

// six_iot_provision_status_handler_t six_iot_get_provision_status_handler_for_softap();

// six_iot_bind_device_result_handler_t six_iot_get_bind_device_result_handler_for_softap();

#ifdef __cplusplus
}
#endif

#endif /* __SIX_IOT_PROV_SOFTAP_H__ */
