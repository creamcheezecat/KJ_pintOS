#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

#include "threads/synch.h"
#include "include/userprog/syscall.h"
#include "include/lib/kernel/hash.h"
static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void*);

/* General process initializer for initd and other process. */
/* initd 및 기타 프로세스를 위한 일반적인 프로세스 초기화 기 */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
/* "initd"라는 이름의 첫 번째 유저랜드 프로그램을 FILE_NAME에서 로드하여 실행합니다.
새로운 스레드는 process_create_initd()가 반환되기 전에 스케줄될 수 있으며
심지어 종료될 수도 있습니다. initd의 스레드 ID를 반환하며,
스레드를 생성할 수 없는 경우에는 TID_ERROR를 반환합니다.
한 번만 호출되어야 함에 유의하세요. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;
	//printf("여기가 먼저인가?\n");
	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	/* FILE_NAME의 사본을 만듭니다.
	그렇지 않으면 호출자와 load() 함수 사이에 경합이 발생할 수 있습니다. */

	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	char *save_ptr;
	file_name = strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	/* FILE_NAME을 실행하기 위해 새로운 스레드를 생성합니다. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);

	if (tid == TID_ERROR){
		palloc_free_page (fn_copy);
	}

	return tid;
}

/* A thread function that launches first user process. */
/* 첫 번째 사용자 프로세스를 시작하는 스레드 함수입니다. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif
	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/* 현재 프로세스를 name으로 복제합니다. 새로운 프로세스의 스레드 ID를 반환하며,
스레드를 생성할 수 없는 경우에는 TID_ERROR를 반환합니다. */
tid_t
process_fork (const char *name, struct intr_frame *if_) {
	/* Clone current thread to new thread.*/
	// do_fork에 전달할때 thread_current()안에 if_ 를 저장해서 같이 전달한다.
	memcpy(&thread_current()->fork_if, if_,sizeof(struct intr_frame));
	tid_t f_tid = thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
	return f_tid;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
/* 이 함수를 pml4_for_each에 전달하여 부모의 주소 공간을 복제합니다. 
이는 프로젝트 2에서만 사용됩니다. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	/* 1. TODO: 만약 parent_page가 커널 페이지인 경우, 즉시 반환합니다. */
	if(is_kern_pte(pte)){
		return true;
	}
	/* 2. Resolve VA from the parent's page map level 4. */
	/* 2. 부모의 페이지 맵 레벨 4에서 VA를 해결합니다. */
	parent_page = pml4_get_page (parent->pml4, va);
	if (parent_page == NULL){
        return false;
	}
	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	/* 3. TODO: 자식을 위해 새로운 PAL_USER 페이지를 할당하고 결과를 NEWPAGE에 설정합니다. */
	newpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (newpage == NULL){
        return false;
	}
	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	/* 4. TODO: 부모의 페이지를 새로운 페이지로 복제하고,
		TODO: 부모의 페이지가 쓰기 가능한지 여부를 확인합니다 (결과에 따라 WRITABLE을 설정합니다). */
	memcpy(newpage,parent_page,PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	/* 5. VA 주소에 WRITABLE 권한을 가진 새로운 페이지를 자식의 페이지 테이블에 추가합니다. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		/* 6. TODO: 페이지 삽입에 실패한 경우, 오류 처리를 수행합니다. */
		palloc_free_page(newpage);
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. 
 * 부모의 실행 컨텍스트를 복사하는 스레드 함수입니다.
 * 힌트) parent->tf는 프로세스의 유저 모드 컨텍스트를 보유하지 않습니다.
 * 즉, process_fork의 두 번째 인자를 이 함수에 전달해야 합니다. 
 */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	/* TODO: 어떤 방식으로든 parent_if를 전달하세요. (예: process_fork()의 if_ 인자) */
	struct intr_frame *parent_if = &parent->fork_if;/* = (struct intr_frame *)aux */

	/* 1. Read the cpu context to local stack. */
	/* 1. CPU 컨텍스트를 로컬 스택으로 읽어옵니다. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));
	/* fork() 함수의 호출 결과를 자식 프로세스에게 알려주기 위해
	 이를 통해 자식 프로세스는 자신이 자식 프로세스임을 
	 확인하고 적절한 동작을 수행할 수 있습니다.*/
	if_.R.rax = 0;

	/* 2. Duplicate PT */
	/* 2. 페이지 테이블을 복제합니다. */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
	/* TODO: 여기에 코드를 작성하세요.
	TODO: 힌트) 파일 객체를 복제하려면 include/filesys/file.h에 있는 file_duplicate를 사용하세요.
	TODO: 부모는 fork()가 성공적으로 부모의 리소스를 복제할 때까지 반환해서는 안 됩니다. */
	process_init ();

	for (int i = 2; i < 128; ++i)
    {
        struct file *fp = *(parent->fdt + i);
		//file_allow_write (fp);
		if(fp != NULL){
			*(current->fdt + i) = file_duplicate(fp);
		}else{
			*(current->fdt + i) = NULL;
		}
    }
	current->next_fd = parent->next_fd;

	sema_up(&current->fork_sema);

	/* Finally, switch to the newly created process. */
	do_iret (&if_);
error:
	sema_up(&current->fork_sema);
	exit(-1);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	/* 
	우리는 스레드 구조체의 intr_frame을 사용할 수 없습니다.
	이는 현재 스레드가 재스케줄되었을 때,
	실행 정보를 해당 멤버에 저장하기 때문입니다. 
	*/
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup ();

	// hash_destroy() 호출하면 아예없어서 init을 새로 해줘야했다.
	// supplemental_page_table_init(&thread_current()->spt);

	lock_acquire(&filesys_lock);
	/* And then load the binary */
	success = load (file_name, &_if);
	lock_release(&filesys_lock);
	/* If load failed, quit. */
	palloc_free_page (file_name);
	if (!success){
		return -1;	
	}
	//hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true);

	/* Start switched process. */
	do_iret (&_if);

	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
/* 
스레드 TID가 종료될 때까지 대기하고 종료 상태를 반환합니다.
커널에 의해 종료되었을 경우 (즉, 예외로 인해 종료되었을 경우), -1을 반환합니다.
TID가 유효하지 않거나 호출하는 프로세스의 자식이 아니거나
주어진 TID에 대해 이미 process_wait()가 성공적으로 호출되었을 경우,
대기하지 않고 즉시 -1을 반환합니다.
이 함수는 2-2 문제에서 구현될 것입니다. 현재는 아무것도 수행하지 않습니다. 
*/
int
process_wait (tid_t child_tid) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */
	struct thread *t_child = NULL;
	
	t_child = thread_child_find(child_tid);
	// 없다면 -1을 반환한다.
	if(t_child == NULL){
		return -1;
	}
	//있다면 자식 리스트에서 그 자식을 빼내고 자신이 자식의 waitlist에 들어간다. 
	list_remove(&t_child->child_elem);

	sema_down(&t_child->wait_sema);
	//list_remove(&t_child->child_elem);
	sema_up(&t_child->clear_sema);

	return t_child->exit_status;
}


