/* 
 * Operating Systems  (2INC0)  Practical Assignment.
 * Condition Variables Application.
 *
 * STUDENT_NAME_1 (STUDENT_NR_1)
 * STUDENT_NAME_2 (STUDENT_NR_2)
 *
 * Grading:
 * Students who hand in clean code that fully satisfies the minimum requirements will get an 8. 
 * Extra steps can lead to higher marks because we want students to take the initiative.
 */
 
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);	    // already implemented (see below)
static ITEM get_next_item (void);   // already implemented (see below)

static int buffer_count = 0; // Number of items in buffer
static int next_expected_item = 0; // The item that is expected in buffer

// Mutex and condition variables for critical section and buffer
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;


/* producer thread */
static void * 
producer (void * arg)
{
	(void)arg;

    while (true /* TODO: not all items produced */)
    {
        // TODO: 
        // * get the new item
		ITEM item = get_next_item();
		
		// Check if all items have been produced
        if (item == NROF_ITEMS) {
            break; 
        }
		
        rsleep (100);	// simulating all kind of activities...
		
	// TODO:
	      // * put the item into buffer[]
	//
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
		//      while not condition-for-this-producer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;
        //
        // (see condition_test() in condition_basics.c how to use condition variables)
		
		// Mutex-lock
		pthread_mutex_lock(&buffer_mutex);

		// While buffer is full and doesn't fit next expected item wait for buffer to have space
		while(buffer_count == BUFFER_SIZE || item != (next_expected_item + buffer_count)){
			pthread_cond_wait(&buffer_not_full, &buffer_mutex);
		}

		// Critical section: place the item in the buffer
        buffer[buffer_count] = item;
		buffer_count++;

        // Signal the consumer that the buffer is not empty
        pthread_cond_signal(&buffer_not_empty);

		// Mutex-unlock
        pthread_mutex_unlock(&buffer_mutex);
    }
	return (NULL);
}

/* consumer thread */
static void * 
consumer (void * arg)
{
	(void)arg;
	
    while (true /* TODO: not all items retrieved from buffer[] */)
    {
        // TODO: 
	      // * get the next item from buffer[]
	      // * print the number to stdout
        //
        // follow this pseudocode (according to the ConditionSynchronization lecture):
        //      mutex-lock;
        //      while not condition-for-this-consumer
        //          wait-cv;
        //      critical-section;
        //      possible-cv-signals;
        //      mutex-unlock;

		// Mutex-lock
		pthread_mutex_lock(&buffer_mutex);

		// Wait while buffer empty or next item is not expected
        while (buffer_count == 0 || buffer[0] != next_expected_item) {
            pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
        }

		// Retrieve the item from the buffer and replace buffer items
        ITEM item = buffer[0];
        for (int i = 1; i < buffer_count; i++) {
            buffer[i - 1] = buffer[i];
        }
        buffer_count--;
		next_expected_item++;

		// Signal producers that buffer has room
        pthread_cond_broadcast(&buffer_not_full);

		// Mutex-unlock
        pthread_mutex_unlock(&buffer_mutex);
		
		// Handle item
        rsleep (100);		// simulating all kind of activities...
		printf("%d\n", item);

		if(item == NROF_ITEMS - 1){
			break;
		}
    }
	return (NULL);
}

int main (void)
{
    // TODO: 
    // * startup the producer threads and the consumer thread
	pthread_t producers[NROF_PRODUCERS]; // Array to hold thread IDs

    // Create threads
    for (int i = 0; i < NROF_PRODUCERS; i++) {
      // Create threads executing manage_light, and pass lane information
      if (pthread_create(&producers[i], NULL, producer, NULL) != 0) {
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
      }
    }

	pthread_t consumer_id;
    if (pthread_create(&consumer_id, NULL, consumer, NULL) != 0) {
    	perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }

	pthread_join(consumer_id, NULL);

    // Wait for threads to finish
    for (int i = 0; i < NROF_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    return (0);
}

/*
 * rsleep(int t)
 *
 * The calling thread will be suspended for a random amount of time between 0 and t microseconds
 * At the first call, the random generator is seeded with the current time
 */
static void 
rsleep (int t)
{
    static bool first_call = true;
    
    if (first_call == true)
    {
        srandom (time(NULL));
        first_call = false;
    }
    usleep (random () % t);
}


/* 
 * get_next_item()
 *
 * description:
 *	thread-safe function to get a next job to be executed
 *	subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1 
 *	in arbitrary order 
 *	return value NROF_ITEMS indicates that all jobs have already been given
 * 
 * parameters:
 *	none
 *
 * return value:
 *	0..NROF_ITEMS-1: job number to be executed
 *	NROF_ITEMS:	 ready
 */
static ITEM
get_next_item(void)
{
    static pthread_mutex_t  job_mutex   = PTHREAD_MUTEX_INITIALIZER;
    static bool    jobs[NROF_ITEMS+1] = { false }; // keep track of issued jobs
    static int     counter = 0;    // seq.nr. of job to be handled
    ITEM           found;          // item to be returned
	
	/* avoid deadlock: when all producers are busy but none has the next expected item for the consumer 
	 * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
	 */
	pthread_mutex_lock (&job_mutex);

	counter++;
	if (counter > NROF_ITEMS)
	{
	    // we're ready
	    found = NROF_ITEMS;
	}
	else
	{
	    if (counter < NROF_PRODUCERS)
	    {
	        // for the first n-1 items: any job can be given
	        // e.g. "random() % NROF_ITEMS", but here we bias the lower items
	        found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
	    }
	    else
	    {
	        // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
	        found = counter - NROF_PRODUCERS;
	        if (jobs[found] == true)
	        {
	            // already handled, find a random one, with a bias for lower items
	            found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
	        }    
	    }
	    
	    // check if 'found' is really an unhandled item; 
	    // if not: find another one
	    if (jobs[found] == true)
	    {
	        // already handled, do linear search for the oldest
	        found = 0;
	        while (jobs[found] == true)
            {
                found++;
            }
	    }
	}
    	jobs[found] = true;
			
	pthread_mutex_unlock (&job_mutex);
	return (found);
}



