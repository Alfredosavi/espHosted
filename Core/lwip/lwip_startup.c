/*
 * teste_startup.c
 *
 *  Created on: Oct 10, 2023
 *      Author: Alfredo Savi
 */

#include "lwip_startup.h"

#include "lwip/init.h"

#include <lwip/tcpip.h>
#include <lwip/netif.h>
#include <lwip/ip.h>
#include <lwip/ip4_addr.h>
#include <lwip/prot/ethernet.h>
#include <lwip/prot/etharp.h>
#include <lwip/dhcp.h>
#include <lwip/snmp.h>
#include <lwip/etharp.h>
#include <string.h>
#include <assert.h>

#include "netdev_api.h"
#include "spi_drv.h"
#include "app_main.h"
#include "httpd.h"



struct netif gnetif_station;
struct netif gnetif_ap;

ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;



ip4_addr_t *get_ip_station_address()
{
	return netif_ip4_addr(&gnetif_station);
}


err_t ethernetif_init_low(struct netif *netif)
{
	//lwip init wifi phy
	return 0;
}


#if (MAIN_APP_CODE == LWIP_DEMO)
struct network_handle *sta_handle, *ap_handle;


static void sta_rx_callback(struct network_handle *net_handle)
{
	struct esp_pbuf *rx_buffer = NULL;

	rx_buffer = network_read(net_handle, 0);

	if (rx_buffer) {
		struct pbuf *p = pbuf_alloc(PBUF_RAW, rx_buffer->len, PBUF_POOL);
//		  memcpy(p->payload, rx_buffer->payload, rx_buffer->len);

		struct pbuf *q;
		uint32_t l = 0;

		if(p != NULL){
			for(q=p; q != NULL; q=q->next){
				memcpy(q->payload, &rx_buffer->payload[l], q->len);
				l = l + q->len;
			}
		}

		if(gnetif_station.input(p, &gnetif_station) != ERR_OK)
			pbuf_free(p);

		free(rx_buffer->payload);
		rx_buffer->payload = NULL;
		free(rx_buffer);
		rx_buffer = NULL;
	}
}


err_t myif_link_station_output(struct netif *netif, struct pbuf *p)
{
	struct pbuf *q;

	int packet_size = 0;
	for(q=p; q != NULL; q=q->next)
		packet_size += q->len;


	int buffer_offset = 0;
	struct esp_pbuf *snd_buffer = NULL;

	snd_buffer = malloc(sizeof(struct esp_pbuf));
	assert(snd_buffer);

	snd_buffer->payload = malloc(packet_size);
	assert(snd_buffer->payload);

	for(q=p; q != NULL; q=q->next){
		memcpy(snd_buffer->payload + buffer_offset, q->payload, q->len);
		buffer_offset += q->len;
	}


	snd_buffer->len = packet_size;
//	printf("Sending %d bytes to station\r\n", snd_buffer->len);
	int ret = network_write(sta_handle, snd_buffer);

	if(ret)
		printf("%s: Failed to send packet\n\r", __func__);

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}


void lwip_startup_station(uint8_t mac_address[6])
{
	/* Initialize the LwIP stack with RTOS */
//	tcpip_init(NULL, NULL);

//	LWIP_MEMPOOL_INIT(RX_POOL);

	sta_handle = network_open(STA_INTERFACE, sta_rx_callback);
	assert(sta_handle);

	/* set MAC hardware address length */
	gnetif_station.hwaddr_len = ETHARP_HWADDR_LEN;

	/* set MAC hardware address */
	memcpy(gnetif_station.hwaddr, mac_address, NETIF_MAX_HWADDR_LEN);

	/* IP addresses initialization with DHCP (IPv4) */
	ip4_addr_set_zero(&ipaddr);
	ip4_addr_set_zero(&netmask);
	ip4_addr_set_zero(&gw);

	/* add the network interface (IPv4/IPv6) with RTOS */
	netif_add(&gnetif_station, &ipaddr, &netmask, &gw, NULL,
			ethernetif_init_low, tcpip_input);

	gnetif_station.linkoutput = myif_link_station_output;
//	gnetif_station.output = myif_output;
	gnetif_station.output = etharp_output;
	gnetif_station.hwaddr_len = 6;
	gnetif_station.mtu = 1500;
	gnetif_station.name[0] = 's';
	gnetif_station.name[1] = 't';
	gnetif_station.flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	gnetif_station->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	/* Registers the default network interface */
	netif_set_default(&gnetif_station);

	if(netif_is_link_up(&gnetif_station)){
		/* When the netif is fully configured this function must be called */
		netif_set_up(&gnetif_station);
	}else{
		/* When the netif link is down this function must be called */
		netif_set_down(&gnetif_station);
	}

	/* Start DHCP negotiation for a network interface (IPv4) */
	dhcp_start(&gnetif_station);
}



