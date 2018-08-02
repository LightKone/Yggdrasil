# Yggdrasil

The Yggdrasil is used to develop protocols that run in wireless environments.
This project is build upon Yggdrasil-LowLevelLib which is used to configure and manipulate the network devices.

All of the protocols defined in the project are tied together by the lk_runtime.
The runtime initialized 2 protocols by default, the dispatcher protocol and the timer protocol.

The dispatcher protocol is used to send and receive messages from the network.
The dispatcher will set the origin address of a message, and send it to the network. 
The dispatcher will receive messages that are destined to him or to the broadcast address.
The destination is set by the one who created the message.

The timer protocol is used to setup and handle timers.
 
This runtime registers each protocol and handles the interation between them.
These interations are done by blocking queues. Each protocol has associated to it a queue which it uses to receive events by other protocols.

The elements present in these queues are queue_t_elem which can be of 4 different types of data in it:
LKMessage, LKTimer, LKEvent, LKRequest.

A definition of these structures can be found in:
```
./core/proto_data_struct.h
```
 
The queue have priorities for each element type, which is defined by the order described in enum queue_t_elemeent_type_ in:
```
./core/utils/queue_elem.h
```

The current priority is:

1. LKTimer
2. LKEvent
3. LKMessage
4. LKRequest


### Configuring the runtime

To run the protocols there should be a main app which will configure the runtime.

This is done in 3 steps: Init, Config and Start.

In the Init fase, the program should send the configuration of the required network, the number of lightkone protocols, the number of user defined protocols and the number of applications will be registered in the runtime.
This is done by calling the function:
```c
/**
 * Initialize the runtime to hold the protocols the applications need to run
 * @param ntconf The configuration of the environment
 * @param n_lk_protos The number of lightkone protocols, not counting the dispatcher and the timer
 * @param n_protos The number of user defined protocols
 * @param n_apps The number of Apps
 * @return SUCCESS if successful; FAILED otherwise
 */
int lk_runtime_init(NetworkConfig* ntconf, int n_lk_protos, int n_protos, int n_apps);
```
Once this is done, each protocol and app can be register by calling:
```c
/**
 * Register a lightkone protocol in the runtime
 * @param protoID The lightkone protocol ID
 * @param protoFunc The lightkone protocol main function
 * @param protoAttr The lightkone protocol specific parameters
 * @param max_event_type The number of events the lightkone protocol has
 * @return SUCCESS if all checks out and if there is still space for lightkone protocols, FAILED otherwise
 */
int addLKProtocol(short protoID, void * (*protoFunc)(void*), void* protoAttr, int max_event_type);

/**
 * Register a user defined protocol in the runtime
 * @param protoID The user defined protocol ID
 * @param protoFunc The user defined protocol main function
 * @param protoAttr The user defined protocol specific parameters
 * @param max_event_type The number of events the user defined protocol has
 * @return SUCCESS if all checks out and if there is still space for user define protocols, FAILED otherwise
 */
int addProtocol(short protoID, void * (*protoFunc)(void*), void* protoAttr, int max_event_type);

/**
 * Register an application in the runtime
 * @param app_inBox The application queue
 * @param max_event_type The number of events the application has
 * @return The application ID
 */
short registerApp(queue_t** app_inBox, int max_event_type);
```
Once all protocols and apps are registered the Init fase is complete. The Init fase will initialize all the queues and protocol parameters needed to function.

The Config fase, is used to define interactions between protocols. One can configure protocols to act in behalf of other protocols by changing the queue reference.
This is done by calling the function:
```c
/**
 * Change the queue reference of the protocol identified by protoID with the queue reference identified by myID
 * @param protoID The protocol of which the queue should be changed
 * @param myID The protocol to which the queue will be changed
 * @return The queue reference of protoID
 */
queue_t* interceptProtocolQueue(short protoID, short myID);
```
If this function is used then the protocol that intercepts the queue of another protocol will be responsible for the other protocols queue. As such, it is highly advised to update the protocol paramenters so that the intercepted queue is passed as a parameter.
This can be done by calling:
```c
/**
 * Update the protocol specific parameters
 * @param protoID The protocol ID
 * @param protoAttr The protocol specific parameters to be updated
 * @return SUCCESS if the protocol was registered, FAILED otherwise
 */
int updateProtoAttr(short protoID, void* protoAttr);
```
The Config fase is also used to register the events in which a protocol is interested.
In order to do this:
```c
/**
 * Regist the events to which a protocol is interested
 * @param protoID The protocol ID who has the events
 * @param events A list of interested event IDs
 * @param nEvents The number of interested events
 * @param myID The protocol which is interested in the events
 * @return This operation always succeeds, if the events do not exist it will simply not register the interest
 */
int registInterestEvents(short protoID, short* events, int nEvents, short myID);
```
After all the configurations are set, the Config fase is finished and the Start fase can begin.

The Start fase will simply launch all of the protocols that were registered in the order they were registered.
This is done by calling the function:
```c
/**
 * Start all of the registered protocols
 * @return This operations always succeeds, if there are errors they will be logged for later analysis
 */
int lk_runtime_start();
```
The applications registered are of the resposibility of the programmer and the runtime will not lunch threads for them if they are needed.

### Designing Protocols


We consider that which protocol is run by a thread, as such which protocol must have main function defined as:
```c
	void * proto_name_init( void * args )
```	
which will be used to create a thread.
The args parameter will always be of type proto_args, where proto_args is defined as:
```c
	//Arguments for a protocol to launch
	typedef struct _proto_args {
		queue_t* inBox; //the respective protocol queue
		unsigned short protoID; //the respective protocol ID
		uuid_t myuuid; //the uuid of the process
		void* protoAttr; //protocol specific arguments
	}proto_args;
```
This struture will pass the protocol queue (inBox) which the protocol must use to receive requests given by other protocols.
The structure will also pass protoAttr which should be, if needed, defined in the designed protocol.

The defined protocol should run an infinite loop where it will pop an element from the its' queue, verify which type of element it is and process it accordingly to the protocol logic. 

For the protocol to interact with the other protocols it should use the runtime as its interactor.
This is done by the functions:
```c
/**
 * Send a message to the network
 * @param msg The message to be sent
 * @return This operation always succeeds
 */
int dispatch(LKMessage* msg); //dispatch a message to the network

/**
 * Setup a timer
 * @param timer The timer to be set
 * @return This operation always succeeds
 */
int setupTimer(LKTimer* timer); //setup a timer

/**
 * Deliver a message to the protocol identified in the message
 * @param msg The message to be delivered
 * @return This operation always succeeds
 */
int deliver(LKMessage* msg);

/**
 * Verifies if the message destination is the process by verifying
 * the destination's mac address,
 * If the mac address belongs to the process or it is the broadcast
 * address, the message is delivered
 * to the protocol identified in the message
 * @param msg The message to be delivered
 * @return This operation always succeeds
 */
int filterAndDeliver(LKMessage* msg);

/**
 * Deliver a timer to the protocol identified in the timer
 * @param timer The timer to be delivered
 * @return This operation always succeeds
 */
int deliverTimer(LKTimer* timer);

/**
 * Deliver an event to all interested protocols in the event
 * @param event The event to be delivered
 * @return This operation always succeeds
 */
int deliverEvent(LKEvent* event);
```

If the protocol is acting in the behalf of another protocol then the protocol can make use of the functions:

```c
int pushPayload(LKMessage* msg, char* buffer, unsigned short len, short protoID, WLANAddr* newDest);
int popPayload(LKMessage* msg, char* buffer, unsigned short readlen);
```

These functions allow to send piggybacked data.
