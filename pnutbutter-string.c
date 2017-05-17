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
	// Quit if notify-send is not in $PATH
	if(system("which notify-send &>/dev/null")){
		printf("Unable to find notify-send (usually part of libnotify)\n");
		exit(EXIT_FAILURE);
	}
	// Hold number of tasks found in file
	int counter;
	// Get pointer to array of tasks
	task* ts = readTasks(FILENAME, &counter);
	// Allocate memory for an array of pthreads
	pthread_t* thArr = malloc(counter*sizeof(pthread_t*));
	
	// Create enough threads for all the reminders
	for(int i = 0; i < counter; i++){
		pthread_create(&thArr[i],NULL, printMessage, (void*)&ts[i]);
	}

	// Use blocking pthread_join to allow threads to loop independently
	pthread_join(thArr[0], NULL);

	// Free some dynamically allocated memory 
	free(thArr);
	free(ts);
}

/*
 * Read tasks from filename and return a pointer to an array of tasks. Also 
 * maintain a count of how many reminders were loaded
 */
task* readTasks(char* filename, int* counter){
	// Allocate enough memory for 50 tasks
	task* ts = malloc(50*sizeof(task));
	FILE *fp;
	char *line = NULL;
	int len = 0;
	int read = 0;
	// Open file for reading and check for errors
	fp = fopen(filename, "r");
	if (fp == NULL){
		printf("I can't seem to find %s\nBailing out.\n", filename);
		exit(EXIT_FAILURE);
	}

	int pos = 0;
	// Read each line
	while ((read = getline(&line, &len, fp)) != -1){
		// Ignore comments and blank lines
		if(line[0] == '#' || line[0] == '\n')
			continue;
		// Tokenize and store to struct
		char* token = strtok(line, "$");
		strcpy(ts[pos].name, token);
		token = strtok(NULL, "$");
		ts[pos].wait_time = atoi(token);
		ts[pos].cancelled = 0;
		token = strtok(NULL, "$");
		strcpy(ts[pos].reminder, token);
		pos++;
	}
	// Close the file
	fclose(fp);
	// Update the counter
	*counter = pos;
	return ts;
}

/*
 * Function to run in each pthread. Call libnotify and wait the required amount
 * of seconds between successive reminders.
 */
void* printMessage(void* tp){
	// Buffer for the next command to be stored in
	char buf[300];
	char* name = (*(task*)tp).name;
	int freq = (*(task*)tp).wait_time;
	char* reminder = (*(task*)tp).reminder;
	// Repeatedly print reminders and sleep
	while(1){
		usleep(1000*1000*freq);
		if((*(task*)tp).cancelled)
			return;
		sprintf(buf, "notify-send %s %s", name, reminder);
		system(buf);
	}
}

