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

/**********************************************************
 * This is Yggdrasil Runtime V0.1 (alpha) no retro-compatibility *
 * will be ensured across versions up to version 1.0      *
 **********************************************************/

#include "ygg_runtime.h"

typedef void* (*gen_function)(void*);

/**********************************************************
 * Protocol Definition
 **********************************************************/

typedef struct protocol_handlers_ {
	YggMessage_handler msg_handler;
	YggTimer_handler timer_handler;
	YggEvent_handler event_handler;
	YggRequest_handler request_handler;
}protocol_handlers;

struct _proto_def{
	unsigned short proto_id;
	char* proto_name;

	void* state;
	Proto_main_loop proto_main_loop;
	protocol_handlers* handlers;

	Proto_destroy proto_destroy;

	int n_producedEvents;

	int n_consumedEvents;
	int* consumedEvents;
};

proto_def* create_protocol_definition(unsigned short proto_id, char* proto_name, void* state, Proto_destroy protocol_destroy_function) {

	proto_def* def = malloc(sizeof(proto_def));
	def->proto_id = proto_id;
	int nameSize = strlen(proto_name) +1;
	def->proto_name = malloc(nameSize);
	bzero(def->proto_name, nameSize);
	strncpy(def->proto_name, proto_name, nameSize-1);

	def->state = state;

	def->proto_main_loop = NULL;
	def->handlers = NULL;
	def->proto_destroy = protocol_destroy_function;

	def->n_producedEvents = 0;
	def->n_consumedEvents = 0;
	def->consumedEvents = NULL;

	return def;
}


void destroy_protocol_definition(proto_def* protocol_definition) {
	if(protocol_definition != NULL) {
		if(protocol_definition->proto_destroy)
			protocol_definition->proto_destroy(protocol_definition->state);
		if(protocol_definition->consumedEvents != NULL)
			free(protocol_definition->consumedEvents);
		if(protocol_definition->handlers != NULL) {
			free(protocol_definition->handlers);
		}

		free(protocol_definition->proto_name);
		free(protocol_definition);
	}
}

void proto_def_add_produced_events(proto_def* protocol_definition, int n_events) {

	protocol_definition->n_producedEvents = n_events;
}

void proto_def_add_consumed_event(proto_def* protocol_definition, short producer_id, int event) {
	protocol_definition->n_consumedEvents ++;
	if(protocol_definition->consumedEvents)
		protocol_definition->consumedEvents = realloc(protocol_definition->consumedEvents, sizeof(int)*protocol_definition->n_consumedEvents);
	else {
		protocol_definition->consumedEvents = malloc(sizeof(int)*protocol_definition->n_consumedEvents);
	}

	if(producer_id >= 0)
		protocol_definition->consumedEvents[protocol_definition->n_consumedEvents-1] = (producer_id * 100) + event;
	else
		protocol_definition->consumedEvents[protocol_definition->n_consumedEvents-1] = (producer_id * 100) + (-1 * event);



}

void proto_def_add_protocol_main_loop(proto_def* protocol_definition, Proto_main_loop proto_main_loop) {
	protocol_definition->proto_main_loop = proto_main_loop;
}

static protocol_handlers* init_proto_handlers() {
	protocol_handlers* handlers = malloc(sizeof(protocol_handlers));

	handlers->msg_handler = NULL;
	handlers->timer_handler = NULL;
	handlers->event_handler = NULL;
	handlers->request_handler = NULL;

	return handlers;
}


void proto_def_add_msg_handler(proto_def* protocol_definition, YggMessage_handler handler) {
	if(protocol_definition->handlers == NULL)
		protocol_definition->handlers = init_proto_handlers();

	protocol_definition->handlers->msg_handler = handler;
}

void proto_def_add_timer_handler(proto_def* protocol_definition, YggTimer_handler handler) {
	if(protocol_definition->handlers == NULL)
		protocol_definition->handlers = init_proto_handlers();

	protocol_definition->handlers->timer_handler = handler;
}

void proto_def_add_event_handler(proto_def* protocol_definition, YggEvent_handler handler) {
	if(protocol_definition->handlers == NULL)
		protocol_definition->handlers = init_proto_handlers();

	protocol_definition->handlers->event_handler =  handler;
}

void proto_def_add_request_handler(proto_def* protocol_definition, YggRequest_handler handler) {
	if(protocol_definition->handlers == NULL)
		protocol_definition->handlers = init_proto_handlers();

	protocol_definition->handlers->request_handler = handler;
}

YggMessage_handler proto_def_get_YggMessageHandler(proto_def* protocol_definition) {
	if(protocol_definition->handlers == NULL)
		return NULL;
	return protocol_definition->handlers->msg_handler;
}

YggTimer_handler proto_def_get_YggTimerHandler(proto_def* protocol_definition) {
	if(protocol_definition->handlers == NULL)
		return NULL;
	return protocol_definition->handlers->timer_handler;
}

YggEvent_handler proto_def_get_YggEventHandler(proto_def* protocol_definition) {
	if(protocol_definition->handlers == NULL)
		return NULL;
	return protocol_definition->handlers->event_handler;
}

YggRequest_handler proto_def_get_YggRequestHandler(proto_def* protocol_definition) {
	if(protocol_definition->handlers == NULL)
		return NULL;
	return protocol_definition->handlers->request_handler;
}

void* proto_def_getState(proto_def* protocol_definition) {
	return protocol_definition->state;
}

short proto_def_getId(proto_def* protocol_definition) {
	return protocol_definition->proto_id;
}

/*********************************************************
 * Application Definition
 *********************************************************/

struct _app_def {
	int app_id;
	char* app_name;

	int n_producedEvents;

	int n_consumedEvents;
	int* consumedEvents;
};

app_def* create_application_definition(int app_id, char* application_name) {
	app_def* application_definition = malloc(sizeof(app_def));
	application_definition->app_id = app_id;

	int nameSize = strlen(application_name) +1;
	application_definition->app_name = malloc(nameSize);
	bzero(application_definition->app_name, nameSize);
	strncpy(application_definition->app_name, application_name, nameSize-1);

	application_definition->n_producedEvents = 0;


	application_definition->n_consumedEvents = 0;
	application_definition->consumedEvents = NULL;

	return application_definition;
}

void destroy_application_definition(app_def* application_definition) {
	if(application_definition != NULL) {
		if(application_definition->consumedEvents != NULL)
			free(application_definition->consumedEvents);

		free(application_definition->app_name);
		free(application_definition);
	}
}

void app_def_add_produced_events(app_def* application_definition, int n_events) {

	application_definition->n_producedEvents = n_events;

}

