#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
#include <esp_bt.h>
#endif
#include <esp_blufi.h>
#include <esp_blufi_api.h>

#include "six_iot_config.h"
#include "six_iot_prov_blufi.h"

#define WIFI_CONNECTION_MAXIMUM_RETRY CONFIG_WIFI_CONNECTION_MAXIMUM_RETRY

#define S_INVALID_REASON 255

#define S_INVALID_RSSI -128

#define WIFI_LIST_NUM 10

static const char *TAG = "six_iot_prov_blufi";

static wifi_config_t sta_config;

static wifi_config_t ap_config;

/* FreeRTOS event group to signal when we are connected & ready to make a
 * request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Retry num to connect to WIFI router */
static uint8_t s_wifi_retry = 0;

/* If the WIFI is initialized */
static bool s_wifi_initialized = false;

/* Device as STA has connected to WIFI router(internet) */
static bool gl_sta_connected = false;

/* Device as STA is connecting */
static bool gl_sta_is_connecting = false;

/* if the device as STA has got IP */
static bool gl_sta_got_ip = false;

/* if the BLE is connected */
static bool ble_is_connected = false;

static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;

/* STA list structure */
static wifi_sta_list_t gl_sta_list;

static esp_blufi_extra_info_t gl_sta_conn_info;

/* Event loop to handle the callback */
static esp_event_loop_handle_t s_six_iot_loop = NULL;

/* IoT config type */
static six_iot_config_t *s_iot_cfg = NULL;

/* The status of the provision operation */
static six_iot_event_t s_six_iot_conn_status = PROVISION_STATUS_NOT_START;

/* Define a very simple BLE CMD to handle the device binding request */
static const uint8_t SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD = 4;

/* "Device Binding" message should be started with "c:b:", c is command, b is
 * binding */
const char *SIX_IOT_CMD_BIND_DEVICE = "c:b:";

/**Inner methods definition start**/

static void _six_iot_init_blufi(wifi_ap_config_t *config);

static void _record_wifi_conn_info(int rssi, uint8_t reason);

static void _wifi_connect(void);

static bool _wifi_reconnect(void);

static void _blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

static int _softap_get_current_connection_number(void);

static void _wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void _ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void _initialise_wifi(wifi_mode_t mode);

static void _blufi_report_wifi_status(void);

static void _blufi_handle_custom_data(const char *data, uint32_t data_len);

/**Inner methods definition end**/

static void s_report_provision_status(six_iot_event_t status, char *msg, void *args) {
	ESP_LOGD(TAG, "s_report_provision_status.msg: %s", msg);
	s_six_iot_conn_status = status;
	if (s_six_iot_loop) {
		if ((status == PROVISION_STATUS_AS_STA_CONN_TO_AP_SUCCEED || status == PROVISION_STATUS_BIND_DEVICE_REC_CMD) &&
			args) {
			char *data = args;
			int size = sizeof(char) * (strlen(data) + 1);
			esp_event_post_to(s_six_iot_loop, SIX_IOT_EVENT, status, data, size, 100 / portTICK_PERIOD_MS);
		} else {
			esp_event_post_to(s_six_iot_loop, SIX_IOT_EVENT, status, args, sizeof(args), 100 / portTICK_PERIOD_MS);
		}
	}
}

// six_iot_provision_status_handler_t six_iot_get_provision_status_handler_for_blufi() { return NULL; }

// six_iot_bind_device_result_handler_t six_iot_get_bind_device_result_handler_for_blufi() { return NULL; }

/**
 *  record the sta_conn_info
 * */
static void _record_wifi_conn_info(int rssi, uint8_t reason) {
	memset(&gl_sta_conn_info, 0, sizeof(esp_blufi_extra_info_t));
	if (gl_sta_is_connecting) {
		gl_sta_conn_info.sta_max_conn_retry_set = true;
		gl_sta_conn_info.sta_max_conn_retry = WIFI_CONNECTION_MAXIMUM_RETRY;
	} else {
		gl_sta_conn_info.sta_conn_rssi_set = true;
		gl_sta_conn_info.sta_conn_rssi = rssi;
		gl_sta_conn_info.sta_conn_end_reason_set = true;
		gl_sta_conn_info.sta_conn_end_reason = reason;
	}
}

/**
 * connect wifi
 * */
static void _wifi_connect(void) {
	s_wifi_retry = 0;
	// report the status, start to connect to the WIFI router
	s_report_provision_status(PROVISION_STATUS_AS_STA_CONN_TO_AP_START, "Set device as STA to connect to AP!", NULL);
	// set the connecting status, only esp_wifi_connect return OK
	gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
	// record the wifi_conn_info
	_record_wifi_conn_info(S_INVALID_RSSI, S_INVALID_REASON);
}

