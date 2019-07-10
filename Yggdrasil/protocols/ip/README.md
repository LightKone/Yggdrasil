# Protocols for IP networks

Here you can find protocols for IP networks implemented using the abstractions provided by Yggdrasil.

We provide a brief description of the implemented protocols.

## Data Transfer Management Protocols
Under `data_transfer/` you can find 3 flavours of protocols to manage the transfer of files:
1. `simple_data_transfer.c`
1. `block_data_transfer.c`
1. `block_data_transfer_mem_opt.c`

### Simple Data Transfer
This protocol reads a given file to memory and performs a dissemination request to a dissemination protocol. 

The bytes of the file will be copied to the payload of a disseminated message. This however, limits the protocol to only be able to request file dissemination that do not exceed the maximum unsigned short value in bytes.

### Block Data Transfer
This protocol addresses the limitations of the previous protocol, by reading blocks (of a page size `~4Kb`) of the file, and issuing multiple file dissemination requests, each containing a block of the file.

The protocol reconstructs the file in disk, upon reception, by calculating the offsets of the received file block. 

#### Block Data Transfer Memory Optimized
The previous protocol will issue *n* dissemination request of size at most `~4Kb` which will be stored in a dissemination protocol if need be to recuperate lost messages.

In addition to the previous protocol, this one interplays with a dissemination protocol (or any other protocol) by providing an additional request interface.
 
 This interface allows another protocol to request the contents of a given message (in this case the file block) when needed.


## Alternative Dispatchers
Under `dispatcher/` you can find 2 alternative dispatcher protocols for Yggdrasil:
1. `multi_tcp_socket_dispatcher.c`
1. `simple_tls_dispatcher.c`

### Multi TCP Socket Dispatcher

A dispatcher that performs asynchronous sends and receives on large messages (by spawning threads). 

To do so, the protocol assumes that for any given destination of a large message, a second connection already exists. 

In other words, the protocol maintains inbound and outbound connections similarly to the default Yggdrasil Dispatcher Protocol.

Existing inbound connections are then used to send asynchronous large messages, and existing outbound connections are used to receive asynchronous large messages.

The protocol guarantees FIFO ordering on asynchronous large messages per destination, by queueing large messages whenever an asynchronous send is ongoing. 

### Simple TLS  Dispatcher

The default Yggdrasil Dispatcher Protocol for IP networks enriched with TLS sockets provided by the openssl library.


## Dissemination Protocols
Under `dissemination/` you can find 5 flavours of dissemination protocols:
1. `flood.c`
1. `flood_flow_control.c`
1. `plumtree.c`
1. `plumtree_mem_optimized.c`
1. `plumtree_mem_optimized_flow_control.c`

All these protocols assume the existence of a membership protocol to feed information regarding the neighbourhood.

### Flood
This protocol disseminates a requested message to all of its neighbourhood.

Upon reception of a message, the protocol forwards it if it is a new message to all neighbours except to the sender of the received message. 


#### Flood with Flow Control
This protocols enriches the previous protocol with a simple flow control mechanism.

Messages are queued in a pipeline, and must be acknowledged by every recipient. Once acknowledged by all, the next message in the pipelined is sent.  

### PlumTree

This protocol builds a dissemination tree among the different nodes in the system. The tree is built by the natural operation of the protocol by logically removing redundant links between nodes.

For more information on this protocol, please refer to the original [paper](http://asc.di.fct.unl.pt/~jleitao/pdf/srds07-leitao.pdf).


#### PlumTree Memory Optimized
The PlumTree protocol utilizes "graft" messages to recover the tree when needed. Responses to graft messages contain the disseminated message, which requires the protocol to store full information of received messages. However, storing a large collection of large messages is suboptimal. 

To address this, this protocol optimizes the management of the collection of received messages, by only storing information relevant for the identification of disseminated messages. This information is then used to request the message contents to the protocol which requested the dissemination (or was delivered to).

In other words, only the message owner protocol contains the contents of the message, as opposed of both the message owner protocol and the dissemination protocol containing the message contents.

 
#### PlumTree Memory Optimized with Flow Control
This protocol enriches the previous one with a simple flow control mechanism similar to the flood with flow control protocol. The exception is that answers to graft messages are also stored in the pipelined with a specific destination address.

    
## Membership Protocols
Under `membership/` you can find 2 flavours of membership protocols:
1. `hyparview.c`
1. `xbot.c`

### HyParView

The HyParView protocol builds a stable random overlay topology in the system. 


For more information on this protocol, please refer to the original [paper](http://asc.di.fct.unl.pt/~jleitao/pdf/dsn07-leitao.pdf).

### X-Bot

The X-Bot protocol operates similarly to the HyParView protocol with the addition of optimization rounds between groups of 4 nodes.

The protocol tries to optimize the resulting random topology to a random topology whose link cost is minimal. To achieve this, the protocol relies on a companion oracle protocol (that operates as ping-pong protocol) to obtain information on the RTT between pairs of nodes.

For more information on this protocol, please refer to the original [paper](http://asc.di.fct.unl.pt/~jleitao/pdf/srds09-leitao.pdf).

## Other Utility Protocols
Under `utility/` you can find other utility protocols that may be used by the above protocols.