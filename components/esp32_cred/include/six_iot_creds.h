#ifndef __SIX_IOT_CREDS_H__
#define __SIX_IOT_CREDS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>

extern const char *CREDENTIALS_PARTITION;

char* get_device_private_key();
char* get_device_guid();
char* get_device_product_id();
char* get_network_prov_field(const char *field_name);

#ifdef __cplusplus
}
#endif

#endif
