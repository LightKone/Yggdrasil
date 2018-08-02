/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#ifndef CORE_RUNTIME_H_
#define CORE_RUNTIME_H_

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "core/utils/queue.h"
#include "core/utils/utils.h"
#include "Yggdrasil_lowlvl.h"

#include "core/proto_data_struct.h"

#define DEFAULT_QUEUE_SIZE 10

#define SIG SIGQUIT

/*********************************************************
 * Yggdrasil Protocol IDs
 *********************************************************/
typedef enum _PROTOID {
	PROTO_DISPATCH = 0,
	PROTO_TIMER = 1,
	PROTO_EXECUTOR = 2,
	PROTO_DISCOV = 3,
	PROTO_FAULT_DECT = 4,
	PROTO_SIMPLE_AGG = 5,
	PROTO_TCP_AGG_SERVER = 6,
	PROTO_TOPOLOGY_MANAGER = 7
} PROTOID;

#define PROTO_0 100 //begin ID for user defined protocols
#define APP_0 400 //begin ID for apps

/*********************************************************
 * Generic Events?
 *********************************************************/
#define ANY_PROTO -1

typedef enum _generic_events {
	START_PROTOS,
	RESTART_PROTOS,
	HALT_PROTOS,
	VALUE_CHANGE,
	MESSAGE_SENT,
	EVENTS_MAX
}generic_events;

/*********************************************************
 * The rest
 *********************************************************/
typedef struct _main_loop_args {
	queue_t* inBox;
	void* state;
}main_loop_args;

/********************************************************
 * Yggdrasil Defined types
 ********************************************************/

typedef short (*YggMessage_handler)(YggMessage*, void*);
typedef short (*YggTimer_handler)(YggTimer*, void*);
typedef short (*YggEvent_handler)(YggEvent*, void*);
typedef short (*YggRequest_handler)(YggRequest*, void*);

typedef struct _proto_def proto_def;
typedef struct _app_def app_def;

typedef proto_def* (*Proto_init)(void*);
typedef proto_def* (*Dispatcher_init)(Channel*, void*);

typedef short (*Proto_destroy)(void*);

typedef void* (*Proto_main_loop)(main_loop_args* args);

typedef void* (*Proto_arg_parser)(char* args);

/*********************************************************
 * Protocol Definition
 *********************************************************/


proto_def* create_protocol_definition(unsigned short proto_id, char* proto_name, void* state, Proto_destroy protocol_destroy_function);

void destroy_protocol_definition(proto_def* protocol_definition);

void proto_def_add_produced_events(proto_def* protocol_definition, int n_events);

void proto_def_add_consumed_event(proto_def* protocol_definition, short producer_id, int event);

void proto_def_add_protocol_main_loop(proto_def* protocol_definition, Proto_main_loop proto_main_loop);

void proto_def_add_msg_handler(proto_def* protocol_definition, YggMessage_handler handler);

void proto_def_add_timer_handler(proto_def* protocol_definition, YggTimer_handler handler);

void proto_def_add_event_handler(proto_def* protocol_definition, YggEvent_handler handler);

void proto_def_add_request_handler(proto_def* protocol_definition, YggRequest_handler handler);

YggMessage_handler proto_def_get_YggMessageHandler(proto_def* protocol_definition);

YggTimer_handler proto_def_get_YggTimerHandler(proto_def* protocol_definition);

YggEvent_handler proto_def_get_YggEventHandler(proto_def* protocol_definition);

YggRequest_handler proto_def_get_YggRequestHandler(proto_def* protocol_definition);

void* proto_def_getState(proto_def* protocol_definition);

short proto_def_getId(proto_def* protocol_definition);

/*********************************************************
 * Application Definition
 *********************************************************/


app_def* create_application_definition(int app_id, char* application_name);

void destroy_application_definition(app_def* application_definition);

void app_def_add_produced_events(app_def* application_definition, int n_events);

void app_def_add_consumed_events(app_def* application_definition, short producer_id, int event);


/*********************************************************
 * Init functions
 *********************************************************/

#include "core/protos/executor.h"
#include "core/protos/dispatcher.h"
#include "core/protos/timer.h"

/**
 * Initialize the runtime to hold the protocols the applications need to run
 * @param ntconf The configuration of the environment
 * @param n_ygg_protos The number of yggdrasil protocols, not counting the dispatcher and the timer
 * @param n_protos The number of user defined protocols
 * @param n_apps The number of Apps
 * @return SUCCESS if successful; FAILED otherwise
 */
