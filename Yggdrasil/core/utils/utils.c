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

#include "utils.h"

static char* hostname;
static pthread_mutex_t loglock;

static FILE* out;

const char* getHostname() {
	return hostname;
}

void ygg_loginit(){
	pthread_mutexattr_t atr;
	pthread_mutexattr_init(&atr);
	pthread_mutex_init(&loglock, &atr);

	out = stdout;

	FILE* f = fopen("/etc/hostname","r");
	if(f > 0) {
		struct stat s;
		if(fstat(fileno(f), &s) < 0){
			perror("FSTAT");
		}
		hostname = malloc(s.st_size+1);
		if(hostname == NULL) {

			ygg_log("YGG_RUNTIME", "INIT", "Hostname is NULL");
			ygg_logflush();
			exit(1);
		}

		memset(hostname, 0, s.st_size+1);
		if(fread(hostname, 1, s.st_size, f) <= 0){
			if(s.st_size < 8) {
				free(hostname);
				hostname = malloc(9);
				memset(hostname,0,9);
			}
			int r = rand() % 10000;
			sprintf(hostname, "host%04d", r);
		}else{

			int i;
			for(i = 0; i < s.st_size + 1; i++){
				if(hostname[i] == '\n') {
					hostname[i] = '\0';
					break;
				}
			}
		}
	} else {

		hostname = malloc(9);
		memset(hostname,0,9);

		int r = rand() % 10000;
		sprintf(hostname, "host%04d", r);
	}
	ygg_log("YGG_RUNTIME", "INIT", "Initialized logging");
}

void ygg_log_change_output(FILE* _out) {
	out = _out;
}

void ygg_log(char* proto, char* event, char* desc){
	char buffer[26];
	struct tm* tm_info;

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	//gettimeofday(&tv,NULL);
	pthread_mutex_lock(&loglock);

	tm_info = localtime(&tp.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

	fprintf(out, "<%s> TIME: %s %ld :: [%s] : [%s] %s\n", hostname, buffer, tp.tv_nsec, proto, event, desc);
	//printf("<%s> TIME: %ld %ld :: [%s] : [%s] %s\n", hostname, tv.tv_sec, tv.tv_usec, proto, event, desc);
	pthread_mutex_unlock(&loglock);
}

void ygg_log_multi(int n, ...) {
	char buffer[26];
	struct tm* tm_info;

	struct timeval tv;
	gettimeofday(&tv,NULL);
	pthread_mutex_lock(&loglock);

	tm_info = localtime(&tv.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

	fprintf(out, "<%s> TIME: %s %ld :: ", hostname, buffer, tv.tv_usec);

	va_list ap;

	va_start(ap, n);

	int i = 0;
	for(; i < n; i++)
		fprintf(out, "%s ", va_arg(ap, char*));

	fprintf(out, "\n");

	pthread_mutex_unlock(&loglock);
}

void ygg_logflush() {
	char buffer[26];
	struct tm* tm_info;
	struct timeval tv;
	gettimeofday(&tv,NULL);
	tm_info = localtime(&tv.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
	fprintf(out, "<%s> TIME: %s %ld :: [%s] : [%s] %s\n", hostname, buffer, tv.tv_usec, "YGG_RUNTIME", "QUIT", "Stopping process");
	//printf("<%s> TIME: %ld %ld :: [%s] : [%s] %s\n", hostname, tv.tv_sec, tv.tv_usec, "YGG_RUNTIME", "QUIT", "Stopping process");
	fflush(out);
}

void ygg_log_stdout(char* proto, char* event, char* desc) {
	char buffer[26];
	struct tm* tm_info;

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	//gettimeofday(&tv,NULL);
	pthread_mutex_lock(&loglock);

	tm_info = localtime(&tp.tv_sec);

	strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);

	printf("<%s> TIME: %s %ld :: [%s] : [%s] %s\n", hostname, buffer, tp.tv_nsec, proto, event, desc);
	//printf("<%s> TIME: %ld %ld :: [%s] : [%s] %s\n", hostname, tv.tv_sec, tv.tv_usec, proto, event, desc);
	pthread_mutex_unlock(&loglock);
}

void ygg_logflush_stdout() {
	if(out != stdout) {
		char buffer[26];
		struct tm* tm_info;
		struct timeval tv;
		gettimeofday(&tv,NULL);
		tm_info = localtime(&tv.tv_sec);

		strftime(buffer, 26, "%Y:%m:%d %H:%M:%S", tm_info);
		printf("<%s> TIME: %s %ld :: [%s] : [%s] %s\n", hostname, buffer, tv.tv_usec, "YGG_RUNTIME", "QUIT", "Stopping process");
		//printf("<%s> TIME: %ld %ld :: [%s] : [%s] %s\n", hostname, tv.tv_sec, tv.tv_usec, "YGG_RUNTIME", "QUIT", "Stopping process");
		fflush(stdout);
	}
}

#define NANOSEC 1*1000*1000*1000

void setNanoTime(struct timespec* time, unsigned long nano) {

	unsigned long current_nano = time->tv_nsec;
	current_nano += nano;
	while(current_nano >= NANOSEC) {
		time->tv_sec ++;
		current_nano -= NANOSEC;
	}
	time->tv_nsec = current_nano;

}

int compare_timespec(struct timespec* time1, struct timespec* time2) {
	if(time1->tv_sec > time2->tv_sec)
		return 1;
	else if(time1->tv_sec == time2->tv_sec) {
		if(time1->tv_nsec > time2->tv_nsec)
			return 1;
		else if(time1->tv_nsec == time2->tv_nsec)
			return 0;
		else
			return -1;
	}else
		return -1;
}

void genStaticUUID(uuid_t id) {
	char* uuid_txt = malloc(37);
	memset(uuid_txt, 0, 37);
	sprintf(uuid_txt, "66600666-1001-1001-1001-0000000000%s", hostname+(strlen(hostname)-2));
	if(uuid_parse(uuid_txt, id) != 0)
		genUUID(id);
	else
		ygg_log("UTILS", "GEN STATIC UUID", uuid_txt);
	free(uuid_txt);
}

void genUUID(uuid_t id){
	int tries = 0;
	while(uuid_generate_time_safe(id) < 0 && tries < 3){
		tries ++;
	}
}

int setDestToAddr(YggMessage* msg, char* addr){

	if(msg->header.type != MAC)
		return FAILED;
	memcpy(msg->header.dst_addr.mac_addr.data, addr, WLAN_ADDR_LEN);
	return SUCCESS;
}

int setDestToBroadcast(YggMessage* msg) {
	char mcaddr[WLAN_ADDR_LEN];
	str2wlan(mcaddr, WLAN_BROADCAST); //translate addr to machine addr
	return setDestToAddr(msg, mcaddr);
}

void setBcastAddr(WLANAddr* addr){
	str2wlan((char*) addr->data, WLAN_BROADCAST);
}

// Both min and max are included in the possible values
int getRandomInt(int min, int max) {
	return (rand() % (max - min + 1)) + min;
}

double getRandomProb() {
	return getRandomInt(0, 100) / 100.0;
}

