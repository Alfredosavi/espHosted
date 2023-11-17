/*
 * mod_interface_wifi.c
 *
 *  Created on: Sep 11, 2023
 *      Author: Alfredo Savi
 */

#include "mod_interface_wifi.h"

#include "stdio.h"
#include "string.h"

#include "app_main.h"

#include "ctrl_api.h"
#include "spi_drv.h"
#include "util.h"

#include "lwip_startup.h"
#include "http_server.h"
#include "httpd.h"
#include "lwiperf.h"



#define MIW_DEBUG_MODE			1 // @@@@ DEBUG @@@@
#define MIW_TEST_BAND_IPERF		0 // @@@@ SERVER IPERF @@@@

#define MIW_DELAY_INIT_ESP32	(uint16_t)10000
#define MIW_DELAY_ESP			(uint16_t)5000
#define MIW_TIMEOUT_GET_IP		(uint16_t)5000
#define MIW_MAX_ATTEMPTS		(uint8_t)3

#define MIW_MAC_OCTETS			(uint8_t)6



typedef enum{
	MIW_INIT = 0,
	MIW_RESET,
	MIW_INIT_HOST,
	MIW_WAIT_HOST,
	MIW_WAIT_ESP,

	MIW_CONFIG,

	MIW_CONFIG_AP,
	MIW_START_LWIP_AP,
	MIW_HTTP_INIT_AP,

	MIW_WAIT_NEW_CONFIG,

	MIW_NEW_CONFIG_AP,

	MIW_CONFIG_STA,
	MIW_START_LWIP_STA,
	MIW_GET_IP_STA,
	MIW_DHCP_WAIT_STA,

	MIW_ERROR, // TODO: teste
}wifiState_e;



typedef struct{
	wifiState_e state;
	uint32_t 	timeMark;
	uint8_t		nAttempts;
	ctrl_cmd_t	*res;
	ip4_addr_t 	*ip;
}wifiContext_t;



persist_backup_t *context = {0};
volatile wifiContext_t wifiContext = {0};
uint8_t parsed_mac[MIW_MAC_OCTETS] = {0};



void miw_reset_context();
#if MIW_TEST_BAND_IPERF
void lwiperf_report(void *arg, enum lwiperf_report_type report_type,
                    const ip_addr_t *local_addr, uint16_t local_port,
                    const ip_addr_t *remote_addr, uint16_t remote_port,
					uint32_t bytes_transferred, uint32_t ms_duration, uint32_t bandwidth_kbitpsec);
#endif



void mod_interface_wifi_init(void)
{
	wifiContext.state = MIW_INIT;

#if MIW_DEBUG_MODE
	printf("[INFO] Init module wifi interface\n\r");
#endif
}



void mod_interface_wifi_run(persist_backup_t *context)
{
	ctrl_cmd_t req = {0};

	switch(wifiContext.state)
	{
		case MIW_INIT:
			miw_reset_context();

			wifiContext.state = MIW_INIT_HOST;
		break;


		case MIW_RESET:
			miw_reset_context();
			// FIXME: netif_init already exists
//			deinit_hosted_control_lib();

			wifiContext.state = MIW_INIT_HOST;
		break;


		case MIW_INIT_HOST:
			hosted_initialize();

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_WAIT_HOST;
		break;


		case MIW_WAIT_HOST:
			if(check_transaction_task()){
				wifiContext.timeMark = xTaskGetTickCount();

				wifiContext.state = MIW_WAIT_ESP;
			}else if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_DELAY_INIT_ESP32){
				wifiContext.state = MIW_RESET;
			}
		break;


		case MIW_WAIT_ESP:
			if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_DELAY_ESP)
				wifiContext.state = MIW_CONFIG;
		break;


		case MIW_CONFIG:
			req.msg_type = CTRL_REQ;
			req.u.wifi_mode.mode = DV_ESP_OPERATING_MODE;
			req.ctrl_resp_cb = NULL;
			req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;

			wifiContext.res = NULL;
			wifiContext.res = wifi_set_mode(req);

			if(wifiContext.res->resp_event_status == SUCCESS){

#if MIW_DEBUG_MODE
			printf("[+INFO] ESP32 configured as STA+AP\n\r");
#endif

				wifiContext.state = MIW_CONFIG_AP;
			}else{
				wifiContext.state = MIW_ERROR;
			}
		break;


		case MIW_CONFIG_AP:
			req.msg_type = CTRL_REQ;

			strcpy((char *)req.u.wifi_softap_config.ssid, (char *)context->payload.ap_conf.ssid);
			strcpy((char *)req.u.wifi_softap_config.pwd, (char *)context->payload.ap_conf.pwd);

			req.u.wifi_softap_config.channel = context->payload.ap_conf.channel;
			req.u.wifi_softap_config.encryption_mode = context->payload.ap_conf.encryption_mode;
			req.u.wifi_softap_config.max_connections = context->payload.ap_conf.max_connections;
			req.u.wifi_softap_config.ssid_hidden = context->payload.ap_conf.ssid_hidden;
			req.u.wifi_softap_config.bandwidth = context->payload.ap_conf.bandwidth;

			req.ctrl_resp_cb = NULL;
			req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;


			wifiContext.res = NULL;
			wifiContext.res = wifi_start_softap(req);

			if(wifiContext.res->resp_event_status == SUCCESS){
				if(convert_mac_to_bytes(parsed_mac, wifiContext.res->u.wifi_softap_config.out_mac) == STM_OK){

#if MIW_DEBUG_MODE
					printf("[+INFO] AP configuration success\n\r");
#endif
					wifiContext.state = MIW_START_LWIP_AP;
				}
			}else{
				wifiContext.state = MIW_ERROR;
			}
		break;


		case MIW_START_LWIP_AP:
			lwip_startup_ap(parsed_mac);
			wifiContext.state = MIW_HTTP_INIT_AP;
		break;


		case MIW_HTTP_INIT_AP:
			httpd_init();