void app_def_add_consumed_events(app_def* application_definition, short producer_id, int event) {
	application_definition->n_consumedEvents ++;
	application_definition->consumedEvents = realloc(application_definition->consumedEvents, sizeof(int)*application_definition->n_consumedEvents);
	if(producer_id >= 0)
		application_definition->consumedEvents[application_definition->n_consumedEvents-1] = (producer_id * 100) + event;
	else
		application_definition->consumedEvents[application_definition->n_consumedEvents-1] = (producer_id * 100) + (-1 * event);
}

/**********************************************************
 * Private data structures
 **********************************************************/
typedef struct _pre_registered_protos {
	short id;
	Proto_init proto_init_function;
	Proto_arg_parser proto_parser;
	//void* args;

	struct _pre_registered_protos* next;
}pre_registered_protos;

//List of events to which a protocol is interested
typedef struct _interest {
	short id;
	struct _interest* next;
}InterestList;

//List of events to which a protocol is interested
typedef struct _eventList {
	int nevents;
	InterestList** eventList;
}EventList;

typedef struct _proto {

	queue_t* protoQueue; //queues for each one
	queue_t* realProtoQueue; //real queues for each one
	pthread_t proto; //threads for each one
	EventList eventList;

	proto_def* definition;

	pthread_mutex_t* lock;
	struct _proto* next;
}proto;

typedef struct _protos_info { //user defined protocols
	int nprotos; //how many
	pthread_mutex_t* lock;
	proto* protos;
}protos_info;

typedef struct _app {
	short appID; //ids (starting from APP_0 (400))
	char* appName;
	queue_t* appQueue; //queues for each one
	EventList eventList;

	int n_consumedEvents;
	int* consumedEvents;

	struct _app* next;
}app;

typedef struct _apps_info { //apps queues and ids
	int napps; //how many
	app* apps;
}apps_info;

typedef struct _thread_info {
	short id;
	Proto_main_loop proto_main_loop;
	main_loop_args* args;
	pthread_t* pthread;
	proto* protocol;
	struct _thread_info* next;
}thread_info;


typedef struct _intercept_proto_request{
	short proto_id;
	proto* to_be_intercepted;

	struct _intercept_proto_request* next;
}intercept_proto_request;

typedef struct _intercept_app_request{
	short proto_id;
	app* to_be_intercepted;

	struct _intercept_app_request* next;
}intercept_app_request;


/**********************************************************
 * Private global variables
 **********************************************************/

static pre_registered_protos* preregister = NULL;

static protos_info ygg_proto_info;
static protos_info proto_info;
static apps_info app_info;

static EventList any_events;

static int threads;
static thread_info* tinfo = NULL;

static intercept_proto_request* ipr = NULL;
static intercept_app_request* iar = NULL;

static void* executor_state;

/**********************************************************
 * Global variables
 **********************************************************/

static Channel ch;
static WLANAddr bcastAddress;

static uuid_t myuuid;

/*********************************************************
 * Info struct utilities and manipulation
 *********************************************************/

static pthread_mutex_t* lock_proto(proto* proto) {
	if(proto->lock != NULL) {
		pthread_mutex_lock(proto->lock);
		return proto->lock;
	}
	return NULL;
}

static pthread_mutex_t* lock_protos(protos_info* protos) {
	if(protos->lock != NULL) {
		pthread_mutex_lock(protos->lock);
		return protos->lock;
	}
	return NULL;

}

static void unlock(pthread_mutex_t* lock) {
	if(lock != NULL)
		pthread_mutex_unlock(lock);
}

static int registered(short proto_id, protos_info* protos){

	pthread_mutex_t* previous = lock_protos(protos);

	proto* it = protos->protos;

	while(it != NULL) {
		pthread_mutex_t* current = lock_proto(it);
		if(proto_id == it->definition->proto_id) {
			unlock(previous);
			unlock(current);
			return 1;
		}

		unlock(previous);
		previous = current;
		it = it->next;
	}
	unlock(previous);
	return 0;

}

static int registeredApp(app_def* app_definition, app* apps){

	app* it = apps;

	while(it != NULL) {
		if(app_definition->app_id == it->appID) {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "Conflicting application ids: application %s and application %s have the same id %d", app_definition->app_name, it->appName, app_definition->app_id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			return 1;
		}

		it = it->next;
	}
	return 0;

}

static proto* findProto(short protoID, protos_info* protos){

	pthread_mutex_t* previous = lock_protos(protos);
	proto* it = protos->protos;
	while(it != NULL) {
		pthread_mutex_t* current = lock_proto(it);
		if(it->definition->proto_id == protoID) {
			unlock(previous);
			return it;
		}
		it = it->next;
		unlock(previous);
		previous = current;
	}

	unlock(previous);
	return NULL;

}

static app* findApp(short appID, app* apps){

	app* it = apps;
	while(it != NULL) {
		if(it->appID == appID)
			return it;
		it = it->next;
	}

	return NULL;

}

static EventList* getEventList(short id){
	EventList* eventList = NULL;

	if(id == -1) {
		eventList = &any_events;
	} else if(id >= APP_0){
		app* app = findApp(id, app_info.apps);

		if(app == NULL)
			return eventList;
		eventList = &app->eventList;
	}
	else if(id >= PROTO_0) {
		proto* proto;
		if((proto = findProto(id, &proto_info)) == NULL)
			return eventList;
		eventList = &proto->eventList;
		unlock(proto->lock);
	}
	else{
		proto* proto;
		if((proto = findProto(id, &ygg_proto_info)) == NULL)
			return eventList;
		eventList = &proto->eventList;
	}
	return eventList;
}

static void destroy_eventList(EventList* eventList) {
	for(int i = 0; i < eventList->nevents; i++) {
		InterestList* itl = eventList->eventList[i];
		while(itl != NULL) {
			InterestList* torm = itl;
			itl = itl->next;
			free(torm);
		}
	}

	free(eventList->eventList);
	eventList->nevents = 0;
}

static queue_t* getProtoQueue(short id){
	queue_t* dropbox = NULL;
	if(id >= APP_0){
		app* app;
		if((app = findApp(id, app_info.apps)) == NULL)
			return NULL;
		dropbox = app->appQueue;
	}
	else if(id >= PROTO_0) {
		proto* proto;
		if((proto = findProto(id, &proto_info)) == NULL)
			return NULL;
		dropbox = proto->protoQueue;
		unlock(proto->lock);
	}
	else{
		proto* proto;
		if((proto = findProto(id, &ygg_proto_info)) == NULL)
			return NULL;
		dropbox = proto->protoQueue;
	}
	if(dropbox == NULL){
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "There is no queue registered for protocol/application with id %d, exiting...", id);
		ygg_log("YGGDRASIL RUNTIME", "PANIC", error_msg);
		ygg_logflush();
		exit(1);
	}
	return dropbox;
}

