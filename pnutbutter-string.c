#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>

#define FILENAME "tasks.txt"
#define EVENT_SIZE (sizeof (struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

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

	int numiNotifyEvents;
	int iNotifyFileDesc;
	int iNotifyWatchDesc;
	char iNotifyBuf[EVENT_BUF_LEN];

	// Prepare for inotify file watch
	iNotifyFileDesc = inotify_init();
	if (iNotifyFileDesc<0){
		printf("There was an error setting up inotify, that functionality will not be available\n");
	}
	// Add reminder file to watch
	iNotifyWatchDesc = inotify_add_watch( iNotifyFileDesc, "FILENAME", IN_MODIFY);		
	// Keep catching modifications forever
	for(;;){
		int i = 0;
		numiNotifyEvents = read( iNotifyFileDesc, iNotifyBuf, EVENT_BUF_LEN);
		while ( i < numiNotifyEvents) {
			struct inotify_event* event = (struct inotify_event*) &iNotifyBuf[i];
			if (event->mask & IN_MODIFY){
				// Placeholder. Re-reading tasks and preparing threads goes here FIXME
				printf("Caught a modification\n");
			}
			i += sizeof(struct inotify_event) + event->len;
		}
	}
	// Romove the watch and close the file descriptor
	inotify_rm_watch(iNotifyFileDesc,iNotifyWatchDesc);
	close(iNotifyFileDesc);

	// Suspend execution (block until signal)
	pause();

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
	task* t = (task*)tp;
	char* name = t->name;
	int freq = t->wait_time;
	char* reminder = t->reminder;
	// Repeatedly print reminders and sleep
	while(1){
		usleep(1000*1000*freq);
		// Check for cancelled thread (not yet used)
		if(t->cancelled)
			return;
		sprintf(buf, "notify-send %s %s", name, reminder);
		system(buf);
	}
}

