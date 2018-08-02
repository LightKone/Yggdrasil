# Yggdrasil

Yggdrasil is used to develop protocols that run in wireless environments.
This project is build upon Yggdrasil-LowLevelLib which is used to configure and manipulate the network devices.

All of the protocols defined in the project are tied together by the Yggdrasil runtime (ygg_runtime).
The runtime initializes 3 protocols by default, the dispatcher protocol, the timer protocol, and the protocol executor.

1. Dispatcher protocol : This will be used to send and receive messages from the network.
2. Timer protocol : This will manage all timers (either unique or periodic) set within the framework.
3. Protocol executor : This will manage protocols that a share a simple execution thread.


The runtime registers each protocol and handles the interation between them.
These interations are done by blocking queues. Each protocol has associated to it a queue which it uses to receive events by other protocols.

The elements present in these queues are queue_t_elem which can be of 4 different types:
YggMessage, YggTimer, YggEvent, YggRequest.

A definition of these structures can be found in:
```
./core/proto_data_struct.h
```
 
The queue have priorities for each element type, which is defined by the order described in enum queue_t_elemeent_type_ in:
```
./core/utils/queue_elem.h
```

The current priority is:

1. YggTimer
2. YggEvent
3. YggMessage
4. YggRequest


### Configuring an application

In Yggdrasil, an application can be composed of three different kinds of components:

1. Yggdrasil protocols
2. User defined protocols
3. Application components

Yggdrasil protocols are protocols provided by yggdrasil itself. As of this version only the dispatcher, timer, protocol executor, and the topology manager protocol (defined in ./protocols/utility) are defined as being Yggdrasil Protocols.
Yggdrasil protocols are designed to provide the most basic and fundamental functionalities to any applications.

User defined protocols are protocols that provide specific functionalities to applications. These are the protocols that one should develop with Yggdrasil. As of now, Yggdrasil provides a variety of user defined protocols that can be found in the directory protocols.

Application components are components that are not necessarly protocols, but are essencial parts of applications.

To incorporate Yggdrasil within a main application, the runtime must be configured.

First the runtime should be initialized by calling the following function:

```c
/**
 * Initialize the runtime to hold the protocols the applications need to run
 * @param ntconf The configuration of the environment
 * @return SUCCESS if successful; FAILED otherwise
 */
int ygg_runtime_init(NetworkConfig* ntconf);
```

Then, each yggdrasil protocol, user defined protocol, and application component that is necessary for the operation of the desired application should be registered within the runtime. This can be achieved with the following functions:

```c
/**
 * Register a yggdrasil protocol in the runtime
 * @param proto_id The yggdrasil protocol ID
 * @param protocol_init_function The yggdrasil protocol initialization function
 * @param proto_args The yggdrasil protocol specific parameters
 * @return SUCCESS if all checks out, FAILED otherwise
 */
short registerYggProtocol(short proto_id, Proto_init protocol_init_function, void* proto_args);

/**
 * Register a user defined protocol in the runtime
 * @param proto_id The user defined protocol ID
 * @param protocol_init_function The user defined protocol initialization function
 * @param proto_args The user defined protocol specific parameters
 * @return SUCCESS if all checks out, FAILED otherwise
 */
short registerProtocol(short proto_id, Proto_init protocol_init_function, void* proto_args);

/**
 * Register an application component in the runtime
 * @param application_definition The application component definition
 * @return The application's event queue
 */
queue_t* registerApp(app_def* application_definition);
```

Once everything is set up, one should start the runtime, starting all the registered protocols.

```c
/**
 * Start all of the registered protocols
 * @return This operations always succeeds, if there are errors they will be logged for later analysis
 */
int ygg_runtime_start();
```


### Designing Protocols

In Yggdrasil protocols are event driven. Events are consumed from the protocol's queue, and pushed to other protocols' queues.

Protocols can be defined in two ways:

1. Following an independent execution model:
	* In this the protocol is configured by the runtime to run on an independ execution thread
2. Following a shared execution model:
	* In this the protocol is configured by the runtime to run withing the context of the execution of the protocol executor.
	
To configure the protocol to run in one model or the other, protocols are required to have a an initialization function, where they create a protocol definition. This definition will contain all the necessary information that is required for the runtime to configure and execute the protocol correctly.

As an example:

```c
proto_def* protocol_init(void* args) {
	protocol_state* state = malloc(sizeof(protocol_state)); //the protocol's state
	... //initialize the protocol's state
	
	proto_def* protocol_definition = create_protocol_definition(protocol_id, protocol_name, state, protocol_destroy_function); //create a protocol definition
	
	proto_def_add_produced_events(protocol_definition, number_of_produced_notifications); 
	proto_def_add_consumed_event(protocol_definition, producer_protocol_id, notification_id);
	if(shared_execution_model) {
		proto_def_add_msg_handler(protocol_definition, process_msg);
		proto_def_add_timer_handler(protocol_definition, process_timer);
		proto_def_add_event_handler(protocol_definition, process_event);
		proto_def_add_request_handler(protocol_definition, process_request);
	} else
		proto_def_add_protocol_main_loop(protocol_definition, protocol_main_loop);
	
	...//other initializations required
	
	return protocol_definition
}
```

Handlers are used for the shared execution model, the protocol's main loop is used for the independent execution model.
More examples can be found in the provided implemented protocols.

### Useful functions

To manage the interactions between protocols, the runtime provides the following functions:

```c

/**
 * Send a message to the network
 * @param msg The message to be sent
 * @return This operation always succeeds
 */
int dispatch(YggMessage* msg); //dispatch a message to the network

/**
 * Send a message to the network (sending it directly to the dispatcher queue)
 * This functions should only be used by control processes
 * @param msg The message to be sent
 * @return This operation always succeeds
 */
int directDispatch(YggMessage* msg);

/**
 * Setup a timer in timer protocol
 * @param timer The timer to be set
 * @return This operation always succeeds
 */
int setupTimer(YggTimer* timer);

/**
 * Cancel an existing timer.
 * @param timer The timer to be cancelled
 * @return This operation always succeeds
 */
int cancelTimer(YggTimer* timer);

/**
 * Deliver a message to the protocol identified in the message
 * @param msg The message to be delivered
 * @return This operation always succeeds
 */
int deliver(YggMessage* msg);

/**
 * Verifies if the message destination is the process by verifying the destination's mac address,
 * If the mac address belongs to the process or it is the broadcast address, the message is delivered
 * to the protocol identified in the message
 * @param msg The message to be delivered
 * @return This operation always succeeds
 */
int filterAndDeliver(YggMessage* msg);

/**
 * Deliver a timer to the protocol identified in the timer
 * @param timer The timer to be delivered
 * @return This operation always succeeds
 */
int deliverTimer(YggTimer* timer);

/**
 * Deliver an event to all interested protocols in the event
 * @param event The event to be delivered
 * @return This operation always succeeds
 */
int deliverEvent(YggEvent* event);

/**
 * Deliver a request to a protocol
 * @param req The request to be delivered
 * @return If the YggRequest is a request returns SUCCESS, if not returns FAILED
 */
int deliverRequest(YggRequest* req);

/**
 * Deliver a reply to a protocol
 * @param res The reply to be delivered
 * @return If the YggRequest is a request returns SUCCESS, if not returns FAILED
 */
int deliverReply(YggRequest* res);
```