static proto* newProto(protos_info* protos, pthread_mutex_t* proto_lock) {

	pthread_mutex_t* previous = lock_protos(protos);
	pthread_mutex_t* current = NULL;

	proto** prot = &protos->protos;
	while(*prot != NULL) {
		current = lock_proto(*prot);
		unlock(previous);
		previous = current;
		prot = &(*prot)->next;
	}

	*prot = malloc(sizeof(proto));
	(*prot)->next = NULL;
	(*prot)->lock = proto_lock;
	lock_proto(*prot);
	unlock(previous);
	return *prot;
}

static proto* rmProto(protos_info* protos, short id) {
	pthread_mutex_t* previous = lock_protos(protos);
	pthread_mutex_t* current = NULL;

	proto** it = &protos->protos;
	proto* prev = NULL;
	while(*it != NULL) {
		current = lock_proto(*it);
		if((*it)->definition->proto_id == id) {
			proto* torm = *it;
			if(prev == NULL) {
				*it = torm->next;
			} else {
				prev->next = torm->next;
			}
			unlock(previous);
			return torm;

		}
		prev = *it;
		unlock(previous);
		previous = current;
		it = &(*it)->next;
	}

	unlock(previous);
	return NULL;

}

static app* newApp() {
	if(app_info.apps == NULL) {
		app_info.apps = malloc(sizeof(app));
		app_info.apps->next = NULL;
		return app_info.apps;
	} else {
		app* it = app_info.apps;
		while(it->next != NULL) {
			it = it->next;
		}

		it->next = malloc(sizeof(app));
		it = it->next;
		it->next = NULL;
		return it;
	}
}

static int filter(YggMessage* msg) {
	if(memcmp(msg->destAddr.data, ch.hwaddr.data, WLAN_ADDR_LEN) == 0 || memcmp(msg->destAddr.data, bcastAddress.data, WLAN_ADDR_LEN) == 0)
		return 0;
	return 1;
}

static char* getProtoName(short id) {
	if(id >= APP_0){
		app* app = findApp(id, app_info.apps);
		if(app == NULL)
			return NULL;
		int nameSize = strlen(app->appName) + 1;
		char* name = malloc(nameSize);
		bzero(name, nameSize);
		memcpy(name, app->appName, nameSize - 1);

		return name;
	}
	else if(id >= PROTO_0) {
		proto* proto;
		if((proto = findProto(id, &proto_info)) == NULL)
			return NULL;
		int nameSize = strlen(proto->definition->proto_name) + 1;
		char* name = malloc(nameSize);
		bzero(name, nameSize);
		memcpy(name, proto->definition->proto_name, nameSize - 1);
		unlock(proto->lock);
		return name;
	}
	else{
		proto* proto;
		if((proto = findProto(id, &ygg_proto_info)) == NULL)
			return NULL;
		int nameSize = strlen(proto->definition->proto_name) + 1;
		char* name = malloc(nameSize);
		bzero(name, nameSize);
		memcpy(name, proto->definition->proto_name, nameSize - 1);

		return name;
	}

}

static void reevaluateQueueDepencies(queue_t* old_queue, queue_t* new_queue) { //TODO make changes visible on protocols

	proto* it = ygg_proto_info.protos;

	while(it != NULL) {

		if(it->protoQueue == old_queue)
			it->protoQueue = new_queue;

		it = it->next;
	}

	pthread_mutex_t* previous = lock_protos(&proto_info);
	it = proto_info.protos;

	while(it != NULL) {
		pthread_mutex_t* current = lock_proto(it);
		if(it->protoQueue == old_queue)
			it->protoQueue = new_queue;

		unlock(previous);
		previous = current;
		it = it->next;
	}
	unlock(previous);

}

/*********************************************************
 * Register Protocols in Runtime
 *********************************************************/

static int registThread(proto* protocol, Proto_main_loop main_loop){

	thread_info* th = NULL;

	if(tinfo == NULL) {
		tinfo = malloc(sizeof(thread_info));
		tinfo->next = NULL;
		th = tinfo;
	}else {
		thread_info* it = tinfo;
		while(it->next != NULL) {
			it = it->next;
		}
		it->next = malloc(sizeof(thread_info));
		it = it->next;
		it->next = NULL;
		th = it;
	}

	th->protocol = protocol;
	th->id = protocol->definition->proto_id;
	th->pthread = &protocol->proto;
	th->proto_main_loop = main_loop;
	th->args = malloc(sizeof(main_loop_args));
	th->args->inBox = protocol->realProtoQueue;
	th->args->state = protocol->definition->state;

	threads ++;

	return SUCCESS;
}

static int unregisterThread(int id) {
	if(tinfo->id == id) {
		thread_info* torm = tinfo;
		tinfo = torm->next;
		free(torm);
	}else{
		thread_info* it = tinfo;
		while(it->next != NULL) {
			if(it->next->id == id) {
				thread_info* torm = it->next;
				it->next = torm->next;
				free(torm);
				return SUCCESS;
			}
			it = it->next;
		}

		return FAILED;
	}

	return SUCCESS;
}

static thread_info* getThread(int id) {
	thread_info* it = tinfo;
	while(it != NULL) {
		if(it->id == id)
			return it;

		it = it->next;
	}

	return NULL;
}

static void apply_intercept_request(short id, queue_t* replacement) {
	intercept_proto_request* it = ipr;
	while(it != NULL) {
		if(it->proto_id == id){
			it->to_be_intercepted->protoQueue = replacement;
			return;
		}
		it = it->next;
	}

	intercept_app_request* it2 = iar;
	while(it2 != NULL) {
		if(it2->proto_id == id){
			it2->to_be_intercepted->appQueue = replacement;
			return;
		}
		it2 = it2->next;
	}
}

static void configProto(proto* proto, proto_def* protocol_definition){
	proto->definition = protocol_definition;

	proto->realProtoQueue = queue_init(proto->definition->proto_id, DEFAULT_QUEUE_SIZE); //timer
	proto->protoQueue = proto->realProtoQueue;

	apply_intercept_request(proto->definition->proto_id, proto->protoQueue);

	proto->eventList.nevents = protocol_definition->n_producedEvents;

	int n_events = proto->eventList.nevents;
	if(n_events > 0)
		proto->eventList.eventList = malloc(sizeof(InterestList*)*n_events);
	else
		proto->eventList.eventList = NULL;

	int i = 0;
	for(; i < n_events; i++){
		proto->eventList.eventList[i] = NULL;
	}

}

