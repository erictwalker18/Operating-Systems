/*
* H2SO4 creation file
* Authors: Eric Walker, Isaac Garfinkle
* Last Modified: 4/26/2016
*/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include "H2SO4.h"

void delay(int);

void check_sem(sem_t**, char*);

// local function just used to randomly shuffle creation order of atoms
void shuffle(int*, int);


// decalre hydrogen, sulfur, and oxygen semaphores as a global variable so
// they are shared and accessible from all threads
sem_t* hydro_sem;
sem_t* oxy_sem;
sem_t* hydro_used_sem;
sem_t* oxy_used_sem;
sem_t* wait_to_leave_sem;
sem_t* sulfur_lock_sem;

int main(int argc, char* argv[]) {
  openSems();

  // set # to create of each atom (atoi converts a string to an int)
  const int numhydros = atoi(argv[1]);
  const int numsulfurs = atoi(argv[2]);
  const int numoxys = atoi(argv[3]);
  const int total = numoxys+numhydros+numsulfurs;

  // seed the random number generator with the current time
  srand(time(NULL));

  // add desired number of each type of atom (represented as a simple int) to an array
  // oxygen is represented as 1, hydrogen as 2, and sulfur as 3
  int order[total];
  int i;
  for (i=0; i<numoxys; i++) {
    order[i] = 1;
  }
  for (; i<numhydros+numoxys; i++) {
    order[i] = 2;
  }
  for (; i<total; i++) {
    order[i] = 3;
  }

  // order now has # of 1's, 2's, and 3's to reflect # of 3 types of atoms,
  // so just need to shuffle to get random order
  shuffle(order, total);

  // now create threads in shuffled order
  pthread_t atoms[total];
  for (i=0; i<total; i++) {
    if (order[i]==1) pthread_create(&atoms[i], NULL, oxygen, NULL);
    else if (order[i]==2) pthread_create(&atoms[i], NULL, hydrogen, NULL);
    else if (order[i]==3) pthread_create(&atoms[i], NULL, sulfur, NULL);
    else printf("something went horribly wrong!!!\n");
  }

  // join all threads before letting main exit
  for (i=0; i<total; i++) {
    pthread_join(atoms[i], NULL);
  }

  closeSems();
  return 0;
}


/*
 * Produces an H atom after a random delay, notifies sem that another H is here, exits
 * no arguments, always returns 0
 */
void* hydrogen(void* args) {

  // produce a hydrogen atom, takes a random amount of work with an upper bound
  delay(rand()%5000);
  printf("hydrogen produced\n");
  fflush(stdout);

  // post (call up) on hydrogen semaphore to signal that a hydrogen atom
  // has been produced, then wait for it to be used in a molecule.
  sem_post(hydro_sem);
  sem_wait(hydro_used_sem);

  printf("hydrogen exit\n");
  fflush(stdout);
  sem_post(wait_to_leave_sem);
  return (void*) 0;
}

/*
 * Produces an O atom after a random delay, checks if 2 H atoms have already been
 * produced, if not waits for them to be produced, then creates H2O molecule and exits
 * uses no arguments, always returns 0
 */
void* oxygen(void* args) {

  // produce an oxygen atom, takes a random amount of work with an upper bound
  delay(rand()%5000);
  printf("oxygen produced\n");
  fflush(stdout);

  // post (call up) on oxygen semaphore to signal that an oxygen atom
  // has been produced, then wait for it to be used in a molecule.
  sem_post(oxy_sem);
  sem_wait(oxy_used_sem);

  printf("oxygen exit\n");
  fflush(stdout);
  sem_post(wait_to_leave_sem);
  return (void*) 0;
}

void* sulfur(void* args) {
  // produce a sulfur atom, takes a random amount of work with an upper bound
  delay(rand()%5000);
  printf("sulfur produced\n");
  fflush(stdout);

  // sulfur waits (calls down) twice on the hydrogen semaphore
  // and four times on the oxygen semaphore
  // meaning it cannot continue until at least 2 hydrogen atoms
  // have been produced
  int err = sem_wait(hydro_sem);
  int err2 = sem_wait(hydro_sem);
  int err3 = sem_wait(oxy_sem);
  int err4 = sem_wait(oxy_sem);
  int err5 = sem_wait(oxy_sem);
  int err6 = sem_wait(oxy_sem);
  if (err==-1 || err2==-1)
    printf("error on sulfur wait for hydro_sem, error # %d\n", errno);
  if  (err3==-1 || err4==-1 || err5==-1 || err6==-1)
    printf("error on sulfur wait for oxy_sem, error # %d\n", errno);

  sem_wait(sulfur_lock_sem);

  // if here, know 2 hydrogen atoms have been made already so produce a water molecule
  printf("made H2SO4\n");
  fflush(stdout);

  sem_post(hydro_used_sem);
  sem_post(hydro_used_sem);

  sem_wait(wait_to_leave_sem);
  sem_wait(wait_to_leave_sem);


  printf("sulfur exit\n");
  fflush(stdout);

  sem_post(oxy_used_sem);
  sem_post(oxy_used_sem);
  sem_post(oxy_used_sem);
  sem_post(oxy_used_sem);

  sem_wait(wait_to_leave_sem);
  sem_wait(wait_to_leave_sem);
  sem_wait(wait_to_leave_sem);
  sem_wait(wait_to_leave_sem);

  sem_post(sulfur_lock_sem);
  return (void*) 0;

}

void openSems() {
  // create the semaphores, very important to use last 3 arguments as shown here
  // first argument is simply filename for semaphore, any name is fine but must be a valid path
  hydro_sem = sem_open("hydrosmphr", O_CREAT|O_EXCL, 0466, 0);
  oxy_sem = sem_open("oxysmphr", O_CREAT|O_EXCL, 0466, 0);
  hydro_used_sem = sem_open("hydroused", O_CREAT|O_EXCL, 0466, 0);
  oxy_used_sem = sem_open("oxyused", O_CREAT|O_EXCL, 0466, 0);
  wait_to_leave_sem = sem_open("leave", O_CREAT|O_EXCL, 0466, 0);
  sulfur_lock_sem = sem_open("sulflock", O_CREAT|O_EXCL, 0466, 0);


  check_sem(&hydro_sem, "hydrosmphr");
  check_sem(&oxy_sem, "oxysmphr");
  check_sem(&hydro_used_sem, "hydroused");
  check_sem(&oxy_used_sem, "oxyused");
  check_sem(&wait_to_leave_sem, "leave");
  check_sem(&sulfur_lock_sem, "sulflock");

  sem_post(sulfur_lock_sem);
}
void closeSems() {
  // important to BOTH close the semaphore object AND unlink the semaphore file
  // for each semaphore
  sem_close(hydro_sem);
  sem_unlink("hydrosmphr");
  sem_close(oxy_sem);
  sem_unlink("oxysmphr");
  sem_close(hydro_used_sem);
  sem_unlink("hydroused");
  sem_close(oxy_used_sem);
  sem_unlink("oxyused");
  sem_close(wait_to_leave_sem);
  sem_unlink("leave");
  sem_close(sulfur_lock_sem);
  sem_unlink("sulflock");
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
      printf("semaphore %s already exists, unlinking and reopening\n", sem_string);
      fflush(stdout);
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

/*
 * NOP function to simply use up CPU time
 * arg limit is number of times to run each loop, so runs limit^2 total loops
 */
void delay( int limit )
{
  int j, k;

  for( j=0; j < limit; j++ )
    {
      for( k=0; k < limit; k++ )
        {
        }
    }
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
