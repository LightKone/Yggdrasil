//
// Created by João Leitão on 2019-06-09.
//

#ifndef YGGDRASIL_DATA_STRUCT_H
#define YGGDRASIL_DATA_STRUCT_H

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WLAN_ADDR_LEN 6

/*************************************************
 * Structure to configure the network device
 ************************************************/
typedef enum {
    TCP,
    UDP
}connection_type;

#pragma pack(1)
// Structure for holding a mac address
typedef struct _WLANAddr{
    // address
    unsigned char data[WLAN_ADDR_LEN];
} WLANAddr;

// Structure for holding ip:port msg addresses
typedef struct _IPAddr {
    char addr[INET_ADDRSTRLEN];
    unsigned short port;
} IPAddr;

#pragma pack()

typedef struct _interface {
    int id;
    char* name;
    int type;
    struct _interface* next;
} Interface;

typedef enum _network_type {
    MAC,
    IP
}network_type;

typedef struct _Channel {
    network_type type;
    // socket descriptor
    int sockid;
    // interface index
    int ifindex;
    // mac address
    WLANAddr hwaddr;
    // maximum transmission unit
    int mtu;
    //ip address (if any)
    IPAddr ip;
} Channel;

/*************************************************
 * NetworkConfig
 *************************************************/

typedef struct _IpNetworkConfig {
    IPAddr ip; //ip address for listen socket
    connection_type sock_type; //TCP / UDP
    int keepalive; // yes/no activate keepalive socket option
    int max_pending_connections; //max pending connections to accept
    //TODO other socket options
}IpNetworkConfig;

typedef struct _MacNetworkConfig {
    int type; //type of the required network (IFTYPE)
    int freq; //frequency of the signal
    int nscan; //number of times to perform network scans
    short mandatoryName; //1 if must connect to named network,; 0 if not
    char* name; //name of the network to connect
    struct sock_filter* filter; //filter for the network
    Interface* interfaceToUse; //interface to use

}MacNetworkConfig;

typedef union _NetworkConfigs {
    MacNetworkConfig macntconf;
    IpNetworkConfig ipntconf;
}net_conf;

typedef struct __NetworkConfig{
    network_type type;
    net_conf config;
}NetworkConfig;

NetworkConfig* defineIpNetworkConfig(const char* ip_addr, unsigned short port, connection_type connection, int max_pending_connections, int keepalive);

/*************************************************
 * Standart method return values
 *************************************************/
#define SUCCESS 1
#define FAILED 0

/*************************************************
 * Global Vars
 *************************************************/
int lk_error_code;

#endif //YGGDRASIL_DATA_STRUCT_H

