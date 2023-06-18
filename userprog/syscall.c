#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "include/lib/kernel/stdio.h"
#include "include/filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "include/lib/string.h"
#include "threads/palloc.h"
#include "vm/vm.h"

static void check_address(void *);

static void sc_exit(struct intr_frame *);
static int sc_fork(struct intr_frame *);
static int sc_exec(struct intr_frame *);
static int sc_wait(struct intr_frame *);
static bool sc_create(struct intr_frame *,struct lock*);
static bool sc_remove(struct intr_frame *,struct lock*);
static int sc_open(struct intr_frame *,struct lock*);
static int sc_filesize(struct intr_frame *,struct lock*);
static int sc_read(struct intr_frame *,struct lock*);
static int sc_write(struct intr_frame *,struct lock*);
static void sc_seek(struct intr_frame *,struct lock*);
static unsigned sc_tell(struct intr_frame *,struct lock*);
static void sc_close(struct intr_frame *,struct lock*);
static int sc_dup2(struct intr_frame *, struct lock*);

static struct file *get_file(int);
static void ptr_check (void *ptr);
static void fd_check(int fd);
static int process_add_file(struct file *f);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */
/* 시스템 콜.
이전에는 시스템 콜 서비스가 인터럽트 핸들러에 의해 처리되었습니다
(예: 리눅스에서의 int 0x80). 그러나 x86-64에서는 제조업체가
시스템 콜을 요청하기 위한 효율적인 경로를 제공합니다. 바로 syscall 명령입니다.
syscall 명령은 Model Specific Register (MSR)에서 값을 읽어옴으로써 작동합니다.
자세한 내용은 매뉴얼을 참조하십시오. */


#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. 
	 * "시스템 콜 진입점이 사용자 공간 스택을 커널 모드 스택으로 교체할 때까지 
	 * 인터럽트 서비스 루틴은 어떠한 인터럽트도 처리해서는 안됩니다. 
	 * 따라서 우리는 FLAG_FL을 마스크하였습니다."*/
	
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}



/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	//printf("syscall_handler 진입\n");
	// TODO: Your implementation goes here.
	// TODO: 여기에 구현 작업을 수행합니다.
	//check_address(f->R.rax);

	#ifdef VM
	thread_current()->rsp = f->rsp;
	#endif

	switch (f->R.rax)
	{
	case SYS_HALT:
		power_off();
		break;
	case SYS_EXIT:
		sc_exit(f);
		break;
	case SYS_FORK:
		f->R.rax = sc_fork(f);
		break;
	case SYS_EXEC:
		f->R.rax = sc_exec(f);
		break;
	case SYS_WAIT:
		f->R.rax = sc_wait(f);
		break;
	case SYS_CREATE:
		f->R.rax = sc_create(f,&filesys_lock);
		break;
	case SYS_REMOVE:
		f->R.rax = sc_remove(f,&filesys_lock);
		break;
	case SYS_OPEN:
		f->R.rax = sc_open(f,&filesys_lock);
		break;
	case SYS_FILESIZE:
		f->R.rax = sc_filesize(f,&filesys_lock);
		break;
	case SYS_READ:
		f->R.rax = sc_read(f,&filesys_lock);
		break;
	case SYS_WRITE:
		f->R.rax = sc_write(f,&filesys_lock);
		break;
	case SYS_SEEK:
		sc_seek(f,&filesys_lock);
		break;
	case SYS_TELL:
		f->R.rax = sc_tell(f,&filesys_lock);
		break;
	case SYS_CLOSE:
		sc_close(f,&filesys_lock);
		break;
	case SYS_DUP2:
		//f->R.rax = sc_dup2(f,&filesys_lock);
		break;
	default:
		//printf("default에 들어옴\n");
		break;
	}
	//printf("여기에 오는건 맞아?\n");
}

static
void check_address(void *addr){
	struct thread *cur = thread_current();
	if(addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL){ // 유저 스택 밖에 있다면 종료
		exit(-1);
	}
}

static
void sc_exit(struct intr_frame *f){
	int status = f->R.rdi;
	exit(status);
}

static
int sc_fork(struct intr_frame *f){
	char * thread_fork_name = (char *)f->R.rdi;
	
	ptr_check(thread_fork_name);

	tid_t fid = process_fork(thread_fork_name,f);
	if(fid != TID_ERROR){
		struct thread *f_child = thread_child_find(fid);
		sema_down(&f_child->fork_sema);
		if(f_child->exit_status == TID_ERROR){
			fid = TID_ERROR;
		}
	}
	return fid;
}

static
int sc_exec(struct intr_frame *f){
	char * file = (char *)f->R.rdi;
	int success = 0;
	ptr_check(file);

	char * cmd_line = palloc_get_page (0);
	if (cmd_line == NULL){
		exit(-1);
	}
	strlcpy (cmd_line, file, PGSIZE);
	if(success = process_exec(cmd_line) < 0){
		exit(-1);
	}
	return success;
}

static
int sc_wait(struct intr_frame *f){
	tid_t pid = (tid_t)f->R.rdi;
	return process_wait(pid);
}

