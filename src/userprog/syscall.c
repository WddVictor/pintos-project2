#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#define STDIN 0
#define STDOUT 1
#define SYS_CALL_NUM 20

typedef int pid_t;

static void syscall_handler (struct intr_frame *);
static void (*syscall_handlers[SYS_CALL_NUM])(struct intr_frame *); // array of all system calls
static struct list file_list;
struct fd_entry{
  int fd;
  struct file *file;
  struct list_elem elem;
  struct list_elem thread_elem;
};

void sys_halt(struct intr_frame* f); /* Halt the operating system. */
void sys_exit(struct intr_frame* f); /* Terminate this process. */
void sys_exec(struct intr_frame* f); /* Start another process. */
void sys_wait(struct intr_frame* f); /* Wait for a child process to die. */
void sys_create(struct intr_frame* f); /* Create a file. */
void sys_remove(struct intr_frame* f);/* Create a file. */
void sys_open(struct intr_frame* f); /*Open a file. */
void sys_filesize(struct intr_frame* f);/* Obtain a file's size. */
void sys_read(struct intr_frame* f);  /* Read from a file. */
void sys_write(struct intr_frame* f); /* Write to a file. */
void sys_seek(struct intr_frame* f); /* Change position in a file. */
void sys_tell(struct intr_frame* f); /* Report current position in a file. */
void sys_close(struct intr_frame* f); /* Close a file. */

static int get_user (const uint8_t *uaddr);
static bool is_valid_p(void * esp, uint8_t argc);
static bool is_valid_string(void * str);
static int alloc_fid (void);
struct fd_entry *find_fd_by_fd(int fd);
void close(int fd);
void exit(int status);
int write (int fd, const void *buffer, unsigned length);
int open(const char *file);
struct file *find_file_by_fd(int fd);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  syscall_handlers[SYS_HALT] = &sys_halt;
  syscall_handlers[SYS_EXIT] = &sys_exit;
  syscall_handlers[SYS_WAIT] = &sys_wait;
  syscall_handlers[SYS_CREATE] = &sys_create;
  syscall_handlers[SYS_REMOVE] = &sys_remove;
  syscall_handlers[SYS_OPEN] = &sys_open;
  syscall_handlers[SYS_WRITE] = &sys_write;
  syscall_handlers[SYS_SEEK] = &sys_seek;
  syscall_handlers[SYS_TELL] = &sys_tell;
  syscall_handlers[SYS_CLOSE] =&sys_close;
  syscall_handlers[SYS_READ] = &sys_read;
  syscall_handlers[SYS_EXEC] = &sys_exec;
  syscall_handlers[SYS_FILESIZE] = &sys_filesize;


  list_init (&file_list);
}

static int
get_user (const uint8_t *uaddr)//done
{
  if(!is_user_vaddr(uaddr))
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}


static bool is_valid_p(void * esp, uint8_t argc){//done
  uint8_t i = 0;
  for (; i < argc; ++i)
  {
    if (get_user(((uint8_t *)esp)+i) == -1){
      return false;
    }
  }
  return true;
}

static bool is_valid_string(void * str)//done
{
  int ch=-1;
  while((ch=get_user((uint8_t*)str++))!='\0' && ch!=-1);
    if(ch=='\0')
      return true;
    else
      return false;
}

static int
alloc_fid (void)
{
  static int fid = 2;
  return fid++;
}

struct fd_entry *find_fd_by_fd(int fd){//done
  struct fd_entry *ret;
  struct list_elem *l;
  struct thread *t;

  t = thread_current();

  for(l = list_begin(&t->fd_list); l != list_end(&t->fd_list); l = list_next(l)){
    ret = list_entry(l, struct fd_entry, thread_elem);
    if (ret->fd == fd){
      return ret;
    }
  }

  return NULL;
}

void close(int fd)//done
{
  struct fd_entry *f = find_fd_by_fd(fd);

  if (f == NULL){
    exit(-1);
  }
  file_close(f->file);
  list_remove(&f->elem);
  list_remove(&f->thread_elem);
  free(f);
}


void exit(int status){//done
  struct thread *t = thread_current();
  struct list_elem *e;

  while(!list_empty(&t->fd_list)){
    e = list_begin(&t->fd_list);
    close(list_entry(e,struct fd_entry, thread_elem)->fd);
  }
  t->exit_status = status;
  thread_exit();
}



static void
syscall_handler (struct intr_frame* f) 
{
  if(!is_valid_p(f->esp,4)){
    exit(-1);
    return;
  }
  int syscall_num = * (int *)f->esp;
  if(syscall_num<=0||syscall_num>=SYS_CALL_NUM){
    exit(-1);
  }
  syscall_handlers[syscall_num](f);
}