static bool _wifi_reconnect(void) {
	bool ret;
	if (gl_sta_is_connecting && s_wifi_retry++ < WIFI_CONNECTION_MAXIMUM_RETRY) {
		BLUFI_INFO("BLUFI WiFi start reconnection...");
		gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
		_record_wifi_conn_info(S_INVALID_RSSI, S_INVALID_REASON);
		ret = true;
	} else {
		s_report_provision_status(PROVISION_STATUS_AS_STA_FAIL_CONN_TO_AP,
								  "Device as STA fail to connect to AP and "
								  "retry number threshold is reached!",
								  &s_wifi_retry);
		ret = false;
	}
	return ret;
}

static int _softap_get_current_connection_number(void) {
	esp_err_t ret;
	ret = esp_wifi_ap_get_sta_list(&gl_sta_list);
	if (ret == ESP_OK) {
		return gl_sta_list.num;
	}
	return 0;
}

static void _ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	switch (event_id) {
	case IP_EVENT_STA_GOT_IP: {
		// set the CONNECTED_BIT
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		gl_sta_got_ip = true;
		// call the _blufi_report_wifi_status to get the status of the WIFI and
		// report it to the Phone through BLE
		_blufi_report_wifi_status();
		// assuming gl_sta_conected will be alway true here since the IP is got
		if (gl_sta_connected) {
			s_report_provision_status(PROVISION_STATUS_AS_STA_CONN_TO_AP_SUCCEED,
									  "Device connect to AP succeed, IP is got!", NULL);
		}
		break;
	}
	default:
		break;
	}
	return;
}

static void _wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	wifi_event_sta_connected_t *event;
	wifi_event_sta_disconnected_t *disconnected_event;
	wifi_mode_t mode;

	switch (event_id) {
	case WIFI_EVENT_STA_START:
		ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
		_wifi_connect();
		break;
	case WIFI_EVENT_STA_CONNECTED:
		ESP_LOGD(TAG, "WIFI_EVENT_STA_CONNECTED");
		gl_sta_connected = true;
		gl_sta_is_connecting = false;
		event = (wifi_event_sta_connected_t *)event_data;
		memcpy(gl_sta_bssid, event->bssid, 6);
		memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
		gl_sta_ssid_len = event->ssid_len;
		break;
	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED");
		/* Only handle reconnection during connecting */
		if (gl_sta_connected == false && _wifi_reconnect() == false) {
			gl_sta_is_connecting = false;
			disconnected_event = (wifi_event_sta_disconnected_t *)event_data;
			_record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
			// actively report the status to the BLE slave
			_blufi_report_wifi_status();
		}
		/* This is a workaround as ESP32 WiFi libs don't currently
		   auto-reassociate. */
		gl_sta_connected = false;
		gl_sta_got_ip = false;
		memset(gl_sta_ssid, 0, 32);
		memset(gl_sta_bssid, 0, 6);
		gl_sta_ssid_len = 0;
		// clean the CONNECTED_BIT
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		break;
	case WIFI_EVENT_AP_START:
		esp_wifi_get_mode(&mode);
		/* TODO: get config or information of softap, then set to report
		 * extra_info */
		if (ble_is_connected == true) {
			if (gl_sta_connected) {
				esp_blufi_extra_info_t info;
				memset(&info, 0, sizeof(esp_blufi_extra_info_t));
				memcpy(info.sta_bssid, gl_sta_bssid, 6);
				info.sta_bssid_set = true;
				info.sta_ssid = gl_sta_ssid;
				info.sta_ssid_len = gl_sta_ssid_len;
				esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP,
												_softap_get_current_connection_number(), &info);
			} else if (gl_sta_is_connecting) {
				esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, _softap_get_current_connection_number(),
												&gl_sta_conn_info);
			} else {
				esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, _softap_get_current_connection_number(),
												&gl_sta_conn_info);
			}
		} else {
			BLUFI_INFO("BLUFI BLE is not connected yet\n");
		}
		break;
	case WIFI_EVENT_SCAN_DONE: {
		uint16_t apCount = 0;
		esp_wifi_scan_get_ap_num(&apCount);
		if (apCount == 0) {
			BLUFI_INFO("Nothing AP found");
			break;
		}
		wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
		if (!ap_list) {
			BLUFI_ERROR("malloc error, ap_list is NULL");
			break;
		}
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));
		esp_blufi_ap_record_t *blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
		if (!blufi_ap_list) {
			if (ap_list) {
				free(ap_list);
			}
			BLUFI_ERROR("malloc error, blufi_ap_list is NULL");
			break;
		}
		for (int i = 0; i < apCount; ++i) {
			blufi_ap_list[i].rssi = ap_list[i].rssi;
			memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
		}

		if (ble_is_connected == true) {
			esp_blufi_send_wifi_list(apCount, blufi_ap_list);
		} else {
			BLUFI_INFO("BLUFI BLE is not connected yet\n");
		}

		esp_wifi_scan_stop();
		free(ap_list);
		free(blufi_ap_list);
		break;
	}
	case WIFI_EVENT_AP_STACONNECTED: {
		wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
		BLUFI_INFO("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
		break;
	}
	case WIFI_EVENT_AP_STADISCONNECTED: {
		wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
		BLUFI_INFO("station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
		break;
	}

	default:
		break;
	}
	return;
}

