/*
 * six_iot_prov_blufi.h
 *
 *  Created on: 2023年7月22日
 *      Author: Stephen Yu
 */

#ifndef __SIX_IOT_PROV_BLUFI_H__
#define __SIX_IOT_PROV_BLUFI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_blufi_api.h>
#include <esp_err.h>
#include <esp_wifi_types.h>

#include "six_iot_config.h"

#define BLUFI_TAG "SIX_IOT_PROV_BLUFI"
#define BLUFI_INFO(fmt, ...) ESP_LOGD(BLUFI_TAG, fmt, ##__VA_ARGS__)
#define BLUFI_ERROR(fmt, ...) ESP_LOGE(BLUFI_TAG, fmt, ##__VA_ARGS__)

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
int esp_blufi_gap_register_callback(void);
esp_err_t esp_blufi_host_init(void);
esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
esp_err_t esp_blufi_host_deinit(void);
esp_err_t esp_blufi_controller_init(void);
esp_err_t esp_blufi_controller_deinit(void);

// set device in soft_ap mode, then it can be used to receive the SSID and
// PASSWORD of other AP()
void six_iot_start_blufi_mode(six_iot_config_t *iot_config, wifi_ap_config_t *config,
										esp_event_loop_handle_t loop_handle);

// six_iot_provision_status_handler_t six_iot_get_provision_status_handler_for_blufi();

// six_iot_bind_device_result_handler_t six_iot_get_bind_device_result_handler_for_blufi();

#ifdef __cplusplus
}
#endif

#endif /* __SIX_IOT_PROV_BLUFI_H__ */
