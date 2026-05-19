/*
 * six_iot_shadow.h
 *
 *  Created on: 2025年8月3日
 *      Author: Stephen Yu
 */

#ifndef __SIX_IOT_SHADOW_H__
#define __SIX_IOT_SHADOW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <cJson.h>

//   shadowTopic: '+/+/shadow'
//   shadowLwtTopic: '+/+/shadow/lwt'
//   shadowUpdateTopic: '+/+/shadow/update'
//   shadowDesiredTopic: '+/+/shadow/desired'
//   shadowReportedTopic: '+/+/shadow/reported'
//   shadowPingTopic: '+/+/shadow/ping'
//   shadowGetTopic: '+/+/shadow/get'
//   shadowOtaTopic: '+/+/shadow/ota'
//   shadowUpdateAcceptedTopic: 'shadow/update/accepted'
//   shadowDesiredAcceptedTopic: 'shadow/desired/accepted'
//   shadowReportedAcceptedTopic: 'shadow/reported/accepted'
//   shadowPingAcceptedTopic: 'shadow/ping/accepted'
//   shadowGetAcceptedTopic: 'shadow/get/accepted'

typedef enum {
	Shadow,
	Lwt,
	Update,
	Ping,
	Get,
	Ota,
	Log,
	UpdateAccepted,
	UpdateDelta,
	PingAccepted,
	GetAccepted,
} six_iot_shadow_topic_t;

/**
 * Parse the shadow status
 */
char *six_iot_parse_shadow_status(cJSON *json);

/**
 * Parse String attribute from the shadow JSON
 */
char *six_iot_parse_shadow_str_attr(cJSON *json, char *attrName);

/**
 * Parse Object attribute from the shadow JSON
 */
cJSON *six_iot_parse_shadow_obj_attr(cJSON *json, char *attrName);

/**
 * Get the shadow topic
 */
char *six_iot_get_shadow_topic(char *productId, char *deviceGuid, six_iot_shadow_topic_t topic);

/**
 * Get general topic
 */
char *six_iot_get_general_topic(char *productId, char *deviceGuid, char *topic);

#ifdef __cplusplus
}
#endif

#endif /* __SIX_IOT_MQTT_H__ */