static void _initialise_wifi(wifi_mode_t mode) {
	if (s_wifi_initialized) {
		BLUFI_INFO("WIFI mode is initialized");
		return;
	}

	// init the net interface
	ESP_ERROR_CHECK(esp_netif_init());
	// create the event group and the event loop
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// create the wifi_sta to connect to the public AP
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

	// create the wifi_ap to receive the sta to connect to
	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_ip_event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// set wifi storage to use FLASH to save the WIFI credential
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
	ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
	_record_wifi_conn_info(S_INVALID_RSSI, S_INVALID_REASON);

	// start the wifi
	ESP_ERROR_CHECK(esp_wifi_start());
	s_wifi_initialized = true;
}

static esp_blufi_callbacks_t s_blufi_callbacks = {
	.event_cb = _blufi_event_callback,
	.negotiate_data_handler = blufi_dh_negotiate_data_handler,
	.encrypt_func = blufi_aes_encrypt,
	.decrypt_func = blufi_aes_decrypt,
	.checksum_func = blufi_crc_checksum,
};

static void _blufi_report_wifi_status() {
	if (!ble_is_connected) {
		BLUFI_INFO("BLUFI get WiFi status from AP, BLE is not connected, can't "
				   "report the status\n");
		return;
	}

	wifi_mode_t mode;
	esp_wifi_get_mode(&mode);

	esp_blufi_extra_info_t info;
	if (gl_sta_connected) {
		memset(&info, 0, sizeof(esp_blufi_extra_info_t));
		memcpy(info.sta_bssid, gl_sta_bssid, 6);
		info.sta_bssid_set = true;
		info.sta_ssid = gl_sta_ssid;
		info.sta_ssid_len = gl_sta_ssid_len;
		esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP,
										_softap_get_current_connection_number(), &info);
	} else if (gl_sta_is_connecting) {
		esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, _softap_get_current_connection_number(),
										&gl_sta_conn_info);
	} else {
		esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, _softap_get_current_connection_number(),
										&gl_sta_conn_info);
	}
	BLUFI_INFO("BLUFI get and report wifi status from AP\n");
}

