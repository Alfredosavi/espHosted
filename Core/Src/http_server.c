/*
 * http_server.c
 *
 *  Created on: Out 11, 2023
 *      Author: Alfredo Savi
 */

#include "http_server.h"

#include "stdint.h"
#include "string.h"
#include "stdio.h"

#include "main.h"

#include "mod_interface_wifi.h"
#include "mod_persist.h"

#include "httpd.h"
#include "lwip/def.h"

#define SSI_TAGS_NUM	(uint8_t)4



hs_ssi_parameters_t ssi_params = {0};
persist_payload_t	cgi_params = {0};
const char* SSI_TAGS[] = {"ssid_ap", "view_ap", "ssid_sta", "conn_sta"};



uint16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen)
{
	get_data_ssi(&ssi_params);

	switch(iIndex){
		case 0: // ssid_ap
			strcpy(pcInsert, (char *)ssi_params.ssid_ap);
			return strlen(pcInsert);
		break;


		case 1: // view_ap (Se está visivel ou não)
			if(ssi_params.ap_ssid_is_hidden == SSID_BE_BROADCASTED)
				strcpy(pcInsert, "SSID vísivel");
			else
				strcpy(pcInsert, "SSID oculto");

			return strlen(pcInsert);
		break;


		case 2: // ssid_sta
			strcpy(pcInsert, (char *)ssi_params.ssid_sta);
			return strlen(pcInsert);
		break;


		case 3: // conn_sta (Se está conectado ou não)
			if(get_status_conn_ap())
				strcpy(pcInsert, "Conectado");
			else
				strcpy(pcInsert, "Desconectado");

			return strlen(pcInsert);
		break;


		default:
			return strlen(pcInsert);
		break;
	}

	return 0;
}


// =========================== CGI HANDLER =================================
const char *CGIForm_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

const tCGI FORM_CGI[] = {
	{
		"/config_ap",
		CGIForm_Handler
	},
	{
		"/config_sta",
		CGIForm_Handler
	}
};



const char *CGIForm_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
	if(iIndex == 0){ // CONFIG AP
		for(int i=0; i<iNumParams; i++){
			if(!strcmp(pcParam[i], "ssid_ap")){
				strcpy((char *)cgi_params.ap_conf.ssid, pcValue[i]);

			}else if(!strcmp(pcParam[i], "pwd_ap")){
				strcpy((char *)cgi_params.ap_conf.pwd, pcValue[i]);

			}else if(!strcmp(pcParam[i], "show_ssid_ap")){
				if(*pcValue[i] == '0')
					cgi_params.ap_conf.ssid_hidden = SSID_BE_BROADCASTED;
				else
					cgi_params.ap_conf.ssid_hidden = SSID_NOT_BE_BROADCASTED;
			}
		}
		set_data_cgi(&cgi_params, UPDATE_AP);

	}else if(iIndex == 1){ // CONFIG STA
		for(int i=0; i<iNumParams; i++){
			if(!strcmp(pcParam[i], "ssid_sta")){
				strcpy((char *)cgi_params.st_conf.ssid, pcValue[i]);

			}else if(!strcmp(pcParam[i], "pwd_sta")){
				strcpy((char *)cgi_params.st_conf.pwd, pcValue[i]);
			}
		}
		set_data_cgi(&cgi_params, UPDATE_STA);

	}else{
		return "/404.html";
	}

	return "/success.html";
}




void http_server_init(void)
{
	http_set_ssi_handler(ssi_handler, SSI_TAGS, SSI_TAGS_NUM);
	http_set_cgi_handlers(FORM_CGI, LWIP_ARRAYSIZE(FORM_CGI));
}
