/*
 * mod_interface_wifi.h
 *
 *  Created on: Sep 11, 2023
 *      Author: Alfredo Savi
 */

#ifndef INC_MOD_INTERFACE_WIFI_H_
#define INC_MOD_INTERFACE_WIFI_H_

#include "stdint.h"
#include "stdbool.h"

#include "mod_persist.h"

void mod_interface_wifi_init(void);
void mod_interface_wifi_run(persist_backup_t *context);
bool get_status_conn_ap(void);



#endif /* INC_MOD_INTERFACE_WIFI_H_ */