void lwip_example_app_platform_assert(const char *msg, int line,
		const char *file) {
	printf("Assertion \"%s\" failed at line %d in %s\n", msg, line, file);
	fflush(NULL);
	osDelay(1000);
	abort();
}

// ======================================================================================
// ======================================================================================
static void ap_rx_callback(struct network_handle *net_handle)
{
	struct esp_pbuf *rx_buffer = NULL;

	rx_buffer = network_read(net_handle, 0);

	if (rx_buffer) {
		struct pbuf *p = pbuf_alloc(PBUF_RAW, rx_buffer->len, PBUF_POOL);
//		  memcpy(p->payload, rx_buffer->payload, rx_buffer->len);

		struct pbuf *q;
		uint32_t l = 0;

		if(p != NULL){
			for(q=p; q != NULL; q=q->next){
				memcpy(q->payload, &rx_buffer->payload[l], q->len);
				l = l + q->len;
			}
		}

		if(gnetif_ap.input(p, &gnetif_ap) != ERR_OK)
			pbuf_free(p);

		free(rx_buffer->payload);
		rx_buffer->payload = NULL;
		free(rx_buffer);
		rx_buffer = NULL;
	}
}


err_t myif_link_ap_output(struct netif *netif, struct pbuf *p)
{
	struct pbuf *q;

	int packet_size = 0;
	for(q=p; q != NULL; q=q->next)
		packet_size += q->len;


	int buffer_offset = 0;
	struct esp_pbuf *snd_buffer = NULL;

	snd_buffer = malloc(sizeof(struct esp_pbuf));
	assert(snd_buffer);

	snd_buffer->payload = malloc(packet_size);
	assert(snd_buffer->payload);

	for(q=p; q != NULL; q=q->next){
		memcpy(snd_buffer->payload + buffer_offset, q->payload, q->len);
		buffer_offset += q->len;
	}


	snd_buffer->len = packet_size;
//	printf("Sending %d bytes to station\r\n", snd_buffer->len);
	int ret = network_write(ap_handle, snd_buffer);

	if(ret)
		printf("%s: Failed to send packet\n\r", __func__);

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}


void lwip_startup_ap(uint8_t mac_address[6])
{
	/* Initialize the LwIP stack with RTOS */
	tcpip_init(NULL, NULL);

//	LWIP_MEMPOOL_INIT(RX_POOL);

	ap_handle = network_open(SOFTAP_INTERFACE, ap_rx_callback);
	assert(ap_handle);

	/* set MAC hardware address length */
	gnetif_ap.hwaddr_len = ETHARP_HWADDR_LEN;

	/* set MAC hardware address */
	memcpy(gnetif_ap.hwaddr, mac_address, NETIF_MAX_HWADDR_LEN);

	/* IP addresses initialization with DHCP (IPv4) */
//	ip4_addr_set_zero(&ipaddr);
//	ip4_addr_set_zero(&netmask);
//	ip4_addr_set_zero(&gw);

	// TODO: IP DO AP FIXO
	IP4_ADDR(&ipaddr, 192, 168, 2, 1);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 2, 1);


	/* add the network interface (IPv4/IPv6) with RTOS */
	netif_add(&gnetif_ap, &ipaddr, &netmask, &gw, NULL,
			ethernetif_init_low, tcpip_input);

	gnetif_ap.linkoutput = myif_link_ap_output;
//	gnetif_ap.output = myif_output;
	gnetif_ap.output = etharp_output;
	gnetif_ap.hwaddr_len = 6;
	gnetif_ap.mtu = 1500;
	gnetif_ap.name[0] = 'a';
	gnetif_ap.name[1] = 'p';
	gnetif_ap.flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	gnetif_ap->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	/* Registers the default network interface */
	netif_set_default(&gnetif_ap);

	if(netif_is_link_up(&gnetif_ap)){
		/* When the netif is fully configured this function must be called */
		netif_set_up(&gnetif_ap);
	}else{
		/* When the netif link is down this function must be called */
		netif_set_down(&gnetif_ap);
	}
}
// ======================================================================================
// ======================================================================================
#endif
