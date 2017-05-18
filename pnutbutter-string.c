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
	char reminder[256];
} task;

task* ts;
pthread_t* thArr;
int* counter;
char* filename;

task* readTasks(char* filename);
void* printMessage(void* name);
void reloadTasks(int n);
void cleanup(int n);

int main(){
	// Quit if notify-send is not in $PATH
	if(system("which notify-send &>/dev/null")){
		printf("Unable to find notify-send (usually part of libnotify)\n");
		exit(EXIT_FAILURE);
	}

	char fnbuf[255];
	filename = fnbuf;
	strncpy(filename, FILENAME, 255);
	int count;
	counter = &count;

	signal(SIGUSR1, reloadTasks);
	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);
	
	// Get pointer to array of tasks
	ts = readTasks(FILENAME);
	// Allocate memory for an array of pthreads
	thArr = malloc(*counter*sizeof(pthread_t*));
	
	// Create enough threads for all the reminders
	for(int i = 0; i < *counter; i++){
		pthread_create(&thArr[i],NULL, printMessage, (void*)&ts[i]);
	}

	int numiNotifyEvents;
	int iNotifyFileDesc;
	int iNotifyWatchDesc;
	char iNotifyBuf[EVENT_BUF_LEN];
	int modified = 0;

	// Prepare for inotify file watch
	iNotifyFileDesc = inotify_init();
	if (iNotifyFileDesc<0){
		printf("There was an error setting up inotify, that functionality will not be available\n");
	}
	
	// Add reminder file to watch
	iNotifyWatchDesc = inotify_add_watch( iNotifyFileDesc, FILENAME, IN_MODIFY);		
	// Keep catching modifications forever
	for(;;){
		int i = 0;
		numiNotifyEvents = read( iNotifyFileDesc, iNotifyBuf, EVENT_BUF_LEN);
		while ( i < numiNotifyEvents) {
			struct inotify_event* event = (struct inotify_event*) &iNotifyBuf[i];
			if (event->mask & IN_MODIFY)
				modified = 1;
			i += sizeof(struct inotify_event) + event->len;
		}
		if(modified)
			reloadTasks(0);
		
	}
	// Remove the watch and close the file descriptor
	inotify_rm_watch(iNotifyFileDesc,iNotifyWatchDesc);
	close(iNotifyFileDesc);

	// Suspend execution (block until signal) in cases where inotify failed
	pause();

	// Cleanup before quitting (though it is unlikely to ever reach here)
	cleanup(0);
}

/*
 * Read tasks from filename and return a pointer to an array of tasks. Also 
 * maintain a count of how many reminders were loaded
 */
task* readTasks(char* filename2){
	// Allocate enough memory for 50 tasks
	task* ts = malloc(50*sizeof(task));
	FILE *fp;
	char *line = NULL;
	int len = 0;
	int read = 0;
	// Open file for reading and check for errors
	fp = fopen(filename2, "r");
	if (fp == NULL){
		printf("I can't seem to find %s\nBailing out.\n", filename2);
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
	for(;;){
		usleep(1000*1000*freq);
		// Check for cancelled thread (not yet used)
		sprintf(buf, "notify-send %s %s", name, reminder);
		system(buf);
	}
}

/*
 * Kill all threads in thArr, reload tasks from filename into ts, then restart
 * threads and update counter
 */
void reloadTasks(int n){
	printf("Caught a modification to %s, reloading reminders\n", FILENAME);
	for(int i = 0; i < *counter; i++){
		pthread_cancel(thArr[i]);
	}
	ts = readTasks(filename);
	printf("Tasks updated\n");
	for(int i = 0; i < *counter; i++){
		pthread_create(&thArr[i], NULL, printMessage, (void*)&ts[i]);
	}
}

/*
 * Kill all currently running threads and free all dynamically allocated
 * memory before quitting gracefully.
 */
void cleanup(int n){
	printf("Caught termination/interruption signal, cleaning up\n");
	for(int i = 0; i < *counter; i++){
		pthread_cancel(thArr[i]);
	}
	printf("Killed %d threads.\n", *counter);
	free(thArr);
	free(ts);
	printf("Goodbye!\n");
	exit(0);
}