// 인자 값으로 받은 tid 를 가지고 현재 내 자식 리스트에서 비교해서 찾는다.
struct thread*
thread_child_find(tid_t child_tid){
	struct thread *curr = thread_current ();
	struct list_elem *e = NULL;
	struct thread *t_child = NULL;
	
	for (e = list_begin(&curr->children); e != list_end(&curr->children); e = list_next(e)){	
		t_child = list_entry(e,struct thread,child_elem);
		if(t_child->tid == child_tid){
			return t_child;
		}
	}

	return NULL;
}

/*------------syscall-----------------------------------------*/

void 
exit(int status){
	struct thread *curr = thread_current ();
	curr->exit_status = status;
	// printf("name : %s status : %d\n",curr->name,status);
	printf ("%s: exit(%d)\n",curr->name ,curr->exit_status);
	thread_exit();
}

/*-----------------------------------------------------------*/

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	/* TODO: 여기에 코드를 작성하세요.
	* TODO: 프로세스 종료 메시지를 구현하세요 (project2/process_termination.html 참조).
	* TODO: 여기서 프로세스 리소스 정리를 구현하는 것을 권장합니다. */
	
	for (int i = 2; i < 128; ++i)
    {
        file_close(*(curr->fdt + i));
        *(curr->fdt + i) = NULL; // null 처리 해줘야 한다.
    }

    palloc_free_page(curr->fdt);
	
	file_close(curr->running_file);
	curr->running_file = NULL;

	process_cleanup ();

	/* lock_acquire(&curr->spt.page_lock);
	hash_destroy(&curr->spt.pages, NULL);
	lock_release(&curr->spt.page_lock); */

	sema_up(&curr->wait_sema);
	

	/* struct list_elem *e = NULL;
	struct thread *c_child = NULL;

	for (e = list_begin(&curr->children); e != list_end(&curr->children); e = list_next(e)){	
		c_child = list_entry(e, struct thread, child_elem);
		sema_up(&c_child->clear_sema);
	} */

	sema_down(&curr->clear_sema);
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	/* 현재 프로세스의 페이지 디렉토리를 파괴하고 
	커널 전용 페이지 디렉토리로 전환합니다. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		/* 여기서 올바른 순서가 중요합니다. 페이지 디렉토리를 전환하기 전에 cur->pagedir을 NULL로 설정해야 합니다.
	   이렇게 하면 타이머 인터럽트가 프로세스 페이지 디렉토리로 다시 전환되지 않습니다.
	   프로세스의 페이지 디렉토리를 파괴하기 전에 기본 페이지 디렉토리를 활성화해야 합니다.
	   그렇지 않으면 우리의 활성 페이지 디렉토리는 해제되고 지워진 페이지 디렉토리가 될 것입니다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
/* nest 스레드에서 사용자 코드를 실행하기 위해 CPU를 설정합니다.
이 함수는 모든 컨텍스트 스위치마다 호출됩니다. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
/* 실행 파일 헤더입니다. [ELF1] 1-4부터 1-8을 참조하세요.
이는 ELF 이진 파일의 맨 처음에 나타납니다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
/* 
현재 스레드에 ELF 실행 파일을 FILE_NAME에서 로드합니다.
실행 파일의 진입점을 *RIP에 저장하고
초기 스택 포인터를 *RSP에 저장합니다.
성공하면 true를 반환하고, 그렇지 않으면 false를 반환합니다. 
*/
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

