#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 * 
 * 커널 스레드 또는 사용자 프로세스입니다.
 * 각 스레드 구조체는 고유한 4 kB 페이지에 저장됩니다. 
 * 스레드 구조체 자체는 페이지의 맨 아래에 위치합니다 (offset 0). 
 * 페이지의 나머지 부분은 스레드의 커널 스택을 위해 예약되어 있으며, 
 * 이는 페이지의 맨 위 (offset 4 kB)에서 아래로 성장합니다. 
 * 다음은 이를 설명한 그림입니다:
 * 
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 * 
 *	결과적으로 이는 두 가지 측면에서 중요합니다:
 * 
 * 	첫째, struct thread는 너무 커지지 않도록 제한되어야 합니다. 
 *  만약 커진다면 커널 스택에 충분한 공간이 없게 될 것입니다. 
 *  우리의 기본 struct thread는 몇 바이트 밖에 되지 않습니다. 
 *  일반적으로 1 kB 이하로 유지되는 것이 좋습니다.
 * 
 *  둘째, 커널 스택은 너무 크게 성장해서는 안 됩니다. 
 *  스택이 오버플로우되면 스레드 상태가 손상될 수 있습니다. 
 *  따라서 커널 함수에서는 비정적 지역 변수로 큰 구조체나 배열을 
 *  할당해서는 안 됩니다. 
 *  대신 malloc()이나 palloc_get_page()를 사용하여 동적 할당을 해야 합니다.
 * 	
 * 이러한 문제 중 하나의 첫 번째 증상은 
 * 일반적으로 thread_current()에서의 어설션 실패일 것입니다. 
 * thread_current()는 실행 중인 스레드의 
 * struct thread의 magic 멤버가 THREAD_MAGIC로 설정되어 있는지 확인합니다. 
 * 스택 오버플로우는 일반적으로 이 값을 변경하여 어설션을 트리거합니다.
 * 
 * elem 멤버는 두 가지 용도를 가지고 있습니다. 
 * 첫째로, run queue (thread.c)의 요소일 수 있고, 
 * 둘째로, 세마포어 대기 목록 (synch.c)의 요소일 수 있습니다. 
 * 이 두 가지 방식으로만 사용할 수 있는 이유는 서로 배타적인데 있습니다: 
 * 준비 상태의 스레드만이 run queue에 있을 뿐, 
 * 차단 상태의 스레드만이 세마포어 대기 목록에 있습니다.
 * 
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

	int64_t wakeup_tick;				/* 잘때 깨어나야할 시간을 저장하는 변수*/
	int init_priority;					/* 처음 부터 갖고있는 우선순위 */
	int exit_status;					/* project 2 자신의 status 를 저장할 변수*/

	struct lock *wait_on_lock;          /* 락에 대한 정보를 담을 구조체*/
	struct semaphore wait_sema;			/* project 2 wait , exit 에서 사용할 세마포어 */
	struct semaphore fork_sema;			/* project 2 fork 에서 사용할 세마포어 */
	struct semaphore clear_sema;		/* project 2 자식 들의 syn를 위해 */

	struct list donations;				/* 자신에게 기부한 스레드를 담을 리스트*/
	struct list children;				/* project 2 자식 리스트*/
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	struct list_elem d_elem;			/* donations 리스트에 쓰일 원소 */
	struct list_elem child_elem;		/* project 2 리스트에 쓰일 원소 */
	
	struct file *running_file;			/* project 2 현재 실행 중인 파일을 담을 변수*/
	struct file **fdt;					/* project 2 파일 디스크립터 설정 */
	int next_fd;						/* project 2 다음 파일을 넣을 인덱스 */
	#ifdef USERPROG
	
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
	#endif
	#ifdef VM

	
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	#endif
	struct intr_frame fork_if;			/*  project 2 fork 시 사용할 프레임 */
	/* Owned by thread.c. */
	/* thread.c에서 소유됩니다. */
	struct intr_frame tf;               /* Information for switching */ /* 스위칭을 위한 정보입니다. */
	unsigned magic;                     /* Detects stack overflow. */ /* 스택 오버플로우를 감지합니다. */

	//struct hash vm;						/* 스레드가 가진 가상 주소 공간을 관리하는 해쉬 */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/*-----Alarm Clock-------------------------------------------------------*/

void thread_sleep(int64_t);
void thread_wakeup(int64_t);
bool thread_compare_time(const struct list_elem *a,
							 const struct list_elem *b,
							 void *aux);

/*-----------------------------------------------------------------------*/



void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);



typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);




/*-----priority scheduler------------------------------------------------*/

bool thread_compare_priority(const struct list_elem *, const struct list_elem *, void *);

void thread_compare(void);

/*-----------------------------------------------------------------------*/




int thread_get_priority (void);
void thread_set_priority (int);





/*-----lock and donate---------------------------------------------------*/

void thread_donate(struct thread *);
void thread_remove_donate(struct lock *);
void thread_donate_reset(struct thread *t);
bool thread_compare_donate_priority(const struct list_elem *,
							 const struct list_elem *,
							 void *);
void thread_donate_depth();

/*-----------------------------------------------------------------------*/



int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif /* threads/thread.h */
