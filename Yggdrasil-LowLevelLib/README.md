# Yggdrasil Low Level Library

The Lightkone-Base project is used to configure the device so that it can support the desired mode.

In this version the Lightkone-Base project only supports the configuration for AdHoc mode in Wireless in Linux based systems.

To do this it uses the functions and data types defined in linux/nl80211.h and in the netlink library.

This project tries to abstract the user (programmer) to the issues of hand configuring the device.

The code written in based on the implementation of the iw linux tool. The code can be found in:
https://github.com/Distrotech/iw

To do so, the project provides a simple API for setup:
```c
*********************************************************
 * Setup
 *********************************************************/

int setupSimpleChannel(Channel* ch, NetworkConfig* ntconf);
int setupChannelNetwork(Channel* ch, NetworkConfig* ntconf);
```

These 2 functions configure a Channel stucture:
```c
typedef struct _Channel {
    // socket descriptor
    int sockid;
    // interface index
    int ifindex;
    // mac address
    WLANAddr hwaddr;
    // maximum transmission unit
    int mtu;
} Channel;
```
Given the configuration defined in the data structure:
```c
typedef struct _NetworkConfig {
	int type; //type of the required network (IFTYPE)
	int freq; //frequency of the signal
	int nscan; //number of times to perform network scans
	short mandatoryName; //1 if must connect to named network,; 0 if not
	char* name; //name of the network to connect
	struct sock_filter* filter; //filter for the network
	Interface* interfaceToUse; //interface to use
} NetworkConfig;
```
The setupSimpleChannel function will analyse if there is a valid and available wireless network interface for the desired network defined in the NetworkConfig.

If there is one, it will succeed and create a Channel by initializing a raw socket and associate it to the Channel.

The setupChannelNetwork will first scan the enviroment to check if there is already a network being anounced with the defined requirements in NetworkConfig.

If there is it will join the network, if not it will create a new network. To end the operation it will bind the socket in the Channel. 

The Channel is then ready be used for basic I/O operations (send/receive).
For that the API also provides with these functions:
```c
/*********************************************************
 * Basic I/O
 *********************************************************/

/**
 * Send a message through the channel to the destination defined
 * in the message
 * @param ch The channel
 * @param message The message to be sent
 * @return The number of bytes sent through the channel
 */
int chsend(Channel* ch, LKPhyMessage* message);

/**
 * Send a message through the channel to the given address
 * @param ch The channel
 * @param message The message to be sent
 * @param addr The mac address of the destination
 * @return The number of bytes sent through the channel
 */
int chsendTo(Channel* ch, LKPhyMessage* message, char* addr);

/**
 * Send a message through the channel to the broadcast address
 * (one hop broadcast)
 * @param ch The channel
 * @param message The message to be sent
 * @return The number of bytes sent through the channel
 */
int chbroadcast(Channel* ch, LKPhyMessage* message);

/**
 * Receive a message through the channel
 * @param ch The channel
 * @param message The message to be received
 * @return The number of bytes received
 */
int chreceive(Channel* ch, LKPhyMessage* message);
```

All these functions use the LKPhyMessage data structure, which is defined as:
```c
// Lightkone Protocol message
typedef struct _LKPhyMessage{
  //Physical Level header;
  WLANHeader phyHeader;
  //Lightkone Protocol header;
  LKHeader lkHeader;
  //PayloadLen
  unsigned short dataLen;
  //Payload
  char data[MAX_PAYLOAD];
} LKPhyMessage;

// Structure of a frame header
typedef struct _WLANHeader{
    // destination address
    WLANAddr destAddr;
    // source address
    WLANAddr srcAddr;
    // type
    unsigned short type;
} WLANHeader;

typedef struct _WLANAddr{
   // address
   unsigned char data[WLAN_ADDR_LEN];
} WLANAddr;

// Structure of a Lightkone message
typedef struct _LKHeader{
   // Protocol family identifation
   unsigned char data[LK_HEADER_LEN];
} LKHeader;
```

The following code describes how to create a new LKPhyKMessage:
```c
	LKMessage msg;
	msg.LKProto = ProtocolID; //the protocol ID who created the message
	msg.phyHeader.type = IP_TYPE; 
	setDestToBroadcast(&msg); or setDestToAddr(&message, addr); //Set the destination to broadcast or a specific mac address
	char id[] = AF_LK_ARRAY; //define the message as a message of the Lightkone protocol
	memcpy(msg.lkHeader.data, id, LK_HEADER_LEN); 
	msg.dataLen = dataLengt;
	memcpy(msg.data, payloadToSend, msg.dataLen); //add the payload

```
Note also that when sending a message through the channel completes the source address.

### Demos

This project offers 5 demos to test the basic functionalities of the project.
These demos are Sender and Receiver applicatios. In order to test connectivity they should be used in pairs.

We provide the following demos:
	
* Sender
* Receiver
* SenderBrute
* SenderWithReply
* ReceiverWithReply

The Sender applications sends a message each two seconds.
The SenderBrute has a closed loop sending messages.
The Receiver simple receives the message and prints it.

The SenderWithReply sends a message and waits for two seconds for the reply.
The ReceiverWithReply receives a message and sends a reply to the sender.