/*----argument parsing-------------------------------------------------------*/
	
	// 인수 분할
	char *argv[64],*token, *save_ptr;
	int argc = 0;

	for(token = strtok_r(file_name," ", &save_ptr); 
		token != NULL;
		token = strtok_r(NULL, " ", &save_ptr)){
			argv[argc++] = token;
	}

	file_name = argv[0];
/*----------------------------------------------------------------------------*/

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* 실행 중인 프로세스의 실행 파일이 수정되지 않도록 보장하기위해 */
	thread_current()->running_file = file;
	file_deny_write(file);

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	
	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}
	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;
	//printf("rspp : %x\n", if_->rsp);
	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */
	/* TODO: 코드가 여기에 옵니다.
	 * TODO: 인수 전달을 구현합니다(project2/argument_passing.html 참조). */
	argument_stack(argv,argc,if_);



	success = true;
done:
	/* We arrive here whether the load is successful or not. */
	//file_close (file);
	return success;
}

/*---------argument_passing------------------------------------------------------*/

void
argument_stack(char ** argv,int argc ,struct intr_frame *if_){
	char *argv_addr[64];
	// 거꾸로 삽입 => 스택은 반대 방향으로 확장하기 떄문!
	
	/* 맨 끝 NULL 값(arg[4]) 제외하고 스택에 저장(arg[0] ~ arg[3]) */
	for (int i = argc-1; i>=0; i--) { 
		int argv_len = strlen(argv[i]);
		/* 
		if_->rsp: 현재 user stack에서 현재 위치를 가리키는 스택 포인터.
		각 인자에서 인자 크기(argv_len)를 읽고 (이때 각 인자에 sentinel이 포함되어 있으니 +1 - strlen에서는 sentinel 빼고 읽음)
		그 크기만큼 rsp를 내려준다. 그 다음 빈 공간만큼 memcpy를 해준다.
		*/
		if_->rsp = if_->rsp - (argv_len + 1);
		memcpy(if_->rsp, argv[i], argv_len+1);
		argv_addr[i] = if_->rsp; // argv_addr 배열에 현재 문자열 시작 주소 위치를 저장한다.
	}

	/* word-align: 8의 배수 맞추기 위해 padding 삽입*/
	while (if_->rsp % 8 != 0) 
	{
		if_->rsp--; // 주소값을 1 내리고
		*(uint8_t *) if_->rsp = 0; //데이터에 0 삽입 => 8바이트 저장
	}

	/* 이제는 주소값 자체를 삽입! 이때 센티넬 포함해서 넣기*/
	
	for (int i = argc; i >=0; i--) 
	{ // 여기서는 NULL 값 포인터도 같이 넣는다.
		if_->rsp = if_->rsp - 8; // 8바이트만큼 내리고
		if (i == argc) { // 가장 위에는 NULL이 아닌 0을 넣어야지
			memset( if_->rsp, 0, sizeof(char **));
		} else { // 나머지에는 argv_addr 안에 들어있는 값 가져오기
			memcpy( if_->rsp, &argv_addr[i], sizeof(char **)); // char 포인터 크기: 8바이트
		}	
	}
	

	/* fake return address */
	if_->rsp = if_->rsp - 8; // void 포인터도 8바이트 크기
	memset(if_->rsp, 0, sizeof(void *));

	if_->R.rdi  = argc;
	if_->R.rsi = if_->rsp + 8; // fake_address 바로 위: arg_address 맨 앞 가리키는 주소값!
}

