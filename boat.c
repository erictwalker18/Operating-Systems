/*
* H2SO4 creation file
* Authors: Eric Walker, Isaac Garfinkle
* Last Modified: 4/26/2016
*/

#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

// Locks for each condition variable
pthread_mutex_t boat_mutex;
pthread_mutex_t children_start_mutex;
pthread_mutex_t children_end_mutex;
pthread_mutex_t adult_mutex;
pthread_mutex_t done_mutex;
// Condition variables
pthread_cond_t boat;
pthread_cond_t start;
pthread_cond_t end;
pthread_cond_t adult_cond;
// Semaphores for initializing and exiting
sem_t* threads_initialized;
sem_t* threads_at_end;

// Shared variables for the threads (not to be used in main)
int adults_start=0;
int children_start=0;
int children_end=0;
int boat_pass=0;
int done=0;

void* adult(void*);
void* child(void*);
void initSynch();
void closeSynch();
// local function just used to randomly shuffle creation order of atoms
void shuffle(int*, int);
// local function used to check that semaphores are available
void check_sem(sem_t**, char*);



int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Need to put in adult and children numbers\n");
    exit(0);
  }
  // set # to create of each atom (atoi converts a string to an int)
  const int num_adults = atoi(argv[1]);
  const int num_children = atoi(argv[2]);
  const int total = num_adults+num_children;

  initSynch();

  //--------------Create threads -----------------
  // add desired number of each type of person
  int order[total];
  int i;
  for (i=0; i<num_adults; i++) {
    order[i] = 1;
  }
  for (; i<num_adults+num_children; i++) {
    order[i] = 2;
  }

  // order now has # of 1's and 2's to reflect # of 2 types of people,
  // so just need to shuffle to get random order
  shuffle(order, total);

  // now create threads in shuffled order
  pthread_t people[total];
  long adult_counter=0;
  long child_counter=0;
  for (i=0; i<total; i++) {
    if (order[i]==1) {
      adult_counter++;
      pthread_create(&people[i], NULL, adult, (void *)adult_counter);
    }
    else if (order[i]==2) {
      child_counter++;
      pthread_create(&people[i], NULL, child, (void*)child_counter);
    }
    else printf("Something went horribly wrong!!!\n");
  }
  //--------------End create threads -----------------

  // Wait for all the threads to be created, then make the
  // boat show up
  for (i=0; i<total; i++) {
    sem_wait(threads_initialized);
  }

  // Signal a child to start
  pthread_cond_signal(&start);

  // Wait for everyone to get across, then exit
  sem_wait(threads_at_end);

  exit(0);

}


// ---------------------------Adult and Child threads---------------------------
void* adult(void* args) {
  long my_id = (long)args;
  // tell others you're ready
  printf("Adult %ld arrived on Oahu.\n", my_id);
  fflush(stdout);
  sem_post(threads_initialized);

  // Get in the adult queue
  pthread_mutex_lock(&adult_mutex);
  adults_start++;
  pthread_cond_wait(&adult_cond, &adult_mutex);
  pthread_mutex_unlock(&adult_mutex);

  // once out of the adult queue, there must be 1 or 0 children
  // on the start island, so row over, signal a child to row back
  // unless there is no one left on the start island

  printf("Adult %ld getting in boat on Oahu.\n", my_id);
  fflush(stdout);
  //Once the adult gets on the boat, it will decrease the adults on the island
  pthread_mutex_lock(&adult_mutex);
  adults_start--;
  pthread_mutex_unlock(&adult_mutex);

  printf("Adult %ld rowing boat from Oahu to Molokai.\n", my_id);
  fflush(stdout);

  printf("Adult %ld leaving boat on Molokai.\n", my_id);
  fflush(stdout);
  // Once adult is on the end (molokai) island, it will signal a child
  // to row back to the start (oahu) island, then exit
  pthread_cond_signal(&end);

  pthread_exit(NULL);
}

