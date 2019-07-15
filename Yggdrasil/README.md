# Yggdrasil



## Yggdrasil Runtime & Abstractions

The Yggdrasil Runtime provides the functionalities for protocols and applications to be able to execute cooperatively. This cooperation emerges from the processing of events within Yggdrasil.


### Yggdrasil Event Types

* `YggMessage` - Network Messages.
* `YggTimer` - Timers.
* `YggEvent` - Notifications.
* `YggRequest` - Requests/Replies.

More details can be found in their definition [here](./core/proto_data_struct.h).

Events have different priorities:
1. `YggTimer`
2. `YggEvent`
3. `YggMessage`
4. `YggRequest`

Can be changed by redefining the ordering of the [enum queue_t_element_type](core/utils/queue_elem.h).

### Yggdrasil Event Routing

Protocols and applications must be registered in the Yggdrasil Runtime for events to pass from one protocol to another.

This is achieved by 2 elements in Yggdrasil:
* Event queues - Upon registration the Yggdrasil Runtime will provide a  `queue_t` data structure, through where events from other protocols can be consumed.
* Protocol unique number - Protocols and Applications in Yggdrasil have a unique numeric to identify them. This enables the Runtime to route events through different protocols (events are tagged with protocol's unique numeric identifier). 

The Runtime provides functions to deliver each event type to their correct destination:

* `dispatch(YggMessage* msg)` - Send a message to the Network.
* `directDispatch(YggMessage* msg)` - Sent a message to directly to the the Dispatcher protocol to be sent to the Network.
* `deliver(YggMessage* msg)` - Deliver a Message to the protocol identified by the protocol identifier in the Message header.
* `filterAndDeliver(YggMessage* msg)` - Verify if the destination address is valid and then deliver the Message. 
* `setupTimer(YggTimer* timer)` - Setup a Timer event.
* `cancelTimer(YggTimer* timer)` - Cancel a Timer event.
* `deliverTimer(YggTimer* timer)` - Deliver the Timer event to the protocol identified by the protocol identifier in the Timer header.
* `deliverEvent(YggEvent* event)` - Deliver a Notification event to all interested parties.
* `deliverRequest(YggRequest* req)` - Deliver a Request event to the protocol identified by the protocol identifier in the Request header.
* `deliverReply(YggRequest* res)` - Deliver a Reply event to the protocol identified by the protocol identifier in the Reply header.

More information on these functions can be found in their definition [here](core/ygg_runtime.h).


### Implementing Protocols

Protocols in Yggdrasil are defined and implement in 3 parts.

A protocol contains a *state*, *event handlers*, and an *initialization function*.

#### State:
The state of a protocol in Yggdrasil is defined as *C* data structure:

```c
typedef struct state_ {
    short protocol_id;
    (... protocol maintained state) 
} state
```

The variable `protocol_id` should be maintained in the state to help initialize events produced by the protocol.

The rest of the variables in the state are protocol dependent. 

#### Event Handlers:

Protocol need to process events. Yggdrasil provides template functions for this purpose:

```c
short process_msg(YggMessage* msg, void* state) {
...
}

short process_timer(YggTimer* timer, void* state) {
...
}

short process_notification(YggEvent* event, void* state) {
...
}

short process_request(YggRequest* request, void* state) {
...
}
```

These functions always receive 2 arguments:
1. The event data structure they process.
1. The state of the protocol as a generic pointer.

Alternatively, protocols can also resort to a main-loop function to guide the processing of events:

```c
void protocol_main_loop(main_loop_args* args) {
    queue_t* inBox = args->inBox;
    void* state = args->state;
    
    while(1) { //loop forever consuming events from the queue and processing them accordingly
        queue_t_elem el;
        queue_pop(inBox, &el); // this will block until an event is present in the queue
        
        switch(el.type) { //switch the type of element retrieved from the queue
            case YGG_MESSAGE:
                process_msg(&el.data.msg, state);
                break;
            case YGG_TIMER:
                process_timer(&el.data.timer, state);
                break;
            case YGG_EVENT:
                process_notification(&el.data.event, state);
                break;
            case YGG_REQUEST:
                process_request(&el.data.request, state);
                break;
        }
        
        free_elem_payload(&el); //Free any allocated memory in the events
    }
}
```

> Developers can use their defined type for the state variable received in these functions to avoid having to cast the generic pointer.
>
> This will lead the C compiler to produce warning however. 

The two modes of defining event handling in a protocol ultimately define how the protocol will be executed at runtime. 

This property is defined upon protocol initialization.

#### Initialization:

The initialization function is responsible for initializing the protocol's state and to provide configuration information for the Yggdrasil Runtime.

The configuration information is stored in a special data structure `proto_def` that the function must return.

```c
proto_def* protocol_init(void* args) {
    state* st = malloc(sizeof(state)); //Allocate memory to hold the state
    st->protocol_id = PROTO_ID; //PROTO_ID is a constant defined in the protocol header file. Alternatively, this could be passed as argument.
    
    //Initialize the rest of the state
    ...
    
    
    //Create a protocol definition to provide protocol configuration information
    proto_def* proto = create_protocol_definition(PROTO_ID /* The protocol's numeric identifier */, "My protocol" /* The protocol's name */, st /* the protocol's state */, to_destroy /* A function to free the protocol's resources (may be NULL)*/ );
    
    
    //Set the protocol to execute in an independent execution thread
    proto_def_add_protocol_main_loop(proto, (Proto_main_loop) protocol_main_loop);
    
    //OR
    
    //Set the protocol to execute in an shared execution thread
    proto_def_add_msg_handler(proto, (YggMessage_handler) process_msg);
    proto_def_add_timer_handler(proto, (YggTimer_handler) process_timer);
    proto_def_add_event_handler(proto, (YggEvent_handler) process_notification);
    proto_def_add_request_handler(proto, (YggRequest_handler) process_request);
    

    //ADD how many notifications does the protocol produce
    proto_def_add_produced_events(proto, number_of_produced_notifications);
    
    //ADD which notifications does the protocol consumes
    proto_def_add_consumed_event(proto, producer_protocol_id, notification_id);
    proto_def_add_consumed_event(proto, producer_protocol_id, notification_id2);
    proto_def_add_consumed_event(proto, producer_protocol_id2, notification_id);
    
    
    //Perform other initialization actions (e.g., setup timers)
    ...  

    //Return the definition, for the Yggdrasil Runtime to process
    return proto;
}
```
> If both modes of execution are defined in the protocol definition, Yggdrasil will default to execute the protocol in an independent thread.

### Building Applications

We define as application, the piece of code that contains the main function.

Applications must first configure the Yggdrasil Runtime by providing a network configuration.

This can be either for IP or Wireless networks.

The Application then registers the protocols it will use, and register itself in the Runtime to obtain an event queue similarly to a protocol.

After all configurations are perform, the Application signals the Runtime to begin the execution of the protocols.

The  following piece of code exemplifies this: 

```c
int main(int argc, char* argv[]) {
    
    //Use traditional IP Networking
    NetworkConfig* ntconf = defineIpNetworkConfig("127.0.0.1"/*IP address of the process*/, 9000/*listing port*/, TCP/*TCP or UDP communication*/, 10/*pending accepts*/, 0/*Other options*/);

    //OR
    
    //Use wireless ad hoc Networking
    NetworkConfig* ntconf = defineWirelessNetworkConfig("AdHoc"/*Wireless Mode*/, 0/*Radio frequency 0 is default to 2412*/, 5/*Number of wireless scans*/, 0/*mandatory network name*/, "ledge"/*the network name*/, YGG_filter/*kernel filter to use*/);



    //Initialize the Yggdrasil Runtime with the given Network configuration
    ygg_runtime_init(ntconf);
    
    
    //Register protocol
    registerYggProtocol(PROTO_ID /*the protocol numeric identifier*/, protocol_init /*the protocol initialization function*/, protocol_args /*the protocol arguments*/);
    
    //register other protocols similarly
    ...
    
    
    //Register the Application
    short myId = 400;
    app_def* myapp = create_application_definition(myId, "MyApp");
    
    app_def_add_consumed_events(myapp, PROTO_ID, notification_id1);
    
    queue_t* inBox = registerApp(myapp);
	
    
    //Start Yggdrasil Runtime:
    ygg_runtime_start();
    
    
    //Execute your application code
    
    while(1) {
        queue_elem_t el;
        queue_pop(inBox, &el);
        switch(el.type) {
        ... // process events and run application code
        }
    }

}


```
> Protocols for each network are still incompatible. If you define a IP network, then you should use protocol that operate over IP networks. The same goes to Wireless.
> 
> This is because, messages for IP networks will use IP headers, whilst messages for Wireless will use MAC headers. The default Dispatcher protocol will go into an undefined state.
>
> UDP is not supported yet.

## Structure

In the following directories you may find:

* application_components - contains application components to be added to an application.
* applications - contains demo and test applications for both ip and wireless.
* core - contains the code for the Yggdrasil Runtime.
* interfaces - contains common interfaces for interactions between protocols
* protocols - contains distributed protocols for both ip and wireless.
* utils - contains other utilities that might be helpful (e.g., hash functions, bloom filters).

> Application components are defined as generic modules for application to deal with application dependent external behaviour (e.g., an interactive terminal; interaction with a non-Yggdrasil Service).

For simple examples on protocols and applications please find them here:
   * [simple wireless discovery protocol](protocols/wireless/discovery/simple_discovery.c)
   * [simple wireless discovery test application](applications/tests/wireless/simple_discovery_test.c)

> As of now, we do not have simple examples for protocols and applications in traditional IP networks.

Click the [link](applications/file%20transfer) to check an application for IP networks fully built using Yggdrasil.

Please find protocols for IP Networks [here](protocols/ip).

Please find protocols for Wireless Ad Hoc Networks [here](protocols/wireless).

## Next Steps:
- [ ] Add an additional header to the `YggMessage` stating its type.
- [ ] Protocol Numeric Identifiers with semantics.
- [ ] Add UPD communication *Channel*.
- [ ] Coexisting communication *Channels*.
- [ ] Allow different kernel filters in Messages (in low-level lib).
- [ ] Apply Patch for low-level receive for wireless.
- [ ] Uniform Interfaces for Protocol interactions.
- [ ] Coexisting IP and Wireless protocols.

In the meantime, we will also continue to improve and add more protocols.

This branch will eventually be merged with `master`, when stable.   

