/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2018
 *********************************************************/

#include "commands.h"

#define child_output "output-child-changed.log"

typedef struct experience_instance_ {
	char* binary;
	int process_id;
	int output_pipe;
} experience_instance;

static experience_instance* child = NULL;

static FILE* outfile;

void kill_application();

void clear_child_process() {
	if(child != NULL) {
		int i = 0;
		int status = 0;
		for(; i <= 5; i++) {
			if(waitpid(child->process_id, NULL, 0) == child->process_id) {
				status = 1;
				break;
			} else {
				ygg_log_stdout("COMMAND", "CLEAR_CHILD", "no child dead.");
			}
		}
		if(status == 0) { //shotgun kill
			kill(child->process_id, 3);
		}

		if(child->binary != NULL)
			free(child->binary);

		free(child);
		child = NULL;
	}
}

void create_child_process(char* binary, int process_id, int output_pipe) {
	if(child != NULL) {
		kill_application();
		clear_child_process();
	}
	child = malloc(sizeof(experience_instance));
	child->binary = malloc(strlen(binary) + 1);
	bzero(child->binary, strlen(binary) + 1);
	memcpy(child->binary, binary, strlen(binary) + 1);
	child->process_id = process_id;
	child->output_pipe = output_pipe;
}

int testChildPipe() {
	fd_set child_pipe;
	FD_ZERO(&child_pipe);
	FD_SET(child->output_pipe, &child_pipe);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	return select(child->output_pipe+1, NULL, &child_pipe, NULL, &timeout);
}

int isChildAlive() {
	if(child != NULL) {
		if(kill(child->process_id, 0) == 0 && testChildPipe() == 1)
			return 1;
	}
	return 0;
}

int do_command(char* executable, char* exec_args[], int nargs) {
	char* args[2+nargs];
	args[0] = executable;
	int i;
	for(i = 0; i < nargs; i++){
		args[1+i] = exec_args[i];
	}
	args[1+nargs] = NULL;

	int child_pid, wpid;
	int status = 0;

	if((child_pid = fork()) == 0) {

		execvp(executable, args);

		exit(1);
	}

	if((wpid = wait(&status)) < 0) {
		//ERROR
		return wpid;
	} else {
		//SUCESS
		return wpid;
	}

}

int run_application(char* executable, char* args[]) {

	int pd[2];

	if(pipe(pd) < 0) {
		perror("Pipe failed");
		return -1;
	}
	int pid;
	if((pid = fork()) == 0) {
		if(close(0) != 0)
			perror("close 0"); //close stdin
		if(dup(pd[0]) != 0)
			perror("dup 0");

		FILE * outfile = fopen(child_output, "w");
		if(outfile < 0)
			perror("file");

		int out = fileno(outfile);

		if(close(1) != 0)
			perror("close 1"); //close stdout
		int d;
		if((d = dup(out)) != 1)
			perror("dup 1");

		fprintf(stderr, "%d\n", d);
		close(pd[1]);
		//		close(pd[0]);
		//		close(out);

		int ret = execvp(executable, args);
		fprintf(stderr, "Error executing execvp for executable: %s. Error code: %d\n", executable, ret);
		int i = 0;
		while(args[i] != NULL) {
			fprintf(stderr, "Argument: %s\n", args[i]);
			i++;
		}
		fflush(stderr);
		perror("execvp failed");

		exit(1);
	}

	close(pd[0]);

	create_child_process(executable, pid, pd[1]);

	return 0;
}

void kill_application() {
	if(child != NULL) {
		kill(child->process_id, 3);
		close(child->output_pipe);
		clear_child_process();
	}
}

void change_link(int pi_number) {

	YggRequest req;
	YggRequest_init(&req, ANY_PROTO, PROTO_TOPOLOGY_MANAGER, REQUEST, CHANGE_LINK); //TODO PROTO ORIGIN??
	YggRequest_addPayload(&req, (void*) &pi_number, sizeof(int));

	deliverRequest(&req);

	YggRequest_freePayload(&req);
}

void change_val(int new_val) {

	double val = (double) new_val;

	YggEvent ev;
	YggEvent_init(&ev, ANY_PROTO, VALUE_CHANGE);
	YggEvent_addPayload(&ev, (void*) &val, sizeof(double));

	deliverEvent(&ev);

	YggEvent_freePayload(&ev);
}

void transfer_results(int towrite){

	FILE * resultsfile = fopen(child_output, "r");

	int results = fileno(resultsfile);

	char buffer[1024];
	int count;
	while ((count = read(results, buffer, 1024)) > 0)
	{
		if (write(towrite, buffer, count) <= 0)
		{
			perror("write"); //  at least
			break;
		}
	}

	fclose(resultsfile);
}

int copy_file_from_to(char* src, char* dst) {

	sleep(1);

	char* cp[4];
	cp[0] = "mv";
	cp[1] = src;
	cp[2] = dst;
	cp[3] = NULL;

	int cp_pid, wpid;
	int status = 0;

	if((cp_pid = fork()) == 0){

		execvp(cp[0], cp);
		perror("cp failed");

		exit(1);
	}

	if((wpid = waitpid(cp_pid, &status, 0)) < 0) {
		ygg_log_stdout("COMMAND", "COPY_FILE_FROM_TO", "waitpid failed.");
		return wpid;
	} else {
		//SUCESS
		return wpid;
	}
}

int my_number = -1;

static void get_number() {
	if(my_number == -1){
		my_number = getTestValue();
	}
}