int ygg_runtime_init(NetworkConfig* ntconf);
int ygg_runtime_init_static(NetworkConfig* ntconf);

/**
 * Register a yggdrasil protocol in the runtime
 * @param protoID The yggdrasil protocol ID
 * @param protoFunc The yggdrasil protocol main function
 * @param protoAttr The yggdrasil protocol specific parameters
 * @param max_event_type The number of events the yggdrasil protocol has
 * @return SUCCESS if all checks out and if there is still space for yggdrasil protocols, FAILED otherwise
 */
//int addYggProtocol(short protoID, void * (*protoFunc)(void*), void* protoAttr, int max_event_type);

short registerYggProtocol(short proto_id, Proto_init protocol_init_function, void* proto_args);

/**
 * Register a user defined protocol in the runtime
 * @param protoID The user defined protocol ID
 * @param protoFunc The user defined protocol main function
 * @param protoAttr The user defined protocol specific parameters
 * @param max_event_type The number of events the user defined protocol has
 * @return SUCCESS if all checks out and if there is still space for user define protocols, FAILED otherwise
 */
//int addProtocol(short protoID, void * (*protoFunc)(void*), void* protoAttr, int max_event_type);

short registerProtocol(short proto_id, Proto_init protocol_init_function, void* proto_args);

short pre_registerProtocol(short proto_id, Proto_init protocol_init_function, Proto_arg_parser proto_parser);

/**
 * Register an application in the runtime
 * @param app_inBox The application queue
 * @param max_event_type The number of events the application has
 * @return The application ID
 */
//short registerApp(queue_t** app_inBox, int max_event_type);

queue_t* registerApp(app_def* application_definition);

/*********************************************************
 * Config
 *********************************************************/

/**
 * Change the dispatcher protocol main loop function (dispatcher init)
 * @param dispatcher function pointer to new dispatcher main loop
 * @return always returns SUCCESS
 */
//int changeDispatcherFunction(void * (*dispatcher)(void*));

short overrideDispatcherProtocol(Dispatcher_init dispatcher_init_function, void* proto_args);

/**
 * Update the protocol specific parameters
 * @param protoID The protocol ID
 * @param protoAttr The protocol specific parameters to be updated
 * @return SUCCESS if the protocol was registered, FAILED otherwise
 */
//int updateProtoAttr(short protoID, void* protoAttr);

/**
 * Change the queue reference of the protocol identified by protoID with the queue reference identified by myID
 * @param protoID The protocol of which the queue should be changed
 * @param myID The protocol to which the queue will be changed
 * @return The queue reference of protoID
 */
queue_t* interceptProtocolQueue(short protoID, short myID);

/**
 * Regist the events to which a protocol is interested
 * @param protoID The protocol ID who has the events
 * @param events A list of interested event IDs
 * @param nEvents The number of interested events
 * @param myID The protocol which is interested in the events
 * @return This operation always succeeds, if the events do not exist it will simply not register the interest
 */
//int registInterestEvents(short protoID, short* events, int nEvents, short myID);

/*********************************************************
 * Start
 *********************************************************/

/**
 * Start all of the registered protocols
 * @return This operations always succeeds, if there are errors they will be logged for later analysis
 */
int ygg_runtime_start();

int startProtocol(short id, char* proto_args);
int stopProtocol(short id);

/*********************************************************
 * Runtime Functions
 *********************************************************/

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

/*********************************************************
 * Other useful read Functions
 *********************************************************/
/**
 * Gets the mac address of the process
 * @param s2 A char array to where the mac address is copied to
 * @return A char array with the mac address
 */
char* getMyAddr(char* s2);
WLANAddr* getMyWLANAddr();

/**
 * Fill the parameter addr with the mac address of the process
 * @param addr A pointer to the WLANAddr that was requested
 */
void setMyAddr(WLANAddr* addr);

/**
 * Returns the raspberry number as a double.
 * This function is used for testing purposes that need distributed values across devices
 * It returns the two last characters as a double value
 * @return A double value
 */
double getTestValue();

const char* getChannelIpAddress();

void getmyId(uuid_t id) ;

WLANAddr* getBroadcastAddr();

#endif /* CORE_RUNTIME_H_ */