#if MIW_DEBUG_MODE
			printf("[+INFO] Server HTTP up\n\r");
#endif


			http_server_init();

#if MIW_DEBUG_MODE
			printf("[+INFO] CGI and SSI up\n\r");
#endif


			wifiContext.state = MIW_WAIT_NEW_CONFIG;
		break;


		case MIW_WAIT_NEW_CONFIG:
			if((xTaskGetTickCount() - wifiContext.timeMark) > 500){
				wifiContext.timeMark = xTaskGetTickCount();

				if(context->newUpdate){
					if(context->newUpdate & UPDATE_AP){
						wifiContext.state = MIW_NEW_CONFIG_AP;
					}else if(context->newUpdate & UPDATE_STA){
						wifiContext.state = MIW_CONFIG_STA;
					}
				}
			}
		break;


		case MIW_NEW_CONFIG_AP:
			req.msg_type = CTRL_REQ;

			strcpy((char *)req.u.wifi_softap_config.ssid, (char *)context->payload.ap_conf.ssid);
			strcpy((char *)req.u.wifi_softap_config.pwd, (char *)context->payload.ap_conf.pwd);

			req.u.wifi_softap_config.channel = context->payload.ap_conf.channel;
			req.u.wifi_softap_config.encryption_mode = context->payload.ap_conf.encryption_mode;
			req.u.wifi_softap_config.max_connections = context->payload.ap_conf.max_connections;
			req.u.wifi_softap_config.ssid_hidden = context->payload.ap_conf.ssid_hidden;
			req.u.wifi_softap_config.bandwidth = context->payload.ap_conf.bandwidth;

			req.ctrl_resp_cb = NULL;
			req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;


			wifiContext.res = NULL;
			wifiContext.res = wifi_start_softap(req);

			if(wifiContext.res->resp_event_status == SUCCESS){

#if MIW_DEBUG_MODE
				printf("[+INFO] New AP configuration success\n\r");
#endif

				context->newUpdate &= ~(UPDATE_AP);
				context->version++;

				wifiContext.state = MIW_WAIT_NEW_CONFIG;
			}else{
				wifiContext.state = MIW_ERROR;
			}
		break;


		case MIW_CONFIG_STA:
			req.msg_type = CTRL_REQ;

			strcpy((char *)req.u.wifi_ap_config.ssid, (char *)context->payload.st_conf.ssid);
			strcpy((char *)req.u.wifi_ap_config.pwd, (char *)context->payload.st_conf.pwd);
			strcpy((char *)req.u.wifi_ap_config.bssid, (char *)context->payload.st_conf.bssid);
			req.u.wifi_ap_config.is_wpa3_supported = context->payload.st_conf.is_wpa3_supported;

			req.u.wifi_ap_config.listen_interval = 0;

			req.ctrl_resp_cb = NULL;
			req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;

			wifiContext.res = NULL;
			wifiContext.res = wifi_connect_ap(req);

			if(wifiContext.res->resp_event_status == SUCCESS){
				if(convert_mac_to_bytes(parsed_mac, wifiContext.res->u.wifi_ap_config.out_mac) == STM_OK)
					wifiContext.state = MIW_START_LWIP_STA;

			}else if(wifiContext.res->resp_event_status == CTRL_ERR_NO_AP_FOUND){

				if(wifiContext.nAttempts > MIW_MAX_ATTEMPTS){
					wifiContext.nAttempts = 0;
					wifiContext.state = MIW_ERROR; //TODO: aguardar novos dados
				}else{
					wifiContext.nAttempts++;
				}
			}else{
				wifiContext.state = MIW_ERROR;
			}
		break;


		case MIW_START_LWIP_STA:
			lwip_startup_station(parsed_mac);
			wifiContext.state = MIW_GET_IP_STA;
		break;


		case MIW_GET_IP_STA:
			wifiContext.ip = get_ip_station_address();

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_DHCP_WAIT_STA;
		break;


		case MIW_DHCP_WAIT_STA:
			if(wifiContext.ip->addr != 0){

#if MIW_DEBUG_MODE
			uint8_t iptxt[32];
			sprintf((char*)iptxt,
					"[+INFO] IP (DHCP): %d.%d.%d.%d",
					(uint8_t)(wifiContext.ip->addr),
					(uint8_t)((wifiContext.ip->addr) >> 8),
					(uint8_t)((wifiContext.ip->addr) >> 16),
					(uint8_t)((wifiContext.ip->addr) >> 24));
			printf("%s\n\r", iptxt);
#endif

			context->newUpdate &= ~(UPDATE_STA);
			context->version++;

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_WAIT_NEW_CONFIG;
			}else{
				if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_TIMEOUT_GET_IP){
					if(wifiContext.nAttempts > MIW_MAX_ATTEMPTS){
						wifiContext.nAttempts = 0;
						wifiContext.state = MIW_ERROR;
					}else{
						wifiContext.nAttempts++;
						wifiContext.state = MIW_GET_IP_STA;
					}
				}
			}
		break;


		case MIW_ERROR:
			if((xTaskGetTickCount() - wifiContext.timeMark) > 1000){
				wifiContext.timeMark = xTaskGetTickCount();
				#if MIW_DEBUG_MODE
							printf("[ERROR] mod_interface_wifi\n\r");
				#endif
			}
		break;


		default:
			wifiContext.state = MIW_INIT;
		break;
	}
}


