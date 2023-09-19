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
#include "lwip_startup.h"
#include "ctrl_api.h"
#include "util.h"
#include "httpd.h"
#include "spi_drv.h"


#include "lwiperf.h"



#define MIW_DEBUG_MODE			1 // @@@@ DEBUG @@@@
#define MIW_TEST_BAND_IPERF		1 // @@@@ SERVER IPERF @@@@

#define MIW_DELAY_INIT_ESP32	(uint16_t)10000
#define MIW_TIMEOUT_GET_MAC		(uint16_t)2000
#define MIW_TIMEOUT_GET_IP		(uint16_t)2000

#define MIW_MAC_OCTETS			(uint8_t)6


typedef enum{
	MIW_INIT = 0,
	MIW_RESET,
	MIW_INIT_HOST,
	MIW_WAIT_HOST,
	MIW_REQ_CONFIG_MAC,
	MIW_START_LWIP,
	MIW_GET_IP,
	MIW_DHCP_WAIT,
	MIW_HTTP_INIT,

	MIW_REQ_CONFIG_AP,
	MIW_START_LWIP_AP,
	MIW_VOID, // TODO: teste
}wifiState_e;


typedef struct{
	wifiState_e state;
	uint32_t 	timeMark;
	ctrl_cmd_t	req;
	ctrl_cmd_t	*res;
	ip4_addr_t 	*ip;
}wifiContext_t;



volatile wifiContext_t wifiContext = { 0 };



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



void mod_interface_wifi_run(void)
{
	switch(wifiContext.state)
	{
		case MIW_INIT:
			wifiContext.state = MIW_RESET;
		break;


		case MIW_RESET:
			uint8_t *tmpPointer = (uint8_t *)&wifiContext;

			for(uint16_t i=0; i<sizeof(wifiContext_t); i++)
				tmpPointer[i] = 0;
			wifiContext.state = MIW_INIT_HOST;
		break;


		case MIW_INIT_HOST:
			hosted_initialize();

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_WAIT_HOST;
		break;


		case MIW_WAIT_HOST:
			if(check_transaction_task())
				wifiContext.state = MIW_REQ_CONFIG_MAC;
			else if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_DELAY_INIT_ESP32)
				wifiContext.state = MIW_RESET;
		break;


		case MIW_REQ_CONFIG_MAC:
			wifiContext.req.msg_type = CTRL_REQ;
			wifiContext.req.u.wifi_mac.mode = WIFI_MODE_STA;
			wifiContext.req.ctrl_resp_cb = NULL;
			wifiContext.req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;

			wifiContext.res = NULL;

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_START_LWIP;
		break;


		case MIW_START_LWIP:
			wifiContext.res = wifi_get_mac(wifiContext.req);

			if(wifiContext.res->resp_event_status == SUCCESS){
				if(wifiContext.res->msg_id == CTRL_RESP_GET_MAC_ADDR){
					uint8_t parsed_mac[MIW_MAC_OCTETS] = {0};

					if(convert_mac_to_bytes(parsed_mac, wifiContext.res->u.wifi_mac.mac) == STM_OK){
						lwip_startup(parsed_mac);

						wifiContext.state = MIW_GET_IP;
					}else{
						wifiContext.state = MIW_RESET;
					}
				}
			}else if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_TIMEOUT_GET_MAC){
				wifiContext.state = MIW_RESET;
			}
		break;


		case MIW_GET_IP:
			wifiContext.ip = get_ip_address();

			wifiContext.timeMark = xTaskGetTickCount();
			wifiContext.state = MIW_DHCP_WAIT;
		break;


		case MIW_DHCP_WAIT:
			if(wifiContext.ip->addr != 0){

#if MIW_DEBUG_MODE
			uint8_t iptxt[32];
			sprintf((char*)iptxt,
					"[INFO] IP (DHCP): %d.%d.%d.%d",
					(uint8_t)(wifiContext.ip->addr),
					(uint8_t)((wifiContext.ip->addr) >> 8),
					(uint8_t)((wifiContext.ip->addr) >> 16),
					(uint8_t)((wifiContext.ip->addr) >> 24));
			printf("%s\n\r", iptxt);
#endif
			wifiContext.state = MIW_HTTP_INIT;
			}else{
				if((xTaskGetTickCount() - wifiContext.timeMark) > MIW_TIMEOUT_GET_IP)
					wifiContext.state = MIW_GET_IP;
			}
		break;


		case MIW_HTTP_INIT:
#if MIW_TEST_BAND_IPERF
			lwiperf_start_tcp_server(IP_ADDR_ANY, LWIPERF_TCP_PORT_DEFAULT, lwiperf_report, NULL);
#endif
//			httpd_init();
			wifiContext.state = MIW_VOID;
		break;


		case MIW_REQ_CONFIG_AP:
			wifiContext.req.ctrl_resp_cb = NULL;
			wifiContext.req.cmd_timeout_sec = DEFAULT_CTRL_RESP_TIMEOUT;
			wifiContext.req.u.wifi_mac.mode = WIFI_MODE_AP;

			strcpy((char *)wifiContext.req.u.wifi_softap_config.ssid, "ESPWIFI");
			strcpy((char *)wifiContext.req.u.wifi_softap_config.pwd, "12345678");
			wifiContext.req.u.wifi_softap_config.channel = 1;
			wifiContext.req.u.wifi_softap_config.encryption_mode = WIFI_AUTH_WPA2_PSK;
			wifiContext.req.u.wifi_softap_config.max_connections = 2;
			wifiContext.req.u.wifi_softap_config.ssid_hidden = 0;
			wifiContext.req.u.wifi_softap_config.bandwidth = WIFI_BW_HT20;

			wifiContext.res = wifi_start_softap(wifiContext.req);

			if(wifiContext.res && wifiContext.res->resp_event_status == SUCCESS){
				printf("MAC %s\n\r", wifiContext.res->u.wifi_softap_config.out_mac);
//				uint8_t parsed_mac[6] = {0x3c, 0x61, 0x05, 0x11, 0xd0, 0x05};

//					lwip_startup2(parsed_mac);
					wifiContext.state = MIW_VOID;
			}
		break;


		case MIW_START_LWIP_AP:
		break;


		case MIW_VOID:
		break;


		default:
			wifiContext.state = MIW_INIT;
		break;
	}
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