static void reconfigProto(proto* proto, proto_def* protocol_definition){

	queue_t* executorQueue = getProtoQueue(PROTO_EXECUTOR);

	if(proto->realProtoQueue == executorQueue) {
		unregister_proto_handlers(proto->definition->proto_id, executor_state);

		proto->realProtoQueue = queue_init(proto->definition->proto_id, DEFAULT_QUEUE_SIZE);
		reevaluateQueueDepencies(executorQueue, proto->realProtoQueue);
	}

	if(proto->eventList.nevents > 0) {
		destroy_eventList(&proto->eventList);
	}

	destroy_protocol_definition(proto->definition);

	proto->definition = protocol_definition;

	proto->eventList.nevents = protocol_definition->n_producedEvents;

	int n_events = proto->eventList.nevents;
	if(n_events > 0)
		proto->eventList.eventList = malloc(sizeof(InterestList*)*n_events);
	else
		proto->eventList.eventList = NULL;

	int i = 0;
	for(; i < n_events; i++){
		proto->eventList.eventList[i] = NULL;
	}

}

static void configExecutorProto(proto* proto, proto_def* protocol_definition){
	proto->definition = protocol_definition;

	queue_t* executorQueue = getProtoQueue(PROTO_EXECUTOR);

	proto->realProtoQueue = executorQueue;
	proto->protoQueue = proto->realProtoQueue;

	apply_intercept_request(proto->definition->proto_id, proto->protoQueue);

	proto->eventList.nevents = protocol_definition->n_producedEvents;

	register_proto_handlers(protocol_definition, executor_state);


	int n_events = proto->eventList.nevents;
	if(n_events > 0)
		proto->eventList.eventList = malloc(sizeof(InterestList*)*n_events);
	else
		proto->eventList.eventList = NULL;

	int i = 0;
	for(; i < n_events; i++){
		proto->eventList.eventList[i] = NULL;
	}

}


static void reconfigExecutorProto(proto* proto, proto_def* protocol_definition){

	queue_t* executorQueue = getProtoQueue(PROTO_EXECUTOR);

	if(proto->realProtoQueue != executorQueue) {
		reevaluateQueueDepencies(proto->realProtoQueue, executorQueue);
		queue_destroy(proto->realProtoQueue);
	}

	unregister_proto_handlers(proto->definition->proto_id, executor_state);
	proto->realProtoQueue = executorQueue;

	if(proto->eventList.nevents > 0) {
		destroy_eventList(&proto->eventList);
	}


	proto->definition = protocol_definition;

	proto->eventList.nevents = protocol_definition->n_producedEvents;

	proto->eventList.nevents = protocol_definition->n_producedEvents;

	register_proto_handlers(protocol_definition, executor_state);


	int n_events = proto->eventList.nevents;
	if(n_events > 0)
		proto->eventList.eventList = malloc(sizeof(InterestList*)*n_events);
	else
		proto->eventList.eventList = NULL;

	int i = 0;
	for(; i < n_events; i++){
		proto->eventList.eventList[i] = NULL;
	}

}

short registerProtocol(short proto_id, Proto_init proto_init, void* proto_args) {


	if(proto_id < 100 || proto_id >= 400) {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Invalid protocol id %d, allowed range [100, 400[", proto_id);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return FAILED;
	}

	if(!registered(proto_id, &proto_info)) {
		proto_def* protocol_definition = proto_init(proto_args);
		pthread_mutex_t* lock = malloc(sizeof(pthread_mutex_t));
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(lock, &attr);

		proto* proto = newProto(&proto_info, lock);

		if(protocol_definition->proto_main_loop != NULL) {
			if(protocol_definition->handlers != NULL){
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Main loop and Handlers defined, defaulting to use only protocol's main loop in protocol %s", protocol_definition->proto_name);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}
			configProto(proto, protocol_definition);
			registThread(proto, protocol_definition->proto_main_loop);
		}else if(protocol_definition->handlers != NULL) {
			configExecutorProto(proto, protocol_definition);

		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "No Main loop nor Handlers defined, ignoring protocol %s", protocol_definition->proto_name);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);

			proto = rmProto(&proto_info, proto->definition->proto_id);
			destroy_protocol_definition(proto->definition);
			unlock(proto->lock);
			pthread_mutex_destroy(proto->lock);
			free(proto->lock);
			free(proto);

			return FAILED;
		}

		unlock(proto->lock);
		return SUCCESS;
	}

	char error_msg[200];
	bzero(error_msg, 200);
	sprintf(error_msg, "Protocol with id %d is already registered as %s, ignoring protocol", proto_id, getProtoName(proto_id));
	ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
	return FAILED;
}

short registerYggProtocol(short proto_id, Proto_init proto_init, void* proto_args) {


	if(proto_id < 0 || proto_id >= 100) {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Invalid protocol id %d, allowed range [0, 100[", proto_id);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return FAILED;
	}

	if(!registered(proto_id, &ygg_proto_info)) {
		proto_def* protocol_definition = proto_init(proto_args);
		proto* proto = newProto(&ygg_proto_info, NULL);

		if(protocol_definition->proto_main_loop != NULL) {
			if(protocol_definition->handlers != NULL){
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Main loop and Handlers defined, defaulting to use only protocol's main loop in protocol %s", protocol_definition->proto_name);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}
			configProto(proto, protocol_definition);
			registThread(proto, protocol_definition->proto_main_loop);
		}else if(protocol_definition->handlers != NULL) {
			configExecutorProto(proto, protocol_definition);

		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "No Main loop nor Handlers defined, ignoring protocol %s", protocol_definition->proto_name);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);

			return FAILED;
		}

		return SUCCESS;
	}

	char error_msg[200];
	bzero(error_msg, 200);
	sprintf(error_msg, "Protocol with id %d is already registered as %s, ignoring protocol", proto_id, getProtoName(proto_id));
	ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
	return FAILED;
}

short overrideDispatcherProtocol(Dispatcher_init proto_init, void* proto_args) {
	proto_def* protocol_definition = proto_init(&ch, proto_args);

	if(protocol_definition->proto_id == PROTO_DISPATCH) {

		proto* dispatch = ygg_proto_info.protos;

		if(protocol_definition->proto_main_loop != NULL) {
			if(protocol_definition->handlers != NULL){
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Main loop and Handlers defined, defaulting to use only protocol's main loop in protocol %s", protocol_definition->proto_name);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}

			reconfigProto(dispatch, protocol_definition);

			thread_info* dispatcher_thread = getThread(PROTO_DISPATCH);
			if(dispatcher_thread != NULL) {
				dispatcher_thread->proto_main_loop = protocol_definition->proto_main_loop;
				dispatcher_thread->args->state = protocol_definition->state;
			}else{
				registThread(dispatch, protocol_definition->proto_main_loop);
			}

		}else if(protocol_definition->handlers != NULL) {
			unregisterThread(PROTO_DISPATCH);
			reconfigExecutorProto(dispatch, protocol_definition);

		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "No Main loop nor Handlers defined, request to override dispatcher by protocol %s will be ignored", protocol_definition->proto_name);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);

			return FAILED;
		}

		return SUCCESS;
	}
	char error_msg[200];
	bzero(error_msg, 200);
	sprintf(error_msg, "Ignoring request to override dispatcher as protocol %s has a different ID (should be %d , you can use constant PROTO_DISPATCH for convenience)", protocol_definition->proto_name, PROTO_DISPATCH);
	ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);

	return FAILED;
}


