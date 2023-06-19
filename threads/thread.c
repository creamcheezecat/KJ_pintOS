#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
/* THREAD_READY 상태인 프로세스들의 목록입니다.
즉, 실행 준비가 된 상태이지만 실제로 실행 중인 것은 아닙니다. */
static struct list ready_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
/* 초기 스레드, init.c:main()을 실행하는 스레드입니다. */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
/* allocate_tid()에서 사용되는 락입니다. */
static struct lock tid_lock;

/* Thread destruction requests */
/* 스레드 파괴 요청들 */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4		  /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule(void);
static tid_t allocate_tid(void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
/* 현재 실행 중인 스레드를 반환합니다.
CPU의 스택 포인터 'rsp'를 읽은 다음, 페이지의 시작 부분으로 내림합니다.
'struct thread'가 항상 페이지의 시작 부분에 있고 스택 포인터는 중간에 위치하기 때문에,
이를 통해 현재 스레드를 찾을 수 있습니다. */
#define running_thread() ((struct thread *)(pg_round_down(rrsp())))

// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

/*-----Alarm Clock---------------------------------------------------------*/

static struct list sleep_list;

/*
스레드를 잠재우는 함수 틱수를 받아서 그만큼 잠들게 스레드 wakeup-tick 에 틱수 저장후
슬림리스트에 저장하고 thread_block() 함수를 실행 잠드는 시간이 짧을 수록 맨 앞에 넣는다.
*/
void thread_sleep(int64_t ticks)
{
	enum intr_level old_level;
	old_level = intr_disable();
	struct thread * t = thread_current();
	/*
	현재 스레드가 idle 스레드가 아니라면 
	스레드를 sleep_list 에 스레드를 넣고
	재운다.
	*/ 
	ASSERT (!intr_context ());
	/* 현재 스레드가 idle 스레드가 아니라면 */
	if (t != idle_thread){
		/* 로컬 틱을 설정한다 */
		t->wakeup_tick = ticks;
		/* 로컬 틱이 작은 순으로 정렬해서 sleep_list를 정렬한다 */
		list_insert_ordered(&sleep_list, &t->elem,thread_compare_time,0);
		/* 스레드를 잠재운다 */
		thread_block();
	}

	intr_set_level(old_level);
}

/* 
타이머 인터럽트에서 글로벌 틱수를 받아와서 
슬립 리스트에 가장 짧게 잠드는 스레드와 글로벌 틱수를 비교해서
시간이 되면 thread_unblock() 함수를 블러온다.
*/
void thread_wakeup(int64_t ticks){
	enum intr_level old_level;
	old_level = intr_disable();

	/* 
	sleep_list에 스레드가 존재하고 sleep_list의 첫번째 스레드가 글로벌 틱수보다 같거나 높다면 
	첫번째 스레드를 pop 해주면서 unblock()에 호출해 준다.
	*/
	while(!list_empty(&sleep_list) && 
		list_entry(list_front(&sleep_list), struct thread, elem)->wakeup_tick <= ticks){
		thread_unblock(list_entry(list_pop_front(&sleep_list), struct thread, elem));
	}

	intr_set_level(old_level);
}

// 스레드의 로컬 타임 비교 전자가 더 높으면 false 후자가 높으면 true
bool thread_compare_time(const struct list_elem *a,
							 const struct list_elem *b,
							 void *aux)
{
	return list_entry(a, struct thread, elem)->wakeup_tick <= list_entry(b, struct thread, elem)->wakeup_tick;
}

/*------------------------------------------------------------------------*/

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
/* 현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화합니다.
이는 일반적으로 작동할 수 없으며,
여기서만 가능한 이유는 loader.S가 스택의 바닥을 페이지 경계에 두도록 신경썼기 때문입니다.

또한 실행 대기열과 TID 락을 초기화합니다.
이 함수를 호출한 후에는 thread_create()를 사용하여 스레드를 생성하기 전에 페이지 할당기를 초기화해야 합니다.
이 함수가 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */
void thread_init(void)
{
	ASSERT(intr_get_level() == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	/* 커널을 위한 임시 GDT를 다시 로드합니다.
	이 GDT에는 사용자 컨텍스트가 포함되어 있지 않습니다.
	커널은 gdt_init()에서 사용자 컨텍스트를 포함하여 GDT를 다시 구성할 것입니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof(gdt) - 1,
		.address = (uint64_t)gdt};
	lgdt(&gdt_ds);

	/* Init the globla thread context */
	lock_init(&tid_lock);
	list_init(&ready_list);
	list_init(&destruction_req);
	// sleep_list 초기화
	list_init(&sleep_list);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread();
	init_thread(initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
/* 인터럽트를 활성화하여 선점형 스레드 스케줄링을 시작합니다.
또한 idle 스레드를 생성합니다. */
void thread_start(void)
{
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init(&idle_started, 0);
	thread_create("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
/* 타이머 인터럽트 핸들러에 의해 각 타이머 틱마다 호출됩니다.
따라서, 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void thread_tick(void)
{
	struct thread *t = thread_current();

	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void)
{
	printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
		   idle_ticks, kernel_ticks, user_ticks);
}

/* thread_create() 추가사항 */
/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
/* 주어진 초기 우선순위로 NAME이라는 이름을 가진 새로운 커널 스레드를 생성합니다.
이 스레드는 FUNCTION을 실행하고 인자로 AUX를 전달하며, 준비 큐에 추가됩니다.
새로운 스레드의 스레드 식별자를 반환하며, 생성에 실패한 경우 TID_ERROR를 반환합니다.

만약 thread_start()가 호출된 상태라면, 새로운 스레드는 thread_create()가 반환되기 전에도 스케줄될 수 있습니다.
심지어 thread_create()가 반환되기 전에 종료될 수도 있습니다.
반대로, 새로운 스레드가 스케줄될 때까지 원래 스레드는 어떤 시간 동안이든 실행될 수 있습니다.
순서를 보장해야 하는 경우 세마포어나 다른 형태의 동기화를 사용하세요.

제공된 코드는 새로운 스레드의 `priority' 멤버를 PRIORITY로 설정하지만,
실제로 우선순위 스케줄링은 구현되어 있지 않습니다.
우선순위 스케줄링은 Problem 1-3의 목표입니다. */
tid_t thread_create(const char *name, int priority,
					thread_func *function, void *aux)
{
	struct thread *t;
	tid_t tid;

	ASSERT(function != NULL);

	/* Allocate thread. */
	t = palloc_get_page(PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread(t, name, priority);
	tid = t->tid = allocate_tid();

	/* project 2 파일 디스크립터 커널 영역에 할당 */
	t->fdt = palloc_get_page(PAL_ZERO);
	if(t->fdt == NULL){
		return TID_ERROR;
	}

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	/* 스케줄되었다면 kernel_thread를 호출합니다.
	참고) rdi는 첫 번째 인자이고, rsi는 두 번째 인자입니다. */
	t->tf.rip = (uintptr_t)kernel_thread;
	t->tf.R.rdi = (uint64_t)function;
	t->tf.R.rsi = (uint64_t)aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;
	/* Add to run queue. */
	thread_unblock(t);

	/* project 2 현재(부모)의 리스트에 자식을 등록*/
	list_push_back(&thread_current()->children,&t->child_elem);


	/* 스레드를 만들어서 ready_list 에 입력했으니까 비교를 한번 해준다*/
	thread_compare();

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
/* 현재 스레드를 잠자게 합니다.
thread_unblock()에 의해 깨어날 때까지 다시 스케줄되지 않습니다.

이 함수는 인터럽트가 꺼진 상태에서 호출해야 합니다.
보통은 synch.h에 있는 동기화 기본 요소 중 하나를 사용하는 것이 더 좋은 방법입니다. */
void thread_block(void)
{
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);

	//printf("%d\n",thread_current()->priority);
	thread_current()->status = THREAD_BLOCKED;
	schedule();
}

/* thread_unblock() 추가사항 */
/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
/* 차단된 스레드 T를 실행 준비 상태로 전환합니다.
T가 차단되지 않은 경우, 오류가 발생합니다.
(실행 중인 스레드를 준비 상태로 만들려면 thread_yield()를 사용하세요.)

이 함수는 실행 중인 스레드를 선점하지 않습니다.
이는 중요할 수 있습니다. 호출자가 인터럽트를 비활성화한 경우,
스레드를 원자적으로 차단 해제하고
다른 데이터를 업데이트할 수 있다고 기대할 수 있습니다. */
void thread_unblock(struct thread *t)
{
	enum intr_level old_level;

	ASSERT(is_thread(t));

	old_level = intr_disable();
	ASSERT(t->status == THREAD_BLOCKED);
	// ready_list에 스레드를 입력할때 우선순위 높은 순으로 정렬해서 입력한다
	list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, 0);
	t->status = THREAD_READY;
	intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name(void)
{
	return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
/* 실행 중인 스레드를 반환합니다.
이는 running_thread() 함수와 몇 가지 검증 절차를 포함한 것입니다.
자세한 내용은 thread.h 파일 상단에 있는 큰 주석을 참조하세요. */
struct thread *
thread_current(void)
{
	struct thread *t = running_thread();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	/* T가 실제로 스레드인지 확인합니다.
	만약 이러한 어설션 중 하나가 작동한다면,
	해당 스레드는 스택 오버플로우가 발생한 것일 수 있습니다.
	각 스레드는 4 kB 미만의 스택을 가지고 있으므로,
	몇 개의 큰 자동 배열이나 중간 정도의 재귀 호출은 스택 오버플로우를 발생시킬 수 있습니다. */
	ASSERT(is_thread(t));
	ASSERT(t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
/* 현재 실행 중인 스레드의 tid를 반환합니다. */
tid_t thread_tid(void)
{
	return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
/* 현재 스레드를 스케줄에서 제외하고 파괴합니다.
호출자에게는 절대로 반환되지 않습니다. */
void thread_exit(void)
{
	ASSERT(!intr_context());

#ifdef USERPROG
	process_exit();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	/* 우리의 상태를 종료 중으로 설정하고 다른 프로세스를 스케줄합니다.
	schedule_tail() 호출 중에 우리는 파괴될 것입니다. */
	intr_disable();
	do_schedule(THREAD_DYING);
	NOT_REACHED();
}

/* thread_yield() 추가사항 */
/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* CPU 양보(Yield)를 수행합니다. 현재 스레드는 슬립 상태로 전환되지 않으며,
스케줄러의 판단에 따라 즉시 다시 스케줄될 수 있습니다. */
void thread_yield(void)
{
	struct thread *curr = thread_current();
	enum intr_level old_level;


	ASSERT(!intr_context());
	old_level = intr_disable();
	if (curr != idle_thread){
		list_insert_ordered(&ready_list, &curr->elem, thread_compare_priority, 0);
	}
	do_schedule(THREAD_READY);
	intr_set_level(old_level);
}

/*-----priority scheduling-----------------------------------------------*/

// 우선순위 비교 전자 가 후자보다 크다면 true
bool thread_compare_priority(const struct list_elem *a,
							 const struct list_elem *b,
							 void *aux)
{
	return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority;
}

/* 
현재 실행중인 쓰레드와 대기 중인 쓰레드의 우선 순위 비교해서
실행중인 쓰레드가 우선순위가 낮다면 양보한다.
*/
void thread_compare(void)
{
	// ready_list에 스레드가 존재한다면
	if (!list_empty(&ready_list))
	{
		/*
		현재 실행중인 스레드와 ready_list에 우선순위가 가장 높은 스레드와 비교해서
		실행 중인 스레드가 낮다면 thread_yield()를 호출해서 실행 중인 스레드를 잠재운다.
		*/ 
		if (thread_compare_priority(&list_entry(list_front(&ready_list), struct thread, elem)->elem,
									&thread_current()->elem, 0))
		{
			thread_yield();
		}
	}
}

/*-----------------------------------------------------------------------*/

/* thread_set_priority() 추가사항 */
/* Sets the current thread's priority to NEW_PRIORITY. */
/* 현재 스레드의 우선순위를 NEW_PRIORITY로 설정합니다. */
void thread_set_priority(int new_priority)
{
	struct thread *t = thread_current();
	t->priority = new_priority;
	t->init_priority = new_priority;
	
	thread_donate_reset(t);
	thread_compare();
}

/* Returns the current thread's priority. */
/* 현재 스레드의 우선순위를 반환합니다. */
int thread_get_priority(void)
{
	struct thread *t = thread_current();

	return t->priority;
}

/*-----lock and donate-----------------------------------------------------*/

/* 
우선순위 높은 스레드가 자원? 점유를 못해서 이미 
낮은 순위에 있는 스레드에게 자신의 우선순위 를 기부한다

함수에 들어오는 스레드는 우선순위가 높은 스레드
스레드 t 는 점유하고 있는 스레드
*/
void thread_donate(struct thread * t){
	enum intr_level old_level;
	ASSERT(is_thread(t));
	old_level = intr_disable();
	/* 
	thread_current() = 이미 락을 점유하고 있는 스레드보다 우선순위가 높아서 점유할려고 들어왔다 
	락을 점유하는 스레드에게 우선순위를 기부하기 위해 donations 에 d_elem을 입력한다.
	*/
	list_insert_ordered(&t->donations,&thread_current()->d_elem,thread_compare_donate_priority,0);
	
	thread_donate_depth();

	intr_set_level(old_level);
}

/* 점유를 풀려고 들어오는 공간 */
void thread_remove_donate(struct lock *release_locker){
	// 지금 락 점유를 갖고 있는 스레드를 지정한다
	struct thread *t = release_locker->holder;
	struct list_elem *e = NULL;
	
	/* 기부 받은 우선순위가 변경 될 수 있으므로 donations를 순회 하면서 겹치는게 있다면 제거해준다.*/
	for (e = list_begin(&t->donations); e != list_end(&t->donations); e = list_next(e)){
		// 기부 받은 스레드의 락이 지금 락하고 같다면
		if(list_entry(e,struct thread,d_elem)->wait_on_lock == release_locker){
			list_remove(e);
		}
	}
}

/* 스레드의 d_elem을 기준 삼아서 비교를 진행한다*/
bool thread_compare_donate_priority(const struct list_elem *a,
							 const struct list_elem *b,
							 void *aux)
{
	return list_entry(a, struct thread, d_elem)->priority > list_entry(b, struct thread, d_elem)->priority;
}

/* 기부 받은 스레드가 변경 되었다면 리셋 해줘야한다*/
void thread_donate_reset(struct thread *t){
	// 기본적으로 본래 자신의 우선순위로 갱신
	t->priority = t->init_priority;
	
	if(!list_empty(&t->donations)){
		// 혹시라도 donations 리스트가 정렬이 안될 수 도 있기 때문에 정렬한다
		list_sort(&t->donations,thread_compare_donate_priority,0);

		if(t->priority < list_entry (list_front (&t->donations), struct thread, elem)->priority){
			t->priority = list_entry (list_front (&t->donations), struct thread, elem)->priority;
		}
	}
}

/* 락을 점유하고 있는 스레드들에게 현재 스레드의 우선순위를 기부한다.*/
void thread_donate_depth(void){
	int depth;
	// 우선 순위가 높은 스레드이지만 락을 점유 못해서 기부하러 온 스레드
	struct thread *curr = thread_current();

	for (depth = 0; depth < 8; depth++) {
		if (!curr->wait_on_lock) break;
		// 락을 점유하고 있는 스레드를 다 순회 한다.
		struct thread *holder = curr->wait_on_lock->holder;
		// 현재 우선순위가 가장 큰 값이므로 현재 우선순위로 변경 해준다.
		holder->priority = curr->priority;
		// 다음 락을 점유 하고 있는 스레드로 바꿔준다.
		curr = holder;
	}
}


/*------------------------------------------------------------------------*/

/* Sets the current thread's nice value to NICE. */
/* 현재 스레드의 nice 값을 NICE로 설정합니다. */
void thread_set_nice(int nice UNUSED)
{
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
/* 현재 스레드의 nice 값을 반환합니다. */
int thread_get_nice(void)
{
	/* TODO: Your implementation goes here */

	return 0;
}

/* Returns 100 times the system load average. */
/* 시스템 로드 평균의 100배를 반환합니다. */
int thread_get_load_avg(void)
{
	/* TODO: Your implementation goes here */

	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
/* 현재 스레드의 recent_cpu 값의 100배를 반환합니다. */
int thread_get_recent_cpu(void)
{
	/* TODO: Your implementation goes here */

	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
/* 아이들 스레드. 다른 실행 대기 중인 스레드가 없을 때 실행됩니다.

아이들 스레드는 thread_start()에 의해 초기에 실행 대기 목록에 추가됩니다.
초기에 한 번 스케줄되며, 그 시점에서 idle_thread를 초기화하고 전달된 세마포를 "up"하여
thread_start()가 계속 실행되도록 허용한 후 즉시 블록됩니다.
그 이후에는 아이들 스레드는 더 이상 실행 대기 목록에 나타나지 않습니다.
ready list가 비어있을 때 특별한 경우로 next_thread_to_run()에 의해 반환됩니다. */
static void
idle(void *idle_started_ UNUSED)
{
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current();
	sema_up(idle_started);

	for (;;)
	{
		/* Let someone else run. */
		/* 다른 스레드에게 실행을 양보합니다. */
		intr_disable();
		thread_block();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		/* 인터럽트를 다시 활성화하고 다음 인터럽트를 기다립니다.

		'sti' 명령은 다음 명령이 완료될 때까지 인터럽트를 비활성화하므로,
		이 두 개의 명령은 원자적으로 실행됩니다.
		이 원자성은 중요합니다.
		그렇지 않으면 인터럽트가 인터럽트를 다시 활성화하고
		다음 인터럽트가 발생하기를 기다리는 동안 처리될 수 있으며,
		이는 한 클록 틱 정도의 시간을 낭비할 수 있습니다.

		참조: [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a] 7.11.1 "HLT Instruction" */
		asm volatile("sti; hlt"
					 :
					 :
					 : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
/* 커널 스레드의 기반으로 사용되는 함수입니다. */
static void
kernel_thread(thread_func *function, void *aux)
{
	ASSERT(function != NULL);

	intr_enable(); /* The scheduler runs with interrupts off. */ /* 인터럽트가 비활성화된 상태에서 스케줄러가 실행됩니다. */
	function(aux); /* Execute the thread function. */			 /* 스레드 함수를 실행 */
	thread_exit(); /* If function() returns, kill the thread. */ /* function()이 반환되면 스레드를 종료합니다. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
/* T를 NAME이라는 이름의 차단된 스레드로 기본 초기화합니다. */
static void
init_thread(struct thread *t, const char *name, int priority)
{
	ASSERT(t != NULL);
	ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT(name != NULL);

	memset(t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy(t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t)t + PGSIZE - sizeof(void *);
	t->priority = priority;
	/* project 1  초기화*/
	t->init_priority = priority;
	t->wait_on_lock = NULL;
	list_init(&t->donations);

	/* project 2 초기화*/
	list_init(&t->children);
	t->exit_status = 0;
	t->next_fd = 2;
	sema_init(&t->wait_sema,0);
	sema_init(&t->clear_sema,0);
	sema_init(&t->fork_sema,0);
	//t->donate_priority = priority;
	t->magic = THREAD_MAGIC;

	
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
/* 다음으로 스케줄링할 스레드를 선택하고 반환합니다.
실행 대기열에 스레드가 있는 경우, 실행 대기열에서 스레드를 반환해야 합니다.
(현재 실행 중인 스레드가 계속 실행될 수 있다면, 실행 대기열에 있을 것입니다.)
 실행 대기열이 비어 있는 경우, idle_thread를 반환합니다. */
static struct thread *
next_thread_to_run(void)
{
	if (list_empty(&ready_list))
		return idle_thread;
	else
		return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
/* 스레드를 실행하기 위해 iretq를 사용합니다. */
void do_iret(struct intr_frame *tf)
{
	__asm __volatile(
		"movq %0, %%rsp\n"   // 스택 포인터 값을 복원합니다.
		"movq 0(%%rsp),%%r15\n"   // 스택에서 값을 읽어와 r15 레지스터에 저장합니다.
		"movq 8(%%rsp),%%r14\n"   // 스택에서 값을 읽어와 r14 레지스터에 저장합니다.
		"movq 16(%%rsp),%%r13\n"   // 스택에서 값을 읽어와 r13 레지스터에 저장합니다.
		"movq 24(%%rsp),%%r12\n"   // 스택에서 값을 읽어와 r12 레지스터에 저장합니다.
		"movq 32(%%rsp),%%r11\n"   // 스택에서 값을 읽어와 r11 레지스터에 저장합니다.
		"movq 40(%%rsp),%%r10\n"   // 스택에서 값을 읽어와 r10 레지스터에 저장합니다.
		"movq 48(%%rsp),%%r9\n"   // 스택에서 값을 읽어와 r9 레지스터에 저장합니다.
		"movq 56(%%rsp),%%r8\n"   // 스택에서 값을 읽어와 r8 레지스터에 저장합니다.
		"movq 64(%%rsp),%%rsi\n"   // 스택에서 값을 읽어와 rsi 레지스터에 저장합니다.
		"movq 72(%%rsp),%%rdi\n"   // 스택에서 값을 읽어와 rdi 레지스터에 저장합니다.
		"movq 80(%%rsp),%%rbp\n"   // 스택에서 값을 읽어와 rbp 레지스터에 저장합니다.
		"movq 88(%%rsp),%%rdx\n"   // 스택에서 값을 읽어와 rdx 레지스터에 저장합니다.
		"movq 96(%%rsp),%%rcx\n"   // 스택에서 값을 읽어와 rcx 레지스터에 저장합니다.
		"movq 104(%%rsp),%%rbx\n"   // 스택에서 값을 읽어와 rbx 레지스터에 저장합니다.
		"movq 112(%%rsp),%%rax\n"   // 스택에서 값을 읽어와 rax 레지스터에 저장합니다.
		"addq $120,%%rsp\n"   // 스택 포인터를 120만큼 증가시킵니다.
		"movw 8(%%rsp),%%ds\n"   // 스택에서 값을 읽어와 ds 레지스터에 저장합니다.
		"movw (%%rsp),%%es\n"   // 스택에서 값을 읽어와 es 레지스터에 저장합니다.
		"addq $32, %%rsp\n"   // 스택 포인터를 32만큼 증가시킵니다.
		"iretq"   // 인터럽트 복원 및 반환 명령을 실행합니다.
		:
		: "g"((uint64_t)tf)   // tf 변수를 피연산자로 사용합니다.
		: "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
/* 새로운 스레드의 페이지 테이블을 활성화하여 스레드를 전환하고,
이전 스레드가 종료 중인 경우에는 파괴합니다.

이 함수가 호출될 때, 우리는 이미 스레드 PREV에서 전환한 상태이며,
새로운 스레드가 이미 실행 중이고 인터럽트는 아직 비활성화된 상태입니다.

스레드 전환이 완료되기 전까지는 printf()를 호출하는 것이 안전하지 않습니다.
실제로는 printf()를 함수의 끝에 추가해야 합니다. */
static void
thread_launch(struct thread *th)
{
	uint64_t tf_cur = (uint64_t)&running_thread()->tf;
	uint64_t tf = (uint64_t)&th->tf;
	ASSERT(intr_get_level() == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */

	/* 주요 전환 로직입니다.

	먼저 전체 실행 컨텍스트를 intr_frame으로 복원한 다음,
	do_iret을 호출하여 다음 스레드로 전환합니다.
	전환이 완료될 때까지 여기서는 스택을 사용해서는 안 됩니다. */
	__asm __volatile(
		/* Store registers that will be used. */
		/* 사용될 레지스터를 저장합니다. */
		"push %%rax\n"
		"push %%rbx\n"
		"push %%rcx\n"
		/* Fetch input once */
		/* 한 번에 입력을 가져옵니다. */
		"movq %0, %%rax\n"
		"movq %1, %%rcx\n"
		"movq %%r15, 0(%%rax)\n"
		"movq %%r14, 8(%%rax)\n"
		"movq %%r13, 16(%%rax)\n"
		"movq %%r12, 24(%%rax)\n"
		"movq %%r11, 32(%%rax)\n"
		"movq %%r10, 40(%%rax)\n"
		"movq %%r9, 48(%%rax)\n"
		"movq %%r8, 56(%%rax)\n"
		"movq %%rsi, 64(%%rax)\n"
		"movq %%rdi, 72(%%rax)\n"
		"movq %%rbp, 80(%%rax)\n"
		"movq %%rdx, 88(%%rax)\n"
		"pop %%rbx\n" // Saved rcx
		"movq %%rbx, 96(%%rax)\n"
		"pop %%rbx\n" // Saved rbx
		"movq %%rbx, 104(%%rax)\n"
		"pop %%rbx\n" // Saved rax
		"movq %%rbx, 112(%%rax)\n"
		"addq $120, %%rax\n"
		"movw %%es, (%%rax)\n"
		"movw %%ds, 8(%%rax)\n"
		"addq $32, %%rax\n"
		"call __next\n" // read the current rip.
		"__next:\n"
		"pop %%rbx\n"
		"addq $(out_iret -  __next), %%rbx\n"
		"movq %%rbx, 0(%%rax)\n" // rip
		"movw %%cs, 8(%%rax)\n"	 // cs
		"pushfq\n"
		"popq %%rbx\n"
		"mov %%rbx, 16(%%rax)\n" // eflags
		"mov %%rsp, 24(%%rax)\n" // rsp
		"movw %%ss, 32(%%rax)\n"
		"mov %%rcx, %%rdi\n"
		"call do_iret\n"
		"out_iret:\n"
		:
		: "g"(tf_cur), "g"(tf)
		: "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
/* 새로운 프로세스를 스케줄합니다. 진입 시, 인터럽트는 꺼져 있어야 합니다.
이 함수는 현재 스레드의 상태를 status로 변경한 후
다른 실행할 스레드를 찾아 전환합니다.
schedule()에서 printf()를 호출하는 것은 안전하지 않습니다. */
static void
do_schedule(int status)
{
	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(thread_current()->status == THREAD_RUNNING);

	while (!list_empty(&destruction_req))
	{
		struct thread *victim =
			list_entry(list_pop_front(&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current()->status = status;
	schedule();
}

static void
schedule(void)
{
	struct thread *curr = running_thread();
	struct thread *next = next_thread_to_run();

	ASSERT(intr_get_level() == INTR_OFF);
	ASSERT(curr->status != THREAD_RUNNING);
	ASSERT(is_thread(next));

	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;
	
#ifdef USERPROG
	/* Activate the new address space. */
	/* 새로운 주소 공간을 활성화합니다. */
	process_activate(next);
#endif
	if (curr != next)
	{
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		/* 전환한 스레드가 종료 중인 경우, 해당 스레드의 struct thread을 파괴합니다.
		이는 thread_exit()이 자신의 바닥을 빼앗지 않도록 뒤늦게 발생해야 합니다.
		여기서는 페이지를 현재 스택이 사용 중인 상태로 페이지 해제 요청만 큐에 추가합니다.
		실제 파괴 로직은 schedule()의 시작 부분에서 호출될 것입니다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread)
		{
			ASSERT(curr != next);
			list_push_back(&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		/* 스레드를 전환하기 전에, 현재 실행 중인 정보를 먼저 저장합니다. */
		thread_launch(next);
	}
}

/* Returns a tid to use for a new thread. */
/* 새로운 스레드에 사용할 tid를 반환합니다. */
static tid_t
allocate_tid(void)
{
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire(&tid_lock);
	tid = next_tid++;
	lock_release(&tid_lock);

	return tid;
}
