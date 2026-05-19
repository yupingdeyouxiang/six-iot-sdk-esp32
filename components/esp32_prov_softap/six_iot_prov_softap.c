/*
 * six_iot_prov_softap.c
 *
 *  Created on: 2023年7月22日
 *      Author: Stephen Yu
 */
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <nvs_flash.h>
#include <string.h>
#include <sys/param.h>

#include "six_iot_config.h"
#include "six_iot_prov_softap.h"

// The log tag
static const char *TAG = "six_iot_prov_softap";

static esp_event_loop_handle_t s_six_iot_loop = NULL;

// The maximum retry times when device as STA client to connect to AP
#define WIFI_CONNECTION_MAXIMUM_RETRY CONFIG_WIFI_CONNECTION_MAXIMUM_RETRY

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK

/*
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif
*/

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

// the retry num for connect the device to internet as STA
static int s_retry_num = 0;

// the WIFI credential/username
static char s_wifi_name[32] = {0};

// the WIFI credential/password
static char s_wifi_password[64] = {0};

// the "user guid" with which the device bind to
static char s_user_guid[64] = {0};

// s_httpd_server
static httpd_handle_t s_httpd_server = NULL;

static six_iot_config_t *s_iot_cfg = NULL;

// the status of the provision operation
static six_iot_event_t s_six_iot_conn_status = PROVISION_STATUS_NOT_START;

static char *s_bind_device_result = NULL;

/**Inner methods definition start**/

void _six_iot_stop_ap_web_server();

void _six_iot_wifi_init_softap(wifi_ap_config_t *config);

void _six_iot_wifi_init_sta(char *ssid, char *password);

void _six_iot_conn_to_field_ap_and_bind_device(char *ssid, char *password);

void _six_iot_set_provision_status(six_iot_event_t status);

void _six_iot_set_device_bind_result(char *result);

httpd_handle_t _six_iot_start_webserver_for_conn_field_ap();

static void _wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void _ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**Inner methods definition end**/

static void s_report_provision_status(six_iot_event_t status, char *msg, void *args) {
	ESP_LOGD(TAG, "s_report_provision_status.msg: %s", msg);
	s_six_iot_conn_status = status;
	if (s_six_iot_loop) {
		if (status == PROVISION_STATUS_AS_STA_CONN_TO_AP_SUCCEED) {
			char *data = args;
			int size = sizeof(char) * (strlen(data) + 1);
			esp_event_post_to(s_six_iot_loop, SIX_IOT_EVENT, status, data, size, 10);
		} else {
			esp_event_post_to(s_six_iot_loop, SIX_IOT_EVENT, status, args, sizeof(args), 10);
		}
	}
}

// six_iot_provision_status_handler_t six_iot_get_provision_status_handler_for_softap() {
// 	return _six_iot_set_provision_status;
// }

// six_iot_bind_device_result_handler_t six_iot_get_bind_device_result_handler_for_softap() {
// 	return _six_iot_set_device_bind_result;
// }

// void _six_iot_set_provision_status(six_iot_event_t status) { s_six_iot_conn_status = status; }

// void _six_iot_set_device_bind_result(char *result) { s_bind_device_result = result; }