void sys_halt(struct intr_frame* f){//done
  shutdown();
};

void sys_exit(struct intr_frame* f){//done
  if (!is_valid_p(f->esp + 4,4)){
    exit(-1);
  }int status = *(int *)(f->esp + 4);
  exit(status);
};


int write (int fd, const void *buffer, unsigned length){
  if(fd==STDOUT){ // stdout
      putbuf((char *) buffer,(size_t)length);
      return (int)length;
  }else{
    struct file *f = find_file_by_fd(fd);
    if(f==NULL){
      exit(-1);
    }
    return (int) file_write(f,buffer,length);

  }
}

void sys_write(struct intr_frame* f){//not done
  if (!is_valid_p(f->esp + 4, 12)){
    exit(-1);
  }
  int fd = *(int *)(f->esp + 4);
  void *buffer = *(char **)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_p(buffer, 1) || !is_valid_p(buffer + size, 1)){
    exit(-1);
  }

  // write to the file, acquire file lock
  f->eax = write(fd, buffer, size);
  return;
};

void sys_wait(struct intr_frame* f){
  pid_t pid;
  if(!is_valid_p(f->esp+4,4)){
    exit(-1);
  }
  pid = *((int*)f->esp+1);
  f->eax = process_wait(pid);
};


void sys_exec(struct intr_frame* f){//done
  char *file_name = *(char **)(f->esp + 4);
  f->eax = process_execute(file_name);
};

void sys_create(struct intr_frame* f){//done
  char *file_name = *(char **)(f->esp + 4);
  if (!is_valid_string(file_name)){
    exit(-1);
  }
  unsigned size = *(int *)(f->esp + 8);
  filesys_create(file_name,size);
};

int open(const char *file){ // done filesys_open...done
  struct file* f = filesys_open(file);
  if (f == NULL){
    return -1;
  }

  struct fd_entry *e = (struct fd_entry *)malloc(sizeof(struct fd_entry));
  if (e == NULL){
    file_close(f);
    return -1;
  }

  struct thread *cur = thread_current();
  e->fd = alloc_fid();
  e->file = f;
  list_push_back(&cur->fd_list, &e->thread_elem);
  list_push_back(&file_list, &e->elem);

  return e->fd;
}
void sys_open(struct intr_frame* f){//done
  char *file_name = *(char **)(f->esp + 4);
  if (!is_valid_string(file_name)){
    exit(-1);
  }

  f->eax = open(file_name);
};

void sys_close(struct intr_frame* f){//done
  int fd = *(int *)(f->esp + 4);
  close(fd);
};

void sys_remove(struct intr_frame* f){//done filesys_remove...done
  char *file_name = *(char **)(f->esp + 4);
  f->eax = filesys_remove(file_name);
};

struct file *find_file_by_fd(int fd){//done
  struct fd_entry *ret;
  ret = find_fd_by_fd(fd);
  if (!ret){
    return NULL;
  }
  return ret->file;
};

int fs(int fd){ // done find_file_by_fd...done
  struct file *f = find_file_by_fd(fd);
  if (f == NULL){
    exit(-1);
  }
  return file_length(f);
};


void sys_filesize(struct intr_frame* f){//done
  int fd = *(int *)(f->esp + 4);
  f->eax = fs(fd);
};

int read (int fd, void *buffer, unsigned length){//done
  // printf("call read %d\n", fd);
  if(fd==STDIN){
    for(unsigned int i=0;i<length;i++){
      *((char **)buffer)[i] = input_getc();
    }
    return length;
  }else{
    struct file *f = find_file_by_fd(fd);

    if(f == NULL){
      return -1;
    }
    return file_read(f,buffer,length);
  }
}
void sys_read(struct intr_frame* f){//done
  int fd = *(int *)(f->esp + 4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_p(buffer, 1) || !is_valid_p(buffer + size,1)){
    exit(-1);
  }
  f->eax = read(fd,buffer,size);
};

unsigned tell(int fd){//done file tell..done
    struct file *f = find_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  return file_tell(f);
}

void sys_tell(struct intr_frame* f){//done
  int fd = *(int *)(f->esp + 4);
  f->eax = tell(fd);
};


void seek(int fd, unsigned position){//file_seek... done
  struct file *f = find_file_by_fd(fd);
  if(f == NULL){
    exit(-1);
  }
  file_seek(f,position);
}
void sys_seek(struct intr_frame* f){//done
    int fd = *(int *)(f->esp + 4);
  unsigned pos = *(unsigned *)(f->esp + 8);
  seek(fd,pos);
};
