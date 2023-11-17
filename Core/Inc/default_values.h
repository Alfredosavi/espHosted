/*
 * default_values.h
 *
 *  Created on: Nov 12, 2023
 *      Author: Alfredo Savi
 */

#ifndef SRC_DEFAULT_VALUES_H_
#define SRC_DEFAULT_VALUES_H_

#include "ctrl_api.h"



#define DV_INIT_VERSION					1

#define DV_ESP_OPERATING_MODE			WIFI_MODE_APSTA

#define DV_WIFI_SOFTAP_SSID				"ESP_WIFI"
#define DV_WIFI_SOFTAP_PWD				"12345678"
#define DV_WIFI_SOFTAP_CHANNEL			1
#define DV_WIFI_SOFTAP_ENCRYPTION		WIFI_AUTH_WPA2_PSK
#define DV_WIFI_SOFTAP_MAX_CONN			2
#define DV_WIFI_SOFTAP_SSID_HIDDEN		SSID_BE_BROADCASTED
#define DV_WIFI_SOFTAP_BANDWIDTH		WIFI_BW_HT20

#define DV_WIFI_STATION_IS_WPA3_SUPP	false


#endif /* SRC_DEFAULT_VALUES_H_ */
