#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#define MAX_ARGC 100

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/**
file descriptor
*/
struct file_descriptor{
  int fd;
  struct list_elem elem; //list entry of a the fd
  struct file* file; //opend files
};

struct process{
  struct list_elem elem;
  tid_t thread_id; 
};

struct read{
  tid_t pid;
  char state;
  struct list_elem elem;
  int value;
};

struct wait{
  tid_t pid;
  char state;
  struct list_elem elem;
  struct semaphore sema;
};

#endif /* userprog/process.h */
