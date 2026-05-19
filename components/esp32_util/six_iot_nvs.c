#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include "six_iot_nvs.h"

static const char *TAG = "six_iot_nvs";

/**
 * @brief Initializes the NVS flash partition.
 */
esp_err_t six_nvs_init_storage() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_PART_NOT_FOUND || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was not found or is corrupted, erase and re-init
		ESP_LOGW(TAG, "NVS partition corrupted or not found. Erasing and re-initializing...");
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	return err;
}

/**
 * @brief Saves an access token to NVS.
 */
esp_err_t six_nvs_save_access_token(const char *token) {
	return six_nvs_save_str(NVS_TOKEN_NAMESPACE, NVS_ACCESS_TOKEN_KEY, token);
}

/**
 * @brief Saves an access token to NVS.
 */
esp_err_t six_nvs_save_id_token(const char *token) {
	return six_nvs_save_str(NVS_TOKEN_NAMESPACE, NVS_ID_TOKEN_KEY, token);
}

/**
 * @brief Saves the jwt signed by device private key to NVS.
 *
 * @param token The jwt string to save.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_save_device_jwt(const char *jwt) {
	return six_nvs_save_str(NVS_TOKEN_NAMESPACE, NVS_DEVICE_JWT_KEY, jwt);
}

/**
 * save the string, meanwhile save the length of the string
 */
esp_err_t six_nvs_save_str(const char *namespace, const char *key, const char *value) {
	nvs_handle_t nvs_handle;
	esp_err_t err;

	// Open NVS handle for our namespace
	err = nvs_open(namespace, NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error opening NVS handle (%s)!", esp_err_to_name(err));
		return err;
	}

	// Write the access token
	err = nvs_set_str(nvs_handle, key, value);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to write string to NVS (%s)!", esp_err_to_name(err));
		nvs_close(nvs_handle);
		return err;
	}

	// Commit changes to flash
	err = nvs_commit(nvs_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to commit NVS changes (%s)!", esp_err_to_name(err));
		nvs_close(nvs_handle);
		return err;
	}

	nvs_close(nvs_handle);
	ESP_LOGD(TAG, "Value of %s saved to NVS successfully.", key);
	return ESP_OK;
}

char *six_nvs_read_token(const char *namespace, const char *token_key) {
	nvs_handle_t nvs_handle;
	esp_err_t err;

	// Open NVS handle for our namespace
	err = nvs_open(namespace, NVS_READONLY, &nvs_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Error opening NVS handle (%s)!", esp_err_to_name(err));
		return NULL;
	}

	size_t required_size;
	err = nvs_get_str(nvs_handle, token_key, NULL, &required_size);
	if (err != ESP_OK) {
		if (err == ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGW(TAG, "Token key '%s' not found in NVS.", token_key);
		} else {
			ESP_LOGE(TAG, "Failed to get size for key '%s' with err: (%s)!", token_key, esp_err_to_name(err));
		}
		nvs_close(nvs_handle);
		return NULL;
	}

	char *token = (char *)malloc(required_size);
	if (token == NULL) {
		ESP_LOGE(TAG, "Failed to allocate memory for token!");
		nvs_close(nvs_handle);
		return NULL;
	}

	// Read the access token
	err = nvs_get_str(nvs_handle, token_key, token, &required_size);
	if (err != ESP_OK) {
		if (err == ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGW(TAG, "Token not found in NVS.");
		} else {
			ESP_LOGE(TAG, "Failed to get size for key '%s' with err: (%s)!", esp_err_to_name(err), token_key);
		}
		nvs_close(nvs_handle);
		free(token);
		return NULL;
	}

	nvs_close(nvs_handle);
	ESP_LOGD(TAG, "Read value of '%s' from NVS successfully.", token_key);
	return token;
}

/**
 * @brief Reads an access token from NVS.
 */
char *six_nvs_read_access_token() { return six_nvs_read_token(NVS_TOKEN_NAMESPACE, NVS_ACCESS_TOKEN_KEY); }

/**
 * @brief Reads an access token from NVS.
 */
char *six_nvs_read_id_token() { return six_nvs_read_token(NVS_TOKEN_NAMESPACE, NVS_ID_TOKEN_KEY); }

/**
 * @brief Reads an jwt of device signed with its private key from NVS.
 * @return JWT in heap or NULL if not exist in NVS
 */
char *six_nvs_read_device_jwt() { return six_nvs_read_token(NVS_TOKEN_NAMESPACE, NVS_DEVICE_JWT_KEY); }