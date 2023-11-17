/*
 * mod_persist.c
 *
 *  Created on: Nov 12, 2023
 *      Author: Alfredo Savi
 */

#include "mod_persist.h"

#include "string.h"



void init_values_default(persist_backup_t *context);



bool mod_persist_init(persist_backup_t *context)
{
	bool result = true;


	// LOGICA PARA RECUPERAR DADOS ...

	init_values_default(context);

	return result;
}


void mod_persist_run(persist_backup_t *context)
{
	// Implementar Logica para verificar updates
}


void init_values_default(persist_backup_t *context)
{
	// Zerando a estrutura
	uint8_t *tmpPointer = (uint8_t *)context;
	for(uint16_t i=0; i<sizeof(persist_backup_t); i++)
		tmpPointer[i] = 0;


	// Configuração da estrutura de backup
	context->version = DV_INIT_VERSION;


	// Configuração da estrutura SOFT AP
	memcpy(context->payload.ap_conf.ssid, DV_WIFI_SOFTAP_SSID, sizeof(DV_WIFI_SOFTAP_SSID));
	memcpy(context->payload.ap_conf.pwd, DV_WIFI_SOFTAP_PWD, sizeof(DV_WIFI_SOFTAP_PWD));

	context->payload.ap_conf.channel = DV_WIFI_SOFTAP_CHANNEL;
	context->payload.ap_conf.encryption_mode = DV_WIFI_SOFTAP_ENCRYPTION;
	context->payload.ap_conf.max_connections = DV_WIFI_SOFTAP_MAX_CONN;
	context->payload.ap_conf.ssid_hidden = DV_WIFI_SOFTAP_SSID_HIDDEN;
	context->payload.ap_conf.bandwidth = DV_WIFI_SOFTAP_BANDWIDTH;

	// Configuração da estrutura STATION
	context->payload.st_conf.is_wpa3_supported = DV_WIFI_STATION_IS_WPA3_SUPP;
}
