#ifndef SIX_IOT_NVS_H
#define SIX_IOT_NVS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>

// Define the NVS namespace and key for our token
#define NVS_TOKEN_NAMESPACE "credentials"
#define NVS_ACCESS_TOKEN_KEY "access_token"
#define NVS_ID_TOKEN_KEY "id_token"
#define NVS_DEVICE_JWT_KEY "device_jwt"

/**
 * @brief Initializes the NVS flash partition.
 * This must be called before any other NVS operations.
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_init_storage();

/**
 * @brief Saves an access token to NVS.
 *
 * @param token The access token string to save.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_save_access_token(const char *token);

/**
 * @brief Saves an id token to NVS.
 *
 * @param token The id token string to save.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_save_id_token(const char *token);

/**
 * @brief Saves the jwt signed by device private key to NVS.
 *
 * @param token The jwt string to save.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_save_device_jwt(const char *jwt);

/**
 * @brief Saves an key:value pair to NVS.
 *
 * @param key The key of string to save.
 * @param value The value of string to save.
 * @param len_key The key of string length to save.
 * @param len The length of the string.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t six_nvs_save_str(const char*namespace, const char* key, const char *value);

/**
 * @brief Reads an access token from NVS.
 * @return ACCESS TOKEN in heap or NULL if not exist in NVS
 */
char* six_nvs_read_access_token();

/**
 * @brief Reads an id token from NVS.
 * @return ID TOKEN in heap or NULL if not exist in NVS
 */
char* six_nvs_read_id_token();


/**
 * @brief Reads an jwt of device signed with its private key from NVS.
 * @return JWT in heap or NULL if not exist in NVS
 */
char* six_nvs_read_device_jwt();

#ifdef __cplusplus
}
#endif

#endif // SIX_IOT_NVS_H