static
bool sc_create(struct intr_frame *f, struct lock* filesys_lock_){
	char *file_create_name = (char *)f->R.rdi;
	off_t initial_size = (off_t)f->R.rsi;

	ptr_check(file_create_name);

	lock_acquire(filesys_lock_);
	bool success = filesys_create(file_create_name,initial_size);
	lock_release(filesys_lock_);

	return success; 
}

static
bool sc_remove(struct intr_frame *f, struct lock* filesys_lock_){
	char *file_remove_name = (char *)f->R.rdi;

	ptr_check(file_remove_name);

	lock_acquire(filesys_lock_);
	bool success = filesys_remove(file_remove_name);
	lock_release(filesys_lock_);

	return success;
}

static
int sc_open(struct intr_frame *f, struct lock* filesys_lock_){
	char *file_open_name = (char *)f->R.rdi;
	struct thread *curr = thread_current();
	int fd = curr->next_fd;

	ptr_check(file_open_name);

	lock_acquire(filesys_lock_);
	struct file *open_file = filesys_open(file_open_name);

	if(open_file == NULL){
		fd = -1;
	}else{
		fd = process_add_file(open_file);
		if(fd == -1){
			file_close(open_file);
		}
	}
	lock_release(filesys_lock_);

	return fd;
}

static
int sc_filesize(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;

	fd_check(fd);

	lock_acquire(filesys_lock_);
	int success = file_length(get_file(fd));
	lock_release(filesys_lock_);

	return success;
}

static
int sc_read(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;
	void *buffer = (void *)f->R.rsi;
	unsigned size = (unsigned)f->R.rdx;
	int real_read;

	fd_check(fd);
	ptr_check(buffer);

	
	if(fd == 0){
		lock_acquire(filesys_lock_);
		real_read = (int)input_getc();
		lock_release(filesys_lock_);
	}else if(fd == 1){
		real_read = -1;
	}else{// 수정이 필요함
		lock_acquire(filesys_lock_);
		real_read = (int)file_read(get_file(fd),buffer,size);
		lock_release(filesys_lock_);
	}


	return real_read;
}

static
int sc_write(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;
	void *buffer = (void *)f->R.rsi;
	unsigned size = (unsigned)f->R.rdx;
	int real_write;
	
	fd_check(fd);
	ptr_check(buffer);

	if(fd == 0){
		real_write = -1;
	}else if(fd == 1){
		putbuf(buffer,size);
		real_write = (int)size;
	}else{
		lock_acquire(filesys_lock_);
		real_write = (int)file_write(get_file(fd),buffer,size);
		lock_release(filesys_lock_);
	}
	//printf("name : %s, fd : %d , real_write : %d\n",thread_current()->name,fd,real_write);
	return real_write;
}

static
void sc_seek(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;
	unsigned position = (unsigned)f->R.rsi;

	fd_check(fd);

	lock_acquire(filesys_lock_);
	file_seek(get_file(fd),position);
	lock_release(filesys_lock_);
}

static
unsigned sc_tell(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;

	fd_check(fd);

	lock_acquire(filesys_lock_);
	unsigned success = (unsigned)file_tell(get_file(fd));
	lock_release(filesys_lock_);

	return success;
}

static
void sc_close(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;
	
	fd_check(fd);
	
	lock_acquire(filesys_lock_);
	struct file *curr_file = get_file(fd);
	
	curr_file = NULL;
	file_close(curr_file);
	lock_release(filesys_lock_);
}

static
int sc_dup2(struct intr_frame *f, struct lock* filesys_lock_){
	int oldfd = f->R.rdi;
	int newfd = f->R.rsi;
	struct thread *curr = thread_current();

	fd_check(oldfd);
	fd_check(newfd);
	
	lock_acquire(filesys_lock_);

	if(*(curr->fdt+ oldfd) == *(curr->fdt+ newfd)){
		return newfd;
	}else if(*(curr->fdt+ oldfd) == NULL){
		return -1;
	}

	newfd = file_duplicate(oldfd);
	
	lock_release(filesys_lock_);
	return newfd;
}

/* 현재 존재하는 파일을 가져올때 파일이 없다면 exit(-1) 함*/
static
struct file *get_file(int fd){
	struct thread *curr = thread_current();

	if(*(curr->fdt + fd) == NULL){
		exit(-1);
	}
	
	return *(curr->fdt + fd);
}

static
void ptr_check(void *ptr)
{
    if (ptr == NULL)
    {
        exit(-1);
    }
}

static 
void fd_check(int fd)
{
    if (fd < 0 || fd >= 64)
    {
		exit(-1);
    }
}

/*
 * 새로운 파일 객체제 대한 파일 디스크립터 생성하는 함수
 * fdt에도 추가해준다.
 */
static
int process_add_file(struct file *f)
{
	struct thread *cur = thread_current();
	struct file **fdt = cur->fdt;

	// 범위를 벗어나지 않고 인덱스에 값이 존재하지 않을 때까지
	while (cur->next_fd < 128 && fdt[cur->next_fd])
	{
		cur->next_fd++;
	}

	if (cur->next_fd >= 128)
	{ // 범위를 넘어설 때까지 남은 공간이 없으면
		return -1;
	}
	fdt[cur->next_fd] = f;

	return cur->next_fd;
}