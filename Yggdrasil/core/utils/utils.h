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

#ifndef CORE_UTILS_H_
#define CORE_UTILS_H_

#include <stdio.h>
#include <uuid/uuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "proto_data_struct.h"


typedef enum bool_t {false, true} bool;

typedef void (*destroy_function)(void*);
typedef bool (*equal_function)(void*, void*);
typedef int (*compare_function)(void*, void*);

#define min(x,y) (((x) < (y))? (x) : (y))
#define max(x,y) (((x) > (y))? (x) : (y))


const char* getHostname();

void ygg_log_change_output(FILE* _out);

void ygg_loginit();
void ygg_log(char* proto, char* event, char* desc);
void ygg_log_multi(int n, ...);
void ygg_logflush();

void ygg_log_stdout(char* proto, char* event, char* desc);
void ygg_logflush_stdout();

void setNanoTime(struct timespec* time, unsigned long nano);

int compare_timespec(struct timespec* time1, struct timespec* time2);

void genUUID(uuid_t id);
void genStaticUUID(uuid_t id);
int setDestToAddr(YggMessage* msg, char* addr); //WLAN addr
int setDestToBroadcast(YggMessage* msg);

/**
 * Fill the paramenter addr with the broadcast address
 * @param addr A pointer to the WLANAddr that was requested
 */
void setBcastAddr(WLANAddr* addr);

int getRandomInt(int min, int max);
double getRandomProb();

#endif /* CORE_UTILS_H_ */