queue_t* registerApp(app_def* application_definition){

	if(application_definition->app_id < 400) {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Invalid application id %d for application %s, allowed range >= 400", application_definition->app_id, application_definition->app_name);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return NULL;
	}

	if(!registeredApp(application_definition, app_info.apps)) {

		app* app = newApp();

		app->appID = application_definition->app_id;

		int nameSize = strlen(application_definition->app_name) + 1;
		app->appName = malloc(nameSize);
		bzero(app->appName, nameSize);
		memcpy(app->appName, application_definition->app_name, nameSize-1);

		app->appQueue = queue_init(app->appID, DEFAULT_QUEUE_SIZE);

		apply_intercept_request(app->appID, app->appQueue);

		int nevents = application_definition->n_producedEvents;

		app->eventList.nevents = nevents;
		if(nevents > 0)
			app->eventList.eventList = malloc(sizeof(InterestList*)*nevents);
		else
			app->eventList.eventList = NULL;
		int i = 0;
		for(; i < nevents; i++){
			app->eventList.eventList[i] = NULL;
		}

		app->n_consumedEvents = application_definition->n_consumedEvents;
		if(app->n_consumedEvents > 0) {
			app->consumedEvents = malloc(sizeof(int)*app->n_consumedEvents);
			memcpy(app->consumedEvents, application_definition->consumedEvents, sizeof(int)*app->n_consumedEvents);
		}else
			app->consumedEvents = NULL;

		app_info.napps ++;

		return app->appQueue;
	}

	char error_msg[200];
	bzero(error_msg, 200);
	sprintf(error_msg, "Application with id %d is already registered, ignoring application %s with id %d", application_definition->app_id, application_definition->app_name, application_definition->app_id);
	ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
	return NULL;
}

static int init_ygg_proto_info() {

	if(registerYggProtocol(PROTO_DISCOV, (Proto_init) &dispatcher_init, (void*) &ch) == FAILED)
		return FAILED;

	if(registerYggProtocol(PROTO_TIMER, &timer_init, NULL) == FAILED)
		return FAILED;

	return registerYggProtocol(PROTO_EXECUTOR, &executor_init, NULL);


}

/*********************************************************
 * Configure Protocol dependencies
 *********************************************************/

static int registInterestEvents(proto* protocol) {

	for(int i = 0; i < protocol->definition->n_consumedEvents; i ++) {
		int consume = protocol->definition->consumedEvents[i];

		int producer_id = consume / 100;
		int notification_id = abs(consume % 100);


		EventList* eventList = getEventList(producer_id);

		if(eventList != NULL) {
			if(eventList->nevents > notification_id) {
				InterestList* interestLst = eventList->eventList[notification_id];
				if(interestLst != NULL){
					while(interestLst->id > -1 && interestLst->next != NULL){
						interestLst = interestLst->next;
					}
					if(interestLst->id < 0)
						interestLst->id = protocol->definition->proto_id;
					else{
						interestLst->next = malloc(sizeof(InterestList));
						interestLst->next->id = protocol->definition->proto_id;
						interestLst->next->next = NULL;
					}
				}else{
					eventList->eventList[notification_id] = malloc(sizeof(InterestList));
					eventList->eventList[notification_id]->id = protocol->definition->proto_id;
					eventList->eventList[notification_id]->next = NULL;
				}
			}else {
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Protocol %s with id %d does not produce the event with id %d to be consumed by protocol %s with id %d, ignoring..", getProtoName(producer_id), producer_id, notification_id, protocol->definition->proto_name, protocol->definition->proto_id);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}
		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "There is no protocol with id %d registered in the Runtime, I can't answer your request to register interest in event for protocol %s with id %d, ignoring..", producer_id, protocol->definition->proto_name, protocol->definition->proto_id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			//TODO EXIT
		}
	}

	return SUCCESS;
}

static int unregisterInterestEvents(proto* protocol) {
	for(int i = 0; i < protocol->definition->n_consumedEvents; i ++) {
		int consume = protocol->definition->consumedEvents[i];

		int producer_id = consume / 100;
		int notification_id = abs(consume % 100);

		EventList* eventList = getEventList(producer_id);

		if(eventList != NULL) {
			if(eventList->nevents > notification_id) {
				InterestList* interestLst = eventList->eventList[notification_id];
				if(interestLst != NULL){
					while(interestLst != NULL){
						if(interestLst->id == protocol->definition->proto_id) {
							interestLst->id = -1;
							break;
						}
						interestLst = interestLst->next;
					}
				}else{
					char error_msg[200];
					bzero(error_msg, 200);
					sprintf(error_msg, "Protocol %s with id %d does not produce the event with id %d to be consumed by protocol %s with id %d, ignoring..", getProtoName(producer_id), producer_id, notification_id, protocol->definition->proto_name, protocol->definition->proto_id);
					ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
				}
			}else {
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Protocol %s with id %d does not produce the event with id %d to be consumed by protocol %s with id %d, ignoring..", getProtoName(producer_id), producer_id, notification_id, protocol->definition->proto_name, protocol->definition->proto_id);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}
		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "There is no protocol with id %d registered in the Runtime, I can't answer your request to unregister interest in event for protocol %s with id %d, ignoring..", producer_id, protocol->definition->proto_name, protocol->definition->proto_id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		}

	}

	return SUCCESS;
}

static int registAppInterestEvents(app* app) {

	for(int i = 0; i < app->n_consumedEvents; i ++) {
		int consume = app->consumedEvents[i];

		int producer_id = consume / 100;
		int notification_id = abs(consume % 100);

		EventList* eventList = getEventList(producer_id);

		if(eventList != NULL) {
			if(eventList->nevents > notification_id) {
				InterestList* interestLst = eventList->eventList[notification_id];
				if(interestLst != NULL){
					while(interestLst->next != NULL){
						interestLst = interestLst->next;
					}
					interestLst->next = malloc(sizeof(InterestList));
					interestLst->next->id = app->appID;
					interestLst->next->next = NULL;
				}else{
					eventList->eventList[notification_id] = malloc(sizeof(InterestList));
					eventList->eventList[notification_id]->id = app->appID;
					eventList->eventList[notification_id]->next = NULL;
				}
			}else {
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Protocol %s with id %d does not produce the event with id %d to be consumed by application %s with id %d, ignoring..", getProtoName(producer_id), producer_id, notification_id, app->appName, app->appID);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			}
		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "There is no protocol with id %d registered in the Runtime, I can't answer your request to register interest in event for application %s with id %d, ignoring", producer_id, app->appName, app->appID);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		}
	}

	return SUCCESS;
}


static void registInterestedEvent() {
	proto* proto = ygg_proto_info.protos;
	while(proto != NULL) {
		registInterestEvents(proto);
		proto = proto->next;
	}

	pthread_mutex_t* previous = lock_protos(&proto_info);
	proto = proto_info.protos;

	while(proto != NULL) {
		pthread_mutex_t* current = lock_proto(proto);
		registInterestEvents(proto);
		unlock(previous);
		previous = current;
		proto = proto->next;
	}
	unlock(previous);

	app* app = app_info.apps;

	while(app != NULL) {
		registAppInterestEvents(app);
		app = app->next;
	}


}

/**********************************************************
 * Initialize runtime
 **********************************************************/

static void processTerminateSignal(int sig, siginfo_t *si, void *uc) {
	ygg_logflush();
	ygg_logflush_stdout();
	exit(0);
}

static void setupTerminationSignalHandler() {
	/* Establish handler for timer signal */
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = processTerminateSignal;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIG, &sa, NULL) == -1) {
		fprintf(stderr, "Unable to setup termination signal handler. Shutdown will not be clean\n");
	}

}