/*----------------------------------------------------------------------------*/

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */
/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
/* 주소 UPAGE에서 시작하는 파일의 OFS 오프셋에서 세그먼트를 로드합니다.
총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 초기화됩니다. 다음과 같습니다:

UPAGE에서 READ_BYTES 바이트는 OFS 오프셋에서 시작하여 FILE에서 읽어야 합니다.

UPAGE + READ_BYTES에서 ZERO_BYTES 바이트는 0으로 설정되어야 합니다.

이 함수에 의해 초기화된 페이지는 WRITABLE이 
true인 경우 사용자 프로세스에 의해 쓰기 가능해야 하며, 그렇지 않으면 읽기 전용이어야 합니다.

성공하면 true를 반환하고, 
메모리 할당 오류 또는 디스크 읽기 오류가 발생하면 false를 반환합니다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		// Get a page of memory.
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		// Load this page.
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		
		//Add the page to the process's address space. 
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}
		
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
/* USER_STACK에서 0인 페이지를 매핑하여 최소 스택 생성 */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
/* 사용자 가상 주소 UPAGE에서 커널로 매핑 추가
 * 페이지 테이블에 대한 가상 주소 KPAGE.
 * WRITABLE이 참이면 사용자 프로세스가 페이지를 수정할 수 있습니다.
 * 그렇지 않으면 읽기 전용입니다.
 * UPAGE는 이미 매핑되어 있지 않아야 합니다.
 * KPAGE는 사용자 풀에서 얻은 페이지여야 합니다.
 * palloc_get_page()와 함께.
 * 성공 시 true를 반환하고 UPAGE가 이미 매핑된 경우 false를 반환하거나
 * 메모리 할당에 실패한 경우. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */	
	/* 해당 가상 페이지에 이미 페이지가 없는지 확인합니다.
	* 주소, 그런 다음 거기에 우리 페이지를 매핑하십시오. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */
/* 여기서부터 코드는 프로젝트 3 이후에 사용됩니다.
 * 프로젝트 2만의 기능을 구현하고 싶다면,
 * 상단 블록. */
static bool
lazy_load_segment (struct page *page, void *aux) {
	ASSERT(page->frame != NULL);
    ASSERT(aux != NULL);
	
	void *kpage = page->frame->kva;

	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	/* TODO: 파일로부터 세그먼트를 로드합니다. /
	/ TODO: 이 함수는 주소 VA에 해 첫 번째 페이지 폴트가 발생할 때 호출됩니다. /
	/ TODO: 이 함수를 호출할 때 VA는 사용 가능합니다. */

	struct file_page *fp = (struct file_page *)aux;
	struct file *file = fp->file;
	off_t offset = fp->offset;
    size_t page_read_bytes = fp->read_bytes;
    size_t page_zero_bytes = fp->zero_bytes;

	

	if(file_read_at(file, kpage, page_read_bytes, offset) != (int)page_read_bytes){
		palloc_free_page(kpage);
		return false;
	}

	memset(kpage + page_read_bytes, 0, page_zero_bytes);
	free(fp);
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
/* 주소에서 FILE의 오프셋 OFS에서 시작하는 세그먼트를 로드합니다.
 * 업데이트. 총 READ_BYTES + ZERO_BYTES바이트의 가상
 * 메모리는 다음과 같이 초기화됩니다.
 *
 * - UPAGE의 READ_BYTES 바이트는 FILE에서 읽어야 합니다.
 * 오프셋 OFS에서 시작.
 *
 * - UPAGE + READ_BYTES에서 ZERO_BYTES 바이트를 0으로 설정해야 합니다.
 *
 * 이 함수에 의해 초기화된 페이지는
 * WRITABLE이 참이면 사용자 프로세스, 그렇지 않으면 읽기 전용.
 *
 * 성공하면 true 반환, 메모리 할당 오류이면 false 반환
 * 또는 디스크 읽기 오류가 발생합니다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/* TODO: 정보를 전달하기 위해 aux를 설정합니다. lazy_load_segment에게 전달됩니다. */
		struct file_page *fp = (struct file_page *)malloc(sizeof(struct file_page));

		fp->file = file;
		fp->offset = ofs;
		fp->read_bytes = page_read_bytes;
		fp->zero_bytes = page_zero_bytes;
		

		if(!vm_alloc_page_with_initializer(VM_ANON,upage,writable,lazy_load_segment, fp)){
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		ofs += page_read_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
/* USER_STACK에서 스택의 PAGE를 생성합니다. 성공하면 true를 반환합니다. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);
	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */	
	/* TODO: 스택을 stack_bottom에 매핑하고 즉시 해당 페이지를 요구합니다.
	TODO: 성공하면 rsp를 적절히 설정합니다.
	TODO: 페이지를 스택으로 표시해야 합니다.
	TODO: 여기에 코드를 작성하세요 */
	if(vm_alloc_page(VM_MARKER_0 | VM_ANON, stack_bottom, 1)){
		// VM_MARKER_0: 스택이 저장된 메모리 페이지를 식별
		success = vm_claim_page(stack_bottom);
		if(success){
			if_->rsp = USER_STACK;
		}
	}

    return success;
}

#endif /* VM */
