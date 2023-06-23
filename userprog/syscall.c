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

static void sc_exit(struct intr_frame *);
static int sc_fork(struct intr_frame *);
static void sc_exec(struct intr_frame *);
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

#ifdef VM
static void *sc_mmap(struct intr_frame *f);
static void sc_munmap(struct intr_frame *f);
#endif

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
		sc_exec(f);
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
	#ifdef VM
	case SYS_MMAP:
		f->R.rax = sc_mmap(f);
		break;
	case SYS_MUNMAP:
		//printf("진입하는거지?");
		sc_munmap(f);
		break;
	#endif
	default:
		//printf("default에 들어옴\n");
		break;
	}
	//printf("여기에 오는건 맞아?\n");
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
	printf("fid : %d",fid);
	if(fid != TID_ERROR){
		struct thread *f_child = thread_child_find(fid);
		sema_down(&f_child->fork_sema);
		if(f_child->exit_status == TID_ERROR){
			printf("자식이 이상해\n");
			fid = TID_ERROR;
		}
	}
	return fid;
}

static
void sc_exec(struct intr_frame *f){
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
		printf("open fd : %d\n" ,fd);
		if(fd == -1){
			printf("fd가 이상해\n");
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
	struct file *cur_file = get_file(fd);

	lock_acquire(filesys_lock_);
	int success = file_length(cur_file);
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
		real_read = (int)input_getc();
	}else if(fd == 1){
		real_read = -1;
	}else{
		struct file *cur_file = get_file(fd);
		#ifdef VM
		struct page *read_page = spt_find_page(&thread_current()->spt, buffer);
		if(read_page && !read_page->writable){
			exit(-1);
		}
		#endif
		lock_acquire(filesys_lock_);
		real_read = (int)file_read(cur_file,buffer,size);
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
		lock_acquire(filesys_lock_);
		putbuf(buffer,size);
		lock_release(filesys_lock_);
		real_write = (int)size;
	}else{
		struct file *cur_file = get_file(fd);
		lock_acquire(filesys_lock_);
		real_write = (int)file_write(cur_file,buffer,size);
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
	struct file *cur_file = get_file(fd);

	lock_acquire(filesys_lock_);
	file_seek(cur_file,position);
	lock_release(filesys_lock_);
}

static
unsigned sc_tell(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;

	fd_check(fd);
	struct file *cur_file = get_file(fd);

	lock_acquire(filesys_lock_);
	unsigned success = (unsigned)file_tell(cur_file);
	lock_release(filesys_lock_);

	return success;
}

static
void sc_close(struct intr_frame *f, struct lock* filesys_lock_){
	int fd = f->R.rdi;
	
	fd_check(fd);
	struct file *cur_file = get_file(fd);
	printf("close fd : %d\n",fd);
	lock_acquire(filesys_lock_);
	cur_file = NULL;
	file_close(cur_file);
	lock_release(filesys_lock_);
}
#ifdef VM

static
void *sc_mmap(struct intr_frame *f){
	void *addr = (void *)f->R.rdi;
	size_t length = (size_t)f->R.rsi;
	int writable = f->R.rdx;
	int fd = f->R.r10;
	off_t offset = (off_t)f->R.r8;
	void *succ = NULL;

	struct file *cur_file = get_file(fd);
	/* 실패 할때 NULL 반환 */

	// fd로 열린 파일의 길이가 0 바이트 면 호출 실패 or length 이 0 일때도 실패
	if(file_length(cur_file) <= 0  || (int)length <= 0){
		return succ;
	}

	//addr 이 NULL 인 경우 실패 or addr이 페이지 정렬 안되면 실패  
	if(addr == NULL || ((uint64_t)addr % PGSIZE)){
		return succ;
	}

	/* 매핑된 페이지 범위가 실행 가능한 로드시간에 매핑된 스택 또는 
	페이지 포함하여 기존 매핑된 페이지 집합과 겹치는 경우 실패 ?? */
	if(!is_user_vaddr(addr) || !is_user_vaddr(addr + length) || spt_find_page(&thread_current()->spt, addr)){
		return succ;
	}


	//콘솔 입력 및 출력 파일 설명자는 매핑 할수 없다
	if(fd == 0 || fd == 1){
		return succ;
	}

	if(offset > length){
		return succ;
	}

	return do_mmap(addr,length,writable,cur_file,offset);

}

static
void sc_munmap(struct intr_frame *f){
	void *addr = (void *)f->R.rdi;

	ptr_check(addr);

	// addr이 페이지 정렬 안되면 실패
	if((uint64_t)addr % PGSIZE != 0){
		exit(-1);
	}

	do_munmap(addr);
}

#endif
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
    if (ptr == NULL|| !is_user_vaddr((uint64_t)ptr))
    {
        exit(-1);
    }
}

static 
void fd_check(int fd)
{
    if (fd < 0 || fd >= 128)
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