void* start_experience(char* command) {

	//sleep(4); //Should we sleep before an experiment?

	int command_len = strlen(command);
	char* tmp = malloc(command_len + 1);
	bzero(tmp, command_len + 1);
	memcpy(tmp, command, command_len);
	char* l = tmp;

	char* proto = strsep(&l, " ");

	int proto_id_to_start = atoi(proto);

	outfile = fopen(child_output, "w");
	if(outfile < 0) {
		perror("file");
	}
	else {
		ygg_log_change_output(outfile);

		//TODO how to parse arg?
		startProtocol(proto_id_to_start, l); //TODO TEST

		//Restart support Protocols
		YggEvent ev;
		YggEvent_init(&ev, ANY_PROTO, RESTART_PROTOS);
		deliverEvent(&ev);
	}
	return NULL;
}

void* old_start_experience(char* command) {

	//sleep(4); //Should we sleep before an experiment?

	wordexp_t words;
	int r = wordexp(command, &words, 0);

	if(r == 0) {
		run_application(words.we_wordv[0], words.we_wordv);
		wordfree(&words);
		free(command);
	} else {
		perror("words failed");
	}

	return NULL;
}

void* kill_child_application (void* tostore) {
	kill_application();
	copy_file_from_to(child_output, (char*)tostore);
	free(tostore);
	return NULL;
}

void stop_experience(char* command) {
	get_number();

	int command_len = strlen(command);
	char* tmp = malloc(command_len + 1);
	bzero(tmp, command_len + 1);
	memcpy(tmp, command, command_len);
	char* l = tmp;

	char* proto = strsep(&l, " ");
	int proto_to_stop = atoi(proto);

	//TODO How to deal with output files (can be ignored for now..)
	char* tostore = strsep(&l, " ");

	int fileNameSize = strlen(tostore) + 1;
	char* fileName = malloc(fileNameSize);
	memcpy(fileName, tostore, fileNameSize);


	char* pinumber;
	int all = 1;
	while((pinumber = strsep(&l, " ")) != NULL) {
		all = 0;
		if(atoi(pinumber) == my_number){
			stopProtocol(proto_to_stop); //TODO TEST
			ygg_logflush();
			ygg_log_change_output(stdout);
			copy_file_from_to(child_output, fileName);

			//Stop support protocols
			YggEvent ev;
			YggEvent_init(&ev, ANY_PROTO, HALT_PROTOS);
			deliverEvent(&ev);

			fclose(outfile); //TODO test if error
		}
	}

	if(all) {
		stopProtocol(proto_to_stop);
		ygg_logflush();
		ygg_log_change_output(stdout);
		copy_file_from_to(child_output, fileName);

		//Stop support protocols
		YggEvent ev;
		YggEvent_init(&ev, ANY_PROTO, HALT_PROTOS);
		deliverEvent(&ev);

		fclose(outfile);
	}

	free(tmp);

}

void old_stop_experience(char* command) {
	get_number();

	int command_len = strlen(command);
	char* tmp = malloc(command_len + 1);
	bzero(tmp, command_len + 1);
	memcpy(tmp, command, command_len);
	char* l = tmp;

	char* tostore = strsep(&l, " ");

	int fileNameSize = strlen(tostore) + 1;
	char* fileName = malloc(fileNameSize);
	memcpy(fileName, tostore, fileNameSize);


	char* pinumber;
	int all = 1;
	while((pinumber = strsep(&l, " ")) != NULL) {
		all = 0;
		if(atoi(pinumber) == my_number){
			if(child != NULL) {
				if(isChildAlive()) {
					pthread_t kill_child;
					pthread_create(&kill_child, NULL, (void * (*)(void *)) kill_child_application, (void*) fileName);
					pthread_detach(kill_child);
				} else {
					close(child->output_pipe);
					clear_child_process();
				}
			}
			return;
		}
	}

	if(all) {
		pthread_t kill_child;
		pthread_create(&kill_child, NULL, (void * (*)(void *)) kill_child_application, (void*) fileName);
		pthread_detach(kill_child);
	}

	free(tmp);

}

void process_change_link(char* command) {
	get_number();
	int command_len = strlen(command);
	char* tmp = malloc(command_len + 1);
	bzero(tmp, command_len + 1);
	memcpy(tmp, command, command_len);
	char* l = tmp;

	char* pi_number;
	while((pi_number = strsep(&l, " ")) != NULL) {
		int pi1 = atoi(pi_number);
		int pi2 = atoi(strsep(&l, " "));

		if(my_number == pi1)
			change_link(pi2);
		else if(my_number == pi2)
			change_link(pi1);
	}

	free(tmp);
}

void process_change_val(char* command) {
	get_number();
	int command_len = strlen(command);
	char* tmp = malloc(command_len + 1);
	bzero(tmp, command_len + 1);
	memcpy(tmp, command, command_len);
	char* l = tmp;

	char* pi_number;
	while((pi_number = strsep(&l, " ")) != NULL) {
		int pi = atoi(pi_number);
		int newval = atoi(strsep(&l, " "));

		if(my_number == pi)
			change_val(newval);
	}

	free(tmp);
}

void sudo_reboot() {

	//	if(child != NULL && isChildAlive()) {
	//		char* cmd = "unexpectedexit.log";
	//		stop_experience(cmd);
	//	}

	if(fork() == 0) {
		char* order = "shutdown";
		char* args[] = {"shutdown", "-r", NULL};


		execvp(order, args);
		perror("shutdown failed");

		exit(1);

	}
	ygg_logflush();

}

void sudo_shutdown() {

	//	if(child != NULL && isChildAlive()) {
	//		char* cmd = "unexpectedexit.log";
	//		stop_experience(cmd);
	//	}

	if(fork() == 0) {
		char* order = "shutdown";
		char* args[] = {"shutdown", NULL};


		execvp(order, args);
		perror("shutdown failed");

		exit(1);

	}
	ygg_logflush();

}