void miw_reset_context()
{
	uint8_t *tmpPointer = (uint8_t *)&wifiContext;

	for(uint16_t i=0; i<sizeof(wifiContext_t); i++)
		tmpPointer[i] = 0;
}


bool get_status_conn_ap()
{
	bool result = false;
	ctrl_cmd_t	req;
	ctrl_cmd_t	*res;

	req.msg_type = CTRL_REQ;
	req.ctrl_resp_cb = NULL;
	req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;

	wifiContext.res = NULL;
	res = wifi_get_ap_config(req);

	if(res->resp_event_status == SUCCESS)
		result = true; // ESP está conectado em um AP
	else
		result = false; // ESP está desconectado do AP

	return result;
}



#if MIW_TEST_BAND_IPERF
void lwiperf_report(void *arg, enum lwiperf_report_type report_type,
                    const ip_addr_t *local_addr, uint16_t local_port,
                    const ip_addr_t *remote_addr, uint16_t remote_port,
					uint32_t bytes_transferred, uint32_t ms_duration, uint32_t bandwidth_kbitpsec)
{
#if MIW_DEBUG_MODE
	printf("=================================================\n\r");
    printf("RELATORIO IPERF:\n\r");
    printf("Tipo de Relatório: %d\n\r", report_type);
    printf("Endereço Local: %s\n\r", ipaddr_ntoa(local_addr));
    printf("Porta Local: %d\n\r", local_port);
    printf("Endereço Remoto: %s\n\r", ipaddr_ntoa(remote_addr));
    printf("Porta Remota: %d\n\r", remote_port);
    printf("Bytes Transferidos: %lu\n\r", bytes_transferred);
    printf("Duração (ms): %lu\n\r", ms_duration);
    printf("Largura de Banda (Kbit/s): %lu\n\r", bandwidth_kbitpsec);
    printf("=================================================\n\r");
#endif
}
#endif