static void _ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { // after connect to the Internet, can
																	 // get the IP that assigned to the
																	 // device and set the status also.
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGD(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		s_report_provision_status(PROVISION_STATUS_AS_STA_CONN_TO_AP_SUCCEED, "connect to AP succeed!", s_user_guid);
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

static void _wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	// if the event_id is STA_START
	if (event_id == WIFI_EVENT_STA_START) {
		ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
		s_report_provision_status(PROVISION_STATUS_AS_STA_CONN_TO_AP_START, "device as STA to connect to AP!", NULL);
		esp_wifi_connect();								  // connect to the Internet
	} else if (event_id == WIFI_EVENT_STA_DISCONNECTED) { // if the connection is
														  // disconnected, then retry the
														  // connection again.
		s_report_provision_status(PROVISION_STATUS_AS_STA_DISCONN_FROM_AP_AND_RETRY,
								  "device as STA disconnect from AP and retry!", &s_retry_num);
		ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED");
		if (s_retry_num < WIFI_CONNECTION_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGD(TAG, "retry to connect to the AP");
		} else {
			s_report_provision_status(PROVISION_STATUS_AS_STA_FAIL_CONN_TO_AP,
									  "device as STA fail to connect to AP and "
									  "retry number threshold is reached!",
									  &s_retry_num);
			// the retry number is more than the retry threshold set, then set
			// the status to the event group
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGD(TAG, "connect to the AP fail");
	} else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
		// wifi_event_ap_staconnected_t will be used to extract the data about
		// the STA client
		wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
		ESP_LOGD(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
		s_report_provision_status(PROVISION_STATUS_STA_CONN_TO_SOFTAP, "one STA is connected to softAP!", event->mac);
	} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
		ESP_LOGD(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
		s_report_provision_status(PROVISION_STATUS_STA_DIS_CONN_FROM_SOFTAP, "one STA is disconnected from softAP!",
								  event->mac);
	}
}

// Device will connect to the AP as STA station, it will wait for the status is
// returned by the event handler
void _six_iot_wifi_init_sta(char *ssid, char *password) {
	/*
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret ==
	ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ret =
	nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	*/

	s_report_provision_status(PROVISION_STATUS_INIT_AS_STA_START, "init WIFI in STA to connect to AP!", NULL);
	ESP_LOGD(TAG, "wifi_init_sta start, SSID is %s, and PASSWORD is %s", ssid, password);

	wifi_config_t wifi_config = {
		.sta =
			{
				/* Authmode threshold resets to WPA2 as default if password
				 * matches WPA2 standards (password len => 8). If you want to
				 * connect the device to deprecated WEP/WPA networks, Please set
				 * the threshold value to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and
				 * set the password with length and format matching to
				 * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
				 */
				.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
				.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
			},
	};

	// set the dynamic "ssid" and "password" to wifi_config
	memcpy(wifi_config.sta.ssid, ssid, strlen(ssid));
	memcpy(wifi_config.sta.password, password, strlen(password));

	// ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	// ESP_ERROR_CHECK(esp_wifi_start());
	esp_wifi_connect();

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT)
	 * or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
	 * The bits are set by event_handler() (see above) */
	EventBits_t bits =
		xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we
	 * can test which event actually happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGD(TAG, "connected to AP SSID:%s password:%s", ssid, password);
		//_six_iot_stop_ap_web_server();
		//_six_iot_bind_device();
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGD(TAG, "Failed to connect to SSID:%s, password:%s", ssid, password);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}

void _six_iot_conn_to_field_ap_and_bind_device(char *ssid, char *password) {
	ESP_LOGD(TAG, "six_iot_connect_to_internet with SSID: %s, password: %s", ssid, password);
	_six_iot_wifi_init_sta(ssid, password);
}

// the handler for the index page when the device act as soft ap
static esp_err_t s_wifi_provision_index_page_handler(httpd_req_t *req) {
	// load the content from the embed file
	extern const unsigned char script_start[] asm("_binary_wifi_html_start");
	extern const unsigned char script_end[] asm("_binary_wifi_html_end");
	const size_t script_size = (script_end - script_start);

	httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
	httpd_resp_send(req, (const char *)script_start, script_size);
	return ESP_OK;
}

httpd_uri_t wifi_provision_index_page = {
	.uri = "/", // 192.168.4.1
	.method = HTTP_GET,
	.handler = s_wifi_provision_index_page_handler,
	/* Let's pass response string in user
	 * context to demonstrate it's usage */
	.user_ctx = NULL,
};

// the handler to receive the AP credentials
static esp_err_t s_wifi_provision_credentials_post_page_handler(httpd_req_t *req) {
	int ret, remaining = req->content_len;

	int buffer_size = 100;
	char buf[buffer_size];

	int total_size = buffer_size;
	char *total = (char *)calloc(total_size, sizeof(char));
	int total_read = 0;

	while (remaining > 0) {
		/* Read the data for the request */
		if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		}

		if (total_read + ret > total_size) {
			ESP_LOGD(TAG, "expand the total buffer");
			total_size = total_size + buffer_size;
			// expand the buffer
			char *expanded_total = (char *)calloc(total_size, sizeof(char));
			// copy the old data to the new total buffer
			memcpy(expanded_total, total, total_read);
			// release the total
			free(total);
			// re point the total to expanded memory
			total = expanded_total;
		}
		// copy the buffer to the total
		memcpy(total + total_read, buf, ret);
		total_read += ret;
		remaining -= ret;
	}

	esp_err_t e = httpd_query_key_value(total, "ssid", s_wifi_name, sizeof(s_wifi_name));
	if (e == ESP_OK) {
		ESP_LOGD(TAG, "ssid = %s\n", s_wifi_name);
	} else {
		ESP_LOGE(TAG, "error = %d\n", e);
	}

	e = httpd_query_key_value(total, "pass", s_wifi_password, sizeof(s_wifi_password));
	if (e == ESP_OK) {
		ESP_LOGD(TAG, "wifi password = %s\n", s_wifi_password);
	} else {
		ESP_LOGE(TAG, "error = %d\n", e);
	}

	e = httpd_query_key_value(total, "userGlobalUuid", s_user_guid, sizeof(s_user_guid));
	if (e == ESP_OK) {
		ESP_LOGD(TAG, "userGlobalUuid = %s\n", s_user_guid);
	} else {
		ESP_LOGE(TAG, "error = %d\n", e);
	}

	/* Log data received */
	ESP_LOGD(TAG, "=========== RECEIVED DATA ==========");
	ESP_LOGD(TAG, "%.*s", total_read, total);
	ESP_LOGD(TAG, "====================================");
	// free the memory for the total
	free(total);

	char payload[200] = {0};
	sprintf(payload, "{\"ssid\":%s,\"password\":%s,\"userGlobalUuid\":%s}", s_wifi_name, s_wifi_password, s_user_guid);

	//	httpd_resp_send_chunk(req, buf, ret);
	//	httpd_resp_send_chunk(req, NULL, 0);
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
	httpd_resp_send(req, (const char *)payload, strlen(payload));

	if (strcmp(s_wifi_name, "\0") != 0 && strcmp(s_wifi_password, "\0") != 0) {
		// xSemaphoreGive(ap_sem);
		ESP_LOGD(TAG, "set wifi name and password successfully! goto station mode");
	}

	s_report_provision_status(PROVISION_STATUS_AP_CREDENTIALS_SUBMIT, "the credentials for AP is submitted by STA!",
							  payload);
	_six_iot_conn_to_field_ap_and_bind_device((char *)s_wifi_name, (char *)s_wifi_password);
	return ESP_OK;
}