static void _blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
	/* actually, should post to blufi_task handle the procedure,
	 * now, as a example, we do it more simply */
	switch (event) {
	case ESP_BLUFI_EVENT_INIT_FINISH:
		BLUFI_INFO("BLUFI init finish\n");
		esp_blufi_adv_start();
		break;
	case ESP_BLUFI_EVENT_DEINIT_FINISH:
		BLUFI_INFO("BLUFI deinit finish\n");
		break;
	case ESP_BLUFI_EVENT_BLE_CONNECT:
		BLUFI_INFO("BLUFI ble connect\n");
		ble_is_connected = true;
		esp_blufi_adv_stop();
		blufi_security_init();
		break;
	case ESP_BLUFI_EVENT_BLE_DISCONNECT:
		BLUFI_INFO("BLUFI ble disconnect\n");
		ble_is_connected = false;
		blufi_security_deinit();
		esp_blufi_adv_start();
		break;
	case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
		BLUFI_INFO("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
		// set the wifi mode requested by the app
		ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
		//_initialise_wifi(param->wifi_mode.op_mode);
		break;
	case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
		BLUFI_INFO("BLUFI requset wifi connect to AP\n");
		/* there is no wifi callback when the device has already connected to
		this wifi so disconnect wifi before connection.
		*/
		// when requested by the app, will first disconnect the device then
		// connect again in order to trigger the wifi event
		esp_wifi_disconnect();
		_wifi_connect();
		break;
	case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
		BLUFI_INFO("BLUFI requset wifi disconnect from AP\n");
		esp_wifi_disconnect();
		break;
	case ESP_BLUFI_EVENT_REPORT_ERROR:
		BLUFI_ERROR("BLUFI report error, error code %d\n", param->report_error.state);
		esp_blufi_send_error_info(param->report_error.state);
		break;
	case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
		_blufi_report_wifi_status();
		break;
	}
	case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
		BLUFI_INFO("blufi close a gatt connection");
		esp_blufi_disconnect();
		break;
	case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
		/* TODO */
		break;
	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
		memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
		sta_config.sta.bssid_set = 1;
		// set the BSSID to sta_config and then set the config to the
		// STA(station)
		esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		BLUFI_INFO("Recv STA BSSID %s\n", sta_config.sta.ssid);
		break;
	case ESP_BLUFI_EVENT_RECV_STA_SSID:
		strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
		// terminating the SSID
		sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
		// set the SSID to the sta_config and then set the config to the
		// STA(station)
		esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		BLUFI_INFO("Recv STA SSID %s\n", sta_config.sta.ssid);
		break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
		strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
		sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
		// set the PASSWD to the sta_config and then set the config to the
		// STA(station)
		esp_wifi_set_config(WIFI_IF_STA, &sta_config);
		BLUFI_INFO("Recv STA PASSWORD %s\n", sta_config.sta.password);
		break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
		strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
		ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
		ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
		esp_wifi_set_config(WIFI_IF_AP, &ap_config);
		BLUFI_INFO("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
		break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
		strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
		ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
		esp_wifi_set_config(WIFI_IF_AP, &ap_config);
		BLUFI_INFO("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
		break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
		if (param->softap_max_conn_num.max_conn_num > 4) {
			return;
		}
		ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
		esp_wifi_set_config(WIFI_IF_AP, &ap_config);
		BLUFI_INFO("Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
		break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
		if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
			return;
		}
		ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
		esp_wifi_set_config(WIFI_IF_AP, &ap_config);
		BLUFI_INFO("Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
		break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
		if (param->softap_channel.channel > 13) {
			return;
		}
		ap_config.ap.channel = param->softap_channel.channel;
		esp_wifi_set_config(WIFI_IF_AP, &ap_config);
		BLUFI_INFO("Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
		break;
	case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
		wifi_scan_config_t scanConf = {.ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false};
		esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
		if (ret != ESP_OK) {
			esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
		}
		break;
	}
	case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
		BLUFI_INFO("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
		esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
		_blufi_handle_custom_data((const char *)param->custom_data.data, param->custom_data.data_len);
		break;
	case ESP_BLUFI_EVENT_RECV_USERNAME:
		/* Not handle currently */
		break;
	case ESP_BLUFI_EVENT_RECV_CA_CERT:
		/* Not handle currently */
		break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
		/* Not handle currently */
		break;
	case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
		/* Not handle currently */
		break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
		/* Not handle currently */
		break;
		;
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
		/* Not handle currently */
		break;
	default:
		break;
	}
}

static void _blufi_handle_custom_data(const char *data, uint32_t data_len) {
	// only handle the CMD that has prefix "c:b:"
	if (data_len > SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD) {
		char *prefix = (char *)calloc(SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD + 1, sizeof(char));
		strncpy(prefix, data, SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD);
		// if the CMD is bind device
		if (strcmp(prefix, SIX_IOT_CMD_BIND_DEVICE) == 0) {
			free(prefix);
			uint32_t principal_id_len = data_len - SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD;
			char *principal_id = (char *)calloc(principal_id_len + 1, sizeof(char));
			strncpy(principal_id, data + SIX_IOT_CUSTOM_DATA_MIN_LEN_FOR_CMD, principal_id_len);
			// esp_event_post_to(s_six_iot_loop, SIX_IOT_EVENT,
			// PROVISION_STATUS_BIND_DEVICE_REC_CMD, principal_id,
			// principal_id_len + 1, 10);
			s_report_provision_status(PROVISION_STATUS_BIND_DEVICE_REC_CMD, "Receive the CMD to bind device",
									  principal_id);
			free(principal_id);
		}
	}
}

static void _six_iot_init_blufi(wifi_ap_config_t *config) {
	esp_err_t ret;

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
	ret = esp_blufi_controller_init();
	if (ret) {
		BLUFI_ERROR("%s BLUFI controller init failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}
#endif

	ret = esp_blufi_host_and_cb_init(&s_blufi_callbacks);
	if (ret) {
		BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
}

void six_iot_start_blufi_mode(six_iot_config_t *iot_config, wifi_ap_config_t *config,
										esp_event_loop_handle_t loop_handle) {
	s_iot_cfg = iot_config;
	s_six_iot_loop = loop_handle;
	s_report_provision_status(PROVISION_STATUS_START_DEVICE_IN_BLUFI_MODE, "Start the device in blufi mode", NULL);
	_six_iot_init_blufi(config);
	// initialise the module in STA module
	_initialise_wifi(WIFI_MODE_STA);
}
