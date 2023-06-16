#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
/*-----syscall-------------------*/
void exit(int status);
struct thread* thread_child_find(tid_t child_tid);
/*-------------------------------*/
void process_exit (void);
void process_activate (struct thread *next);

/*------argument passing----------------*/
void argument_passing(char ** argv,int argc ,struct intr_frame *_if_);
void argument_stack(char ** argv,int argc ,struct intr_frame *if_);
/*--------------------------------------*/
#endif /* userprog/process.h */