int ygg_runtime_init_static(NetworkConfig* ntconf) {

	ygg_loginit(); //Initialize logger

	createChannel(&ch, "wlan0");
	bindChannel(&ch);
	set_ip_addr(&ch);
	defineFilter(&ch, ntconf->filter);

	setupTerminationSignalHandler();

	srand(time(NULL));   // should only be called once
	//genUUID(myuuid); //Use this line to generate a random uuid
	genStaticUUID(myuuid); //Use this to generate a static uuid based on the hostname

	threads = 0;

	ygg_proto_info.protos = NULL;
	ygg_proto_info.lock = NULL;


	if(init_ygg_proto_info() == FAILED){ //Initialize the mandatory lightkone protocols (dispatcher and timer)
		ygg_log("YGGDRASIL RUNTIME", "SETUP ERROR", "Failed to setup mandatory ygg protocols dispatcher and timer");
		ygg_logflush();
		exit(1);
	}

	executor_state = findProto(PROTO_EXECUTOR, &ygg_proto_info)->definition->state;

	any_events.nevents = EVENTS_MAX;
	if(any_events.nevents > 0)
		any_events.eventList = malloc(sizeof(InterestList*)*any_events.nevents);
	else
		any_events.eventList = NULL;

	int i = 0;
	for(; i < any_events.nevents; i++){
		any_events.eventList[i] = NULL;
	}

	//Ready the structure to have the information about Apps

	app_info.apps = NULL;

	app_info.napps = 0;

	//Ready the structure to have information about user defined protocols
	proto_info.protos = NULL;
	proto_info.lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(proto_info.lock, &attr);

	proto_info.nprotos = 0;

	str2wlan((char*)bcastAddress.data, WLAN_BROADCAST);

	return SUCCESS;
}


int ygg_runtime_init(NetworkConfig* ntconf) {

	ygg_loginit(); //Initialize logger

	if(setupSimpleChannel(&ch, ntconf) != SUCCESS){ //try to configure the physical device and open a socket on it
		ygg_log("YGGDRASIL RUNTIME", "SETUP ERROR", "Failed to setup channel for communication");
		ygg_logflush();
		exit(1);
	}

	if(setupChannelNetwork(&ch, ntconf) != SUCCESS){ //try to connect to a network which was specified in the NetworkConfig structure
		ygg_log("YGGDRASIL RUNTIME", "SETUP ERROR", "Failed to setup channel network for communication");
		ygg_logflush();
		exit(1);
	}

	setupTerminationSignalHandler();

	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	//srand(time(NULL));   // should only be called once
	srand(time.tv_nsec);   // should only be called once
	//genUUID(myuuid); //Use this line to generate a random uuid
	genStaticUUID(myuuid); //Use this to generate a static uuid based on the hostname

	threads = 0;

	ygg_proto_info.protos = NULL;
	ygg_proto_info.lock = NULL;


	if(init_ygg_proto_info() == FAILED){ //Initialize the mandatory lightkone protocols (dispatcher and timer)
		ygg_log("YGGDRASIL RUNTIME", "SETUP ERROR", "Failed to setup mandatory ygg protocols dispatcher and timer");
		ygg_logflush();
		exit(1);
	}

	executor_state = findProto(PROTO_EXECUTOR, &ygg_proto_info)->definition->state;

	any_events.nevents = EVENTS_MAX;
	if(any_events.nevents > 0)
		any_events.eventList = malloc(sizeof(InterestList*)*any_events.nevents);
	else
		any_events.eventList = NULL;

	int i = 0;
	for(; i < any_events.nevents; i++){
		any_events.eventList[i] = NULL;
	}

	//Ready the structure to have the information about Apps

	app_info.apps = NULL;

	app_info.napps = 0;

	//Ready the structure to have information about user defined protocols
	proto_info.protos = NULL;
	proto_info.lock = malloc(sizeof(pthread_mutex_t));
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(proto_info.lock, &attr);

	proto_info.nprotos = 0;

	str2wlan((char*)bcastAddress.data, WLAN_BROADCAST);

	return SUCCESS;
}

const char* getChannelIpAddress() {
	if(ch.ip_addr[0] == '\0')
		set_ip_addr(&ch);

	return ch.ip_addr;

}

int ygg_runtime_start() {

#ifdef DEBUG
	int i = 0;
#endif
	pthread_attr_t patribute;

	registInterestedEvent();

	thread_info* it = tinfo;

	while(it != NULL){
		int err = 0;

#ifdef DEBUG
		proto* protocol = it->protocol;
		char m[2000];
		memset(m, 0, 2000);
		sprintf(m,"threads to start: %d; on starting %d; starting thread with id %d for protocol %s",threads, i, it->id, protocol->definition->proto_name);
		ygg_log("YGGDRASIL RUNTIME","INFO", m);
		i++;
#endif
		pthread_attr_init(&patribute);
		err = pthread_create(it->pthread, &patribute, (gen_function) it->proto_main_loop, (void *)(it->args));

		if(err != 0){
			switch(err){
			case EAGAIN: ygg_log("YGGDRASIL RUNTIME","PTHREAD ERROR","No resources to create thread"); ygg_logflush(); exit(err);
			break;
			case EPERM: ygg_log("YGGDRASIL RUNTIME","PTHREAD ERROR","No permissions to create thread"); ygg_logflush(); exit(err);
			break;
			case EINVAL: ygg_log("YGGDRASIL RUNTIME","PTHREAD ERROR","Invalid attributes on create thread"); ygg_logflush(); exit(err);
			break;
			default:
				ygg_log("YGGDRASIL RUNTIME","PTHREAD ERROR","Unknown error on create thread"); ygg_logflush(); exit(1);
			}
		}
		it=it->next;
	}

	return SUCCESS;
}


