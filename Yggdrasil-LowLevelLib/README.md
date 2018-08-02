# Yggdrasil Low Level Library

This Library is used to configure the device so that it can support the desired mode.

In this version only supports the configuration for AdHoc mode in Wireless in Linux based systems.

To do this it uses the functions and data types defined in linux/nl80211.h and in the netlink library.

This project tries to abstract the user (programmer) to the issues of hand configuring the device.

The code written in based on the implementation of the iw linux tool. The code can be found in:
https://github.com/Distrotech/iw


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


