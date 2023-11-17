/*
 * lwip_startup.h
 *
 *  Created on: Oct 10, 2023
 *      Author: Alfredo Savi
 */

#ifndef LWIP_LWIP_STARTUP_H_
#define LWIP_LWIP_STARTUP_H_

#include <stdint.h>
#include <lwip/ip_addr.h>

void lwip_startup_station(uint8_t mac_address[6]);
void lwip_startup_ap(uint8_t mac_address[6]);

ip4_addr_t *get_ip_station_address();

#endif /* LWIP_LWIP_STARTUP_H_ */