static int preregisteredProtocol(short proto_id) {
	pre_registered_protos* it = preregister;
	while(it != NULL) {
		if(it->id == proto_id) {
			return SUCCESS;
		}
		it = it->next;
	}

	//TODO maybe lock?
	proto* it2 = proto_info.protos;
	while(it2 != NULL) {
		if(it2->definition->proto_id == proto_id) {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "Conflicting protocol ids: registered protocol %s has the same id %d of new pre registered protocol", it2->definition->proto_name, proto_id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			return SUCCESS;
		}
		it2 = it2->next;
	}

	return FAILED;
}

short pre_registerProtocol(short proto_id, Proto_init protocol_init_function, Proto_arg_parser proto_parser) {
	if(!preregisteredProtocol(proto_id)) {
		if(proto_id < 100 || proto_id >= 400) {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "Invalid protocol id %d, allowed range [0, 100[", proto_id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			return FAILED;
		}

		pre_registered_protos* newproto = malloc(sizeof(pre_registered_protos));
		newproto->id = proto_id;
		newproto->proto_init_function = protocol_init_function;
		newproto->proto_parser = proto_parser;
		newproto->next = preregister;
		preregister = newproto;

		return SUCCESS;

	} else {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Protocol with id %d is already registered", proto_id);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return FAILED;
	}
}

static pre_registered_protos* findPreRegistered(short id) {
	pre_registered_protos* it = preregister;
	while(it != NULL) {
		if(it->id == id) {
			return it;
		}
		it = it->next;
	}
	return NULL;
}

int startProtocol(short id, char* proto_args) {
	pre_registered_protos* tostart = findPreRegistered(id);
	if(tostart != NULL && !registered(id, &proto_info)) {

		proto_def* protocol_definition;
		if(proto_args != NULL && tostart->proto_parser == NULL) {
			protocol_definition = tostart->proto_init_function((void*)proto_args);
		} else if(proto_args != NULL && tostart->proto_parser != NULL) {
			protocol_definition = tostart->proto_init_function(tostart->proto_parser(proto_args));
		}

		pthread_mutex_t* lock = malloc(sizeof(pthread_mutex_t));
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(lock, &attr);

		proto* proto = newProto(&proto_info, lock);

		if(protocol_definition->proto_main_loop != NULL) {
			if(protocol_definition->handlers != NULL){
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Main loop and Handlers defined, defaulting to use only protocol's handlers protocol %s", protocol_definition->proto_name);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
				configExecutorProto(proto, protocol_definition);
				registInterestEvents(proto);
				unlock(proto->lock);
			}else {
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "No Handlers defined for protocol %s, operation not supported, ignoring...", protocol_definition->proto_name);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
				unlock(proto->lock);
				proto = rmProto(&proto_info, proto->definition->proto_id);

				if(proto->definition->proto_destroy != NULL)
					proto->definition->proto_destroy(proto->definition->state);

				unlock(proto->lock);
				pthread_mutex_destroy(proto->lock);
				free(proto->lock);
				free(proto);

				return FAILED;
			}

		}else if(protocol_definition->handlers != NULL) {
			configExecutorProto(proto, protocol_definition);
			registInterestEvents(proto);
			unlock(proto->lock);
		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "No Handlers defined, ignoring protocol %s", protocol_definition->proto_name);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
			unlock(proto->lock);
			proto = rmProto(&proto_info, proto->definition->proto_id);

			if(proto->definition->proto_name != NULL)
				proto->definition->proto_destroy(proto->definition->state);

			unlock(proto->lock);
			pthread_mutex_destroy(proto->lock);
			free(proto->lock);
			free(proto);
			return FAILED;
		}

		return SUCCESS;


	} else {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Protocol with id %d does not exist in the runtime or is already running", id);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return FAILED;
	}
}

int stopProtocol(short id) {
	pre_registered_protos* tostop = findPreRegistered(id);
	if(tostop != NULL && registered(id, &proto_info)) {

		proto* proto = findProto(id, &proto_info);

		if(proto != NULL) {
			if(proto->realProtoQueue == getProtoQueue(PROTO_EXECUTOR)){

				proto = rmProto(&proto_info, id);
				unregisterInterestEvents(proto);

				YggRequest req;
				YggRequest_init(&req, id, PROTO_EXECUTOR, REQUEST, EXECUTOR_STOP_PROTOCOL);
				YggRequest_addPayload(&req, &id, sizeof(short));

				deliverRequest(&req);

				unlock(proto->lock);
				pthread_mutex_destroy(proto->lock);
				free(proto->lock);
				free(proto);

			}else {
				unlock(proto->lock);
				char error_msg[200];
				bzero(error_msg, 200);
				sprintf(error_msg, "Protocol %s with id %d is not running in the executor, it can't be stopped, ignoring...", proto->definition->proto_name, id);
				ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
				return FAILED;
			}

		}else {
			char error_msg[200];
			bzero(error_msg, 200);
			sprintf(error_msg, "No protocol with id %d is currently running, ignoring request..", id);
			ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);

			return FAILED;
		}

		return SUCCESS;

	} else {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "Protocol with id %d does not exist in the runtime or is not running", id);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
		return FAILED;
	}
}


static void set_proto_queue_intercept_request(proto* protocol, short id) {
	intercept_proto_request* req = malloc(sizeof(intercept_proto_request));
	req->proto_id = id;
	req->to_be_intercepted = protocol;
	req->next = ipr;
	ipr = req;

}

static void set_app_queue_intercept_request(app* application, short id) {
	intercept_app_request* req = malloc(sizeof(intercept_proto_request));
	req->proto_id = id;
	req->to_be_intercepted = application;
	req->next = iar;
	iar = req;
}

queue_t* interceptProtocolQueue(short protoID, short myID) {

	queue_t* ret = getProtoQueue(protoID);

	if(ret != NULL) {

		if(protoID >= APP_0){
			app* app = findApp(protoID, app_info.apps);
			set_app_queue_intercept_request(app, myID);

		}
		else if(protoID >= PROTO_0) {
			proto* proto = findProto(protoID, &proto_info);
			set_proto_queue_intercept_request(proto, myID);
			unlock(proto->lock);
		}
		else {
			proto* proto = findProto(protoID, &ygg_proto_info);
			set_proto_queue_intercept_request(proto, myID);
		}
	} else {
		char error_msg[200];
		bzero(error_msg, 200);
		sprintf(error_msg, "There is no protocol with id %d registered in the Runtime, I can't answer your request to intercept queue, returning NULL", protoID);
		ygg_log("YGGDRASIL RUNTIME", "WARNING", error_msg);
	}
	return ret;
}



