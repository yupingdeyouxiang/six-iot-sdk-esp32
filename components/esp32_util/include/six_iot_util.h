#ifndef __SIX_IOT_UTIL_H__
#define __SIX_IOT_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <mbedtls/base64.h>

char *base64_encode_urlsafe(const unsigned char *input, size_t input_len);

/**
 * Parse String attribute from JSON
 */
char *six_parse_json_str_attr(cJSON *json, char *attrName);

/**
 * Parse Object attribute from JSON
 */
cJSON *six_parse_json_obj_attr(cJSON *json, char *attrName);

/**
 * Check the WiFi quality by RSSI value
*/
void six_check_wifi_quality(const char *tag);

/**
 * Take the heap snapshot
*/
void six_memory_snapshot(const char* tag);

/**
 * Defragment heap to consolidate the heap for some operation e.g. WSS connection.
 * 
*/
void six_defregment_heap(const char* tag);

/**
 * Sign the base64 encoded payload with the RSA private key to generate the JWT signature
 */
char* sign_data_with_rsa_private_key(char *base64_payload, char* private_key);

/**
 * Parse the `openId` (or `openid`/`sub`) claim from a JWT id_token.
 * Returns a malloc'd string containing the claim value, or NULL on failure.
 * Caller must free the returned string.
 */
char *six_iot_parse_openid_from_jwt(const char *idtoken);

#ifdef __cplusplus
}
#endif
#endif /* __SIX_IOT_UTIL_H__ */