// the end point defined to receive the AP credentials from the app
httpd_uri_t wifi_provision_credentials_post_page = {.uri = "/provision/credentials",
													.method = HTTP_POST,
													.handler = s_wifi_provision_credentials_post_page_handler,
													.user_ctx = NULL};

// the handler for the "status" endpoint when the device act as soft ap
static esp_err_t s_wifi_provision_status_page_handler(httpd_req_t *req) {
	int size = 30 + (NULL == s_bind_device_result ? 0 : strlen(s_bind_device_result));
	char *payload = calloc(size, sizeof(char));
	if (NULL != s_bind_device_result) {
		sprintf(payload, "{\"status\":%d,\"data\":%s}", s_six_iot_conn_status, s_bind_device_result);
	} else {
		sprintf(payload, "{\"status\":%d}", s_six_iot_conn_status);
	}
	ESP_LOGD(TAG, "payload is %s\n", payload);
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
	httpd_resp_send(req, (const char *)payload, strlen(payload));
	free(payload);
	return ESP_OK;
}

httpd_uri_t wifi_provision_status_page = {
	.uri = "/provision/status", .method = HTTP_GET, .handler = s_wifi_provision_status_page_handler, .user_ctx = NULL};

static void s_six_iot_initialise_wifi(void) {
	static bool initialized = false;
	if (initialized) {
		return;
	}
	// esp_wifi_deinit();
	ESP_ERROR_CHECK(esp_netif_init());
	s_wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);

	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	// set the event handler for the WIFI_EVENT and IP_EVENT
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_wifi_event_handler, NULL,
														&instance_any_id));
	ESP_ERROR_CHECK(
		esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &_ip_event_handler, NULL, &instance_got_ip));

	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
	ESP_ERROR_CHECK(esp_wifi_start());

	initialized = true;
}

void _six_iot_wifi_init_softap(wifi_ap_config_t *config) {
	/*
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret ==
	ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ret =
	nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	*/
	//	esp_wifi_disconnect ();
	//	esp_wifi_stop ();
	//	esp_wifi_deinit ();

	// initialize the WIFI common configuration
	s_six_iot_initialise_wifi();

	// set the soft AP config
	wifi_config_t wifi_config = {.ap = *config};

	if (strlen((char *)wifi_config.ap.password) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	// SET APSTA as the mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	// SET the connection config
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

	// esp_wifi_disconnect();
	// start the WIFI(in AP mode)
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGD(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", wifi_config.ap.ssid,
			 wifi_config.ap.password, wifi_config.ap.channel);
	s_report_provision_status(PROVISION_STATUS_INIT_DEVICE_IN_SOFTAP_SUCCEED,
							  "initialize device as softAP is successful!", NULL);
	s_httpd_server = _six_iot_start_webserver_for_conn_field_ap();
}

void six_iot_start_softap_mode(six_iot_config_t *iot_config, wifi_ap_config_t *config,
										  esp_event_loop_handle_t loop_handle) {
	s_six_iot_loop = loop_handle;
	s_iot_cfg = iot_config;
	s_report_provision_status(PROVISION_STATUS_START_DEVICE_IN_SOFTAP_MODE, "start the device in softAP mode", NULL);
	_six_iot_wifi_init_softap(config);
}

httpd_handle_t _six_iot_start_webserver_for_conn_field_ap() {
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.lru_purge_enable = true;

	// Start the httpd server
	ESP_LOGD(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		// Set URI handlers
		ESP_LOGD(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &wifi_provision_index_page);
		httpd_register_uri_handler(server, &wifi_provision_credentials_post_page);
		httpd_register_uri_handler(server, &wifi_provision_status_page);
		s_report_provision_status(PROVISION_STATUS_WEB_SERVER_STARTED,
								  "web server is started to receive the request from STA!", server);

		return server;
	}
	s_report_provision_status(PROVISION_STATUS_WEB_SERVER_START_FAIL, "web server fail to start!", NULL);
	ESP_LOGD(TAG, "Error starting server!");
	return NULL;
}

void _six_iot_stop_ap_web_server() { httpd_stop(s_httpd_server); }