/********************************************************
 * Internal functions
 ********************************************************/

/********************************************************
 * Runtime functions
 ********************************************************/

int dispatch(YggMessage* msg) {

	queue_t_elem elem;
	bzero(&elem, sizeof(queue_t_elem));
	elem.type = YGG_MESSAGE;
	memcpy(&elem.data.msg, msg, sizeof(YggMessage));

	queue_push(ygg_proto_info.protos->protoQueue, &elem);

	return SUCCESS;
}

int directDispatch(YggMessage* msg) {
	queue_t_elem elem;
	bzero(&elem, sizeof(queue_t_elem));
	elem.type = YGG_MESSAGE;
	memcpy(&elem.data.msg, msg, sizeof(YggMessage));

	queue_push(ygg_proto_info.protos->realProtoQueue, &elem);

	return SUCCESS;
}

int setupTimer(YggTimer* timer){
	queue_t_elem elem;
	elem.type = YGG_TIMER;

	memcpy(&elem.data.timer, timer, sizeof(YggTimer));
#ifdef DEBUG
	char msg[100];
	memset(msg,0,100);
	sprintf(msg, "Setting up timer with proto source %u and proto dest %u at time %ld", timer->proto_origin, timer->proto_dest, timer->config.first_notification);
	ygg_log("YGGDRASIL RUNTIME", "SETTING UP TIMER", msg);
#endif
	queue_push(getProtoQueue(PROTO_TIMER), &elem);
	return SUCCESS;
}

int cancelTimer(YggTimer* timer) {
	queue_t_elem elem;
	elem.type = YGG_TIMER;

	timer->config.first_notification.tv_sec = 0;
	timer->config.first_notification.tv_nsec = 0;
	timer->config.repeat_interval.tv_sec = 0;
	timer->config.repeat_interval.tv_nsec = 0;

	memcpy(&elem.data.timer, timer, sizeof(YggTimer));
#ifdef DEBUG
	char msg[100];
	memset(msg,0,100);
	sprintf(msg, "Canceling timer with proto source %u and proto dest %u", timer->proto_origin, timer->proto_dest);
	ygg_log("YGGDRASIL RUNTIME", "SETTING UP TIMER", msg);
#endif
	queue_push(getProtoQueue(PROTO_TIMER), &elem);
	return SUCCESS;
}

int deliver(YggMessage* msg) {

	short id = msg->Proto_id;
	queue_t* dropBox = getProtoQueue(id);

	queue_t_elem elem;
	elem.type = YGG_MESSAGE;
	memcpy(&elem.data.msg, msg, sizeof(YggMessage));

	if(dropBox == NULL)
		return FAILED;
	queue_push(dropBox, &elem);

	return SUCCESS;
}

int filterAndDeliver(YggMessage* msg) {

	if(filter(msg) == 0){
		short id = msg->Proto_id;
		queue_t* dropBox = getProtoQueue(id);

		queue_t_elem elem;
		elem.type = YGG_MESSAGE;
		memcpy(&elem.data.msg, msg, sizeof(YggMessage));

		if(dropBox == NULL)
			return FAILED;
		queue_push(dropBox, &elem);

		return SUCCESS;
	}
	return SUCCESS;
}

int deliverTimer(YggTimer* timer) {

	short id = timer->proto_dest;
	queue_t* dropBox = getProtoQueue(id);

	queue_t_elem elem;
	elem.type = YGG_TIMER;
	memcpy(&elem.data.timer, timer, sizeof(YggTimer));

	if(dropBox == NULL)
		return FAILED;
	queue_push(dropBox, &elem);

	return SUCCESS;
}


int deliverEvent(YggEvent* event) {

	short id = event->proto_origin;
	short eventType = event->notification_id;

	EventList* evlst = getEventList(id);

	if(evlst->nevents > eventType) {
		InterestList* interestList = evlst->eventList[eventType];
		while(interestList != NULL){

			short pid = interestList->id;
			if(pid >= 0) {
				queue_t* dropBox = getProtoQueue(pid);
				if(dropBox != NULL) {
					queue_t_elem elem;
					elem.type = YGG_EVENT;
					event->proto_dest = pid;
					memcpy(&elem.data.event, event, sizeof(YggEvent));
					queue_push(dropBox, &elem);
				}
			}
			interestList = interestList->next;
		}
	}else {
		ygg_log("YGGDRASIL RUNTIME", "DELIVER EVENT", "an event was sent that does not exist ignoring");
	}

	return SUCCESS;
}

int deliverRequest(YggRequest* req) {
	if(req->request == REQUEST){
		short id = req->proto_dest;
		queue_t* dropBox = getProtoQueue(id);

		queue_t_elem elem;
		elem.type = YGG_REQUEST;
		memcpy(&elem.data.request, req, sizeof(YggRequest));

		if(dropBox == NULL)
			return FAILED;
		queue_push(dropBox, &elem);

		return SUCCESS;
	}
	return FAILED;
}

int deliverReply(YggRequest* res) {
	if(res->request == REPLY){
		short id = res->proto_dest;

#ifdef DEBUG
		char desc[200];
		bzero(desc, 200);
		sprintf(desc, "Got a reply to %d", id);
		ygg_log("YGGDRASIL RUNTIME", "ALIVE", desc);
#endif
		queue_t* dropBox = getProtoQueue(id);

		queue_t_elem elem;
		elem.type = YGG_REQUEST;
		memcpy(&elem.data.request, res, sizeof(YggRequest));

		if(dropBox == NULL)
			return FAILED;
		queue_push(dropBox, &elem);

		return SUCCESS;
	}
	return FAILED;
}

/********************************************************
 * Other useful read Functions
 ********************************************************/

WLANAddr* getMyWLANAddr() {
	return &ch.hwaddr;
}

char* getMyAddr(char* s2){
	return wlan2asc(&ch.hwaddr, s2);
}

void setMyAddr(WLANAddr* addr){
	memcpy(addr->data, ch.hwaddr.data, WLAN_ADDR_LEN);
}

double getTestValue() {
	const char* hostname = getHostname();
	return atoi(hostname+(strlen(hostname)-2));
}

void getmyId(uuid_t id) {
	memcpy(id, myuuid, sizeof(uuid_t));
}

WLANAddr* getBroadcastAddr() {
	WLANAddr* addr = malloc(sizeof(WLANAddr));
	str2wlan((char*) addr->data, WLAN_BROADCAST);
	return addr;
}