// Child thread
// Loops through getting on boat until everyone is on the last island
void* child(void* args) {
  long my_id = (long)args;

  pthread_mutex_lock(&children_start_mutex);
  children_start++;
  // initialize where you are, and tell main you're ready
  printf("Child %ld arrive on start (Oahu)\n", my_id);
  fflush(stdout);
  sem_post(threads_initialized);
  // get in the starting queue, but let other children start
  pthread_cond_wait(&start, &children_start_mutex);
  pthread_mutex_unlock(&children_start_mutex);

  while(1==1) {
    // If there are exactly 2 children and 0 adults, we'll be done after
    // This boat trip, so save that.
    pthread_mutex_lock(&children_start_mutex);
    pthread_mutex_lock(&adult_mutex);
    pthread_mutex_lock(&done_mutex);
    if (adults_start==0 && children_start == 2) {
      done=1;
    }
    pthread_mutex_unlock(&done_mutex);
    pthread_mutex_unlock(&adult_mutex);
    pthread_mutex_unlock(&children_start_mutex);


    pthread_mutex_lock(&boat_mutex);
    // Once child has been signaled that there's a spot to go to
    // the child should get on the boat, go in pairs
    printf("Child %ld getting in boat on Oahu.\n", my_id);
    fflush(stdout);
    boat_pass++;
    //Signal another child to get in the boat.
    if (boat_pass<2) {
      pthread_cond_signal(&start);
    }
    // Wait until another child gets in the boat.
    while (boat_pass<2) {
      pthread_cond_wait(&boat, &boat_mutex);
    }
    // If another child is already in the boat, signal them to wake up.
    pthread_cond_signal(&boat);

    //row the boat
    printf("Child %ld rowing boat from Oahu to Molokai.\n", my_id);
    fflush(stdout);
    pthread_mutex_lock(&children_start_mutex);
    children_start--;
    pthread_mutex_unlock(&children_start_mutex);


    // Leave the boat and get on the end island, but wait for other child
    // to row to end
    if (boat_pass>1) {
      pthread_cond_wait(&boat, &boat_mutex);
    }
    boat_pass--;
    pthread_cond_signal(&boat);
    pthread_mutex_unlock(&boat_mutex);
    // get out of the boat
    printf("Child %ld leaving boat on Molokai.\n", my_id);
    fflush(stdout);


    pthread_mutex_lock(&children_end_mutex);
    children_end++;

    // If we decided we're done, then tell main we're done, and
    // let main run and exit the program.
    pthread_mutex_lock(&done_mutex);
    if (done == 1) {
      sem_post(threads_at_end);
      pthread_cond_wait(&boat, &done_mutex);
    }

    // signal the end queue to send one back, then wait in the queue
    pthread_cond_signal(&end);
    pthread_mutex_unlock(&done_mutex);

    pthread_cond_wait(&end, &children_end_mutex);
    // release the lock
    pthread_mutex_unlock(&children_end_mutex);



    // go back to first island
    // ...................
    pthread_mutex_lock(&boat_mutex);
    // Once child has been signaled that there's a spot to go to
    // the child should get on the boat, then go back alone.
    printf("Child %ld getting in boat on Molokai.\n", my_id);
    fflush(stdout);
    boat_pass++;
    pthread_mutex_unlock(&boat_mutex);

    //row the boat
    printf("Child %ld rowing boat from Molokai to Oahu.\n", my_id);
    fflush(stdout);

    pthread_mutex_lock(&children_start_mutex);
    children_end--;
    pthread_mutex_unlock(&children_start_mutex);

    // get out of the boat
    printf("Child %ld leaving boat on Oahu.\n", my_id);
    fflush(stdout);

    // Leave the boat and get on the start island.
    pthread_mutex_lock(&boat_mutex);
    boat_pass--;
    pthread_mutex_unlock(&boat_mutex);

    // Leave the boat and get on the start island, signal children
    // that there is a boat here to get into, if no children besides
    // self, then signal an adult to go.
    pthread_mutex_lock(&children_start_mutex);
    children_start++;
    if (children_start>1) {
      pthread_cond_signal(&start);
    } else {
      pthread_cond_signal(&adult_cond);
    }
    // Get in the start queue
    pthread_cond_wait(&start, &children_start_mutex);
    pthread_mutex_unlock(&children_start_mutex);

  }

  pthread_exit(NULL);
}

// -------------------------End Adult and Child threads-------------------------

void initSynch() {
  //Create all the mutexes and condition variables
  pthread_mutex_init(&boat_mutex, NULL);
  pthread_mutex_init(&children_start_mutex, NULL);
  pthread_mutex_init(&children_end_mutex, NULL);
  pthread_mutex_init(&adult_mutex, NULL);
  pthread_mutex_init(&done_mutex, NULL);
  pthread_cond_init(&boat, NULL);
  pthread_cond_init(&start, NULL);
  pthread_cond_init(&end, NULL);
  pthread_cond_init(&adult_cond, NULL);

  // Create start/end semaphores
  threads_initialized = sem_open("startsmphr", O_CREAT|O_EXCL, 0466, 0);
  threads_at_end = sem_open("endsmphr", O_CREAT|O_EXCL, 0466, 0);

  check_sem(&threads_initialized, "startsmphr");
  check_sem(&threads_at_end, "endsmphr");
}

void shuffle(int* intArray, int arrayLen) {
  int i=0;
  for (i=0; i<arrayLen; i++) {
    int r = rand()%arrayLen;
    int temp = intArray[i];
    intArray[i] = intArray[r];
    intArray[r] = temp;
  }
}

void check_sem(sem_t** semaphore, char* sem_string) {
  // *** opening semaphores using C on a unix system creates an actual semaphore file that is not
  // automatically closed or deleted when the program exits.  As long as you close the semaphore AND
  // unlink the filename you gave in sem_open, you won't have any problems, but if you forget, or if
  // your program crashes in the middle or you have to quit using ctrl-c or something similar, you
  // will get an error when you try to run your program again because the semaphore file will already
  // exist. ***
  // The following code handles the above issue by deleting the sempahore file if it already existed
  // and then creating a new one.  It also handles issues where you are not allowed to create/open a
  // new file, e.g. you do not have permission at the given location.
  while (*semaphore==SEM_FAILED) {
    if (errno == EEXIST) {
      //printf("semaphore %s already exists, unlinking and reopening\n", sem_string);
      //fflush(stdout);
      sem_unlink(sem_string);
      *semaphore = sem_open(sem_string, O_CREAT|O_EXCL, 0466, 0);
    }
    else {
      printf("semaphore could not be opened, error # %d\n", errno);
      fflush(stdout);
      exit(1);
    }
  }
}
