#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define FILENAME "tasks.txt"

typedef struct task {
	char name[32];
	int wait_time;
	int cancelled;
	char reminder[256];
} task;

task* readTasks(char* filename, int* counter);
void* printMessage(void* name);

int main(){
	if(system("which notify-send &>/dev/null")){
		printf("Unable to find notify-send (usually part of libnotify)\n");
		exit(EXIT_FAILURE);
	}
	int counter;
	task* ts = readTasks(FILENAME, &counter);
	pthread_t* thArr = malloc(counter*sizeof(pthread_t*));
	
	for(int i = 0; i < counter; i++){
		pthread_create(&thArr[i],NULL, printMessage, (void*)&ts[i]);
	}

	pthread_join(thArr[0], NULL);

	free(thArr);
	free(ts);
	printf("%d", counter);
}

task* readTasks(char* filename, int* counter){
	task* ts = malloc(50*sizeof(task));
	FILE *fp;
	char *line = NULL;
	int len = 0;
	int read = 0;
	fp = fopen(filename, "r");
	if (fp == NULL){
		printf("I can't seem to find %s\nBailing out.\n", filename);
		exit(EXIT_FAILURE);
	}

	int pos = 0;
	while ((read = getline(&line, &len, fp)) != -1){
		if(line[0] == '#' || line[0] == '\n')
			continue;
		char* token = strtok(line, "$");
		strcpy(ts[pos].name, token);
		token = strtok(NULL, "$");
		ts[pos].wait_time = atoi(token);
		ts[pos].cancelled = 0;
		token = strtok(NULL, "$");
		strcpy(ts[pos].reminder, token);
		pos++;
	}
	fclose(fp);
	*counter = pos;
	return ts;
}

void* printMessage(void* tp){
	char buf[300];
	char* name = (*(task*)tp).name;
	int freq = (*(task*)tp).wait_time;
	char* reminder = (*(task*)tp).reminder;
	while(1){
		usleep(1000*1000*freq);
		if((*(task*)tp).cancelled)
			return;
		sprintf(buf, "notify-send %s %s", name, reminder);
		system(buf);
	}
}

