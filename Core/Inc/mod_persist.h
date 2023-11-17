/*
 * mod_persist.h
 *
 *  Created on: Nov 12, 2023
 *      Author: Alfredo Savi
 */

#ifndef SRC_MOD_PERSIST_H_
#define SRC_MOD_PERSIST_H_

#include "default_values.h"

#include "stdbool.h"



enum{
	NO_UPDATE  = 0,
	UPDATE_AP  = 1,
	UPDATE_STA = 2
};

typedef struct{
	uint8_t 			ssid[SSID_LENGTH];
	uint8_t 			pwd[PASSWORD_LENGTH];
	int 				channel;
	int 				encryption_mode;
	int 				max_connections;
	bool 				ssid_hidden;
	wifi_bandwidth_e 	bandwidth;
}dv_softap_config_t;


typedef struct{
	uint8_t 			ssid[SSID_LENGTH];
	uint8_t 			pwd[PASSWORD_LENGTH];
	uint8_t 			bssid[BSSID_LENGTH];
	bool 				is_wpa3_supported;
}dv_station_config_t;


typedef struct{
	dv_softap_config_t 	ap_conf;
	dv_station_config_t	st_conf;
}persist_payload_t;

typedef struct{
	uint16_t 			version;
	persist_payload_t	payload;
	uint8_t				newUpdate;
//	uint32_t 			crc;
}persist_backup_t;



bool mod_persist_init(persist_backup_t *context);
void mod_persist_run(persist_backup_t *context);


#endif /* SRC_MOD_PERSIST_H_ */
