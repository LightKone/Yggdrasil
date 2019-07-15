# Yggdrasil Low Level Library


This Library supports the Yggdrasil Runtime in configuring the device with the desired communication mode. 

Yggdrasil now offers 2 modes for communication:
1. Wireless Ad Hoc
1. TCP/IP

These communication modes are exposed to the Yggdrasil Runtime as a *channel* abstraction. 

### Wireless Ad Hoc
In this mode the radio device itself is reconfigured to operate in ad hoc mode. 

To do this we use the functions and data types defined in [linux/nl80211.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/nl80211.h) and in the [netlink library](https://www.infradead.org/~tgr/libnl/doc/core.html).

This Library tries to abstract the user (programmer) to the issues of hand configuring the device.

The code written in based on the implementation of the iw linux tool. The code can be found in:
[https://github.com/Distrotech/iw](https://github.com/Distrotech/iw)

The code that handles this can be found [here](src_wireless).

**NOTE:** In this mode, communication is performed via message exchange directly at the MAC layer (Layer 2 of the OSI stack).

### TCP/IP

In this mode a simple listening socket is configure through where it is possible to accept incoming connections.


### Demos

We offer 5 demos to test the basic functionalities of this Library over Wireless.
These demos are Sender and Receiver applications. In order to test connectivity they should be used in pairs.

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
