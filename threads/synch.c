/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"



/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
/* 세마포어(SEMA)를 VALUE로 초기화합니다. 세마포어는 음이 아닌 정수와 함께 두 개의 원자적 연산자를 가지고 있어 이를 조작합니다:

down 또는 "P": 값이 양수가 될 때까지 기다린 다음 값을 감소시킵니다.

up 또는 "V": 값을 증가시킵니다 (그리고 대기 중인 스레드가 있다면 하나를 깨웁니다). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
/* 세마포어에 대한 down 또는 "P" 연산입니다. 
SEMA의 값이 양수가 될 때까지 기다렸다가 원자적으로 값을 감소시킵니다.

이 함수는 sleep할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 
이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, 
sleep을 하면 다음 예약된 스레드가 인터럽트를 다시 활성화할 가능성이 있습니다. 
이것은 sema_down 함수입니다. */
void
sema_down (struct semaphore *sema) {
	ASSERT (sema != NULL);
	ASSERT (!intr_context ());
	enum intr_level old_level;
	old_level = intr_disable();
	// thread_current() = 여기는 여러가지 스레드들이 들어온다.
	while (sema->value == 0) {
		/* 
		thread_current() = 이미 락을 점유하고 있는 스레드보다 우선순위가 높아서 점유할려고 들어왔다 
		sema->waiters(리스트)에 입력할때 우선순위가 높은 순으로 정렬해서 입력한다.
		*/
		list_insert_ordered(&sema->waiters, &thread_current()->elem,thread_compare_priority,0);
		// 준비과정이 끝나면 thread_current() 를 잠재운다.
		thread_block ();
	}

	sema->value--;

	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
/* 세마포어에 대한 down 또는 "P" 연산을 수행하지만, 
세마포어가 이미 0이 아닌 경우에만 수행됩니다. 
세마포어가 감소되면 true를 반환하고, 그렇지 않으면 false를 반환합니다.

이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 세마포어에 대한 up 또는 "V" 연산입니다. 
SEMA의 값을 증가시키고, SEMA를 기다리는 스레드 중 하나를 깨웁니다 (있는 경우).

이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;
	old_level = intr_disable ();
	ASSERT (sema != NULL);
	// thread_current() = 여러가지 스레드들이 들어올 수있다.
	
	/* 락에 대기중인 스레드가 존재한다면 */
	if (!list_empty (&sema->waiters)){
		/* 
		sema_up 에서 정렬하더라도 락에서 스레드를 공유하고 우선순위 기부로 인한 
		우선 순위가 바뀔 수 있기 때문에 unblock 전에 정렬을 해줘야한다.
		예시) priority-donate-sema 테스트 케이스
		*/
		list_sort(&sema->waiters,thread_compare_priority,0);
		// waiters 에서 우선순위 높은 스레드를 먼저 깨운다.
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
		
	}

	sema->value++;

	thread_compare();

	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
   
/* 세마포어의 자가 테스트로, 
한 쌍의 스레드 간에 "핑-퐁" 형태의 제어를 수행합니다. 
진행 상황을 확인하기 위해 printf() 호출을 삽입하세요. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
/* sema_self_test()에서 사용되는 스레드 함수입니다. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}


/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
/* LOCK을 초기화합니다. 
한 번에 최대 하나의 스레드만이 잠금을 보유할 수 있습니다. 
우리의 잠금은 "재귀적(recursive)"이 아니므로, 
현재 잠금을 보유한 스레드가 해당 잠금을 다시 얻으려고 시도하는 것은 오류입니다.

잠금은 초기 값이 1인 세마포어의 특수한 형태입니다. 
잠금과 이러한 세마포어의 차이점은 두 가지입니다. 
첫째, 세마포어는 1보다 큰 값을 가질 수 있지만, 
잠금은 한 번에 한 스레드만 소유할 수 있습니다. 
둘째, 세마포어는 소유자가 없으므로 한 스레드가 
세마포어를 "down"하고 다른 스레드가 "up"할 수 있지만, 
잠금의 경우 동일한 스레드가 잠금을 획득하고 해제해야 합니다. 
이러한 제한이 불편하게 느껴질 때는 잠금 대신 세마포어를 사용해야 하는 좋은 신호입니다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* LOCK을 획득하며, 필요한 경우 사용 가능해질 때까지 대기합니다. 
잠금은 현재 스레드에 의해 이미 보유되어서는 안 됩니다.

이 함수는 sleep할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 
이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만, 
sleep해야 할 경우 인터럽트가 다시 활성화됩니다. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	/*잠금을 사용할 수 없는 경우 잠금 주소를 저장합니다.
	현재 우선 순위를 저장하고 기부된 스레드를 목록에 유지합니다(다중 기부).
	우선적으로 기부하세요.*/
	// thread_current() = 여러가지 스레드들이 들어올 수있다.

	if(lock->holder){
		/* 
		thread_current() = 이미 락을 점유하고 있는 스레드보다 우선순위가 높아서 점유할려고 들어왔다 
		현재 스레드에 락을 입력해준다.
		*/
		thread_current()->wait_on_lock = lock;
		thread_donate(lock->holder);
	}

	sema_down (&lock->semaphore);
	// sema_down에서 나온다는 것은 락 점유 했다는 뜻
	thread_current()->wait_on_lock = NULL;
	lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
/* LOCK을 획득하려고 시도하고, 성공하면 true를 반환하고 실패하면 false를 반환합니다. 
잠금은 현재 스레드에 의해 이미 보유되어서는 안 됩니다.

이 함수는 sleep하지 않으므로 인터럽트 핸들러 내에서 호출될 수 있습니다. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

/* 현재 스레드가 소유한 LOCK을 해제합니다. 이것은 lock_release 함수입니다.

인터럽트 핸들러는 잠금을 획득할 수 없으므로, 
인터럽트 핸들러 내에서 잠금을 해제하려는 것은 의미가 없습니다. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));
	/*
	잠금이 해제되면 기부 목록에서 
	잠금을 유지하고 있는 스레드를 제거하고 우선 순위를 적절하게 설정하십시오.
	*/
	// thread_current() = 여러가지 스레드들이 들어올 수있다.
	// 락에 대기중인 스레드가 있다면
	if(!list_empty (&lock->semaphore.waiters)){
		thread_remove_donate(lock);
		thread_donate_reset(lock->holder);
	}
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
/* 현재 스레드가 LOCK을 보유하고 있는 경우 true를 반환하고, 
그렇지 않은 경우 false를 반환합니다. 
(다른 스레드가 잠금을 보유하고 있는지 테스트하는 것은 경쟁 상태가 발생할 수 있습니다.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
/* 리스트 내의 하나의 세마포어입니다. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
/* 조건 변수 COND을 초기화합니다. 
   조건 변수는 한 코드 조각이 조건을 신호로 보내고, 협력하는 코드가 그 신호를 받고 해당 조건에 따라 작업을 수행할 수 있게 합니다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 이 함수는 LOCK을 원자적으로 해제하고, 다른 코드에 의해 COND가 신호를 받을 때까지 대기합니다. 
COND가 신호를 받은 후에는 반환하기 전에 LOCK을 다시 획득합니다. 
이 함수를 호출하기 전에 LOCK이 보유되어 있어야 합니다.

이 함수로 구현된 모니터는 "Mesa" 스타일이며 "Hoare" 스타일이 아닙니다. 
즉, 신호를 보내고 받는 것이 원자적인 동작이 아닙니다. 
따라서 대기가 완료된 후에는 호출자가 조건을 다시 확인하고 필요한 경우 다시 대기해야 합니다.

주어진 조건 변수는 단일 락과 관련이 있지만, 
하나의 락은 여러 개의 조건 변수와 관련될 수 있습니다. 
즉, 락에서 조건 변수로의 일대다 매핑이 있습니다.

이 함수는 슬립(sleep)할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 
인터럽트가 비활성화된 상태에서 이 함수를 호출할 수는 있지만, 
슬립이 필요한 경우 인터럽트가 다시 활성화됩니다.*/
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	/* 우선 순위가 높은 순으로 정렬해서 cond_waiters에 입력한다*/
	list_insert_ordered(&cond->waiters, &waiter.elem,cond_compare_waiter,0);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/*만약 COND(LOCK로 보호된)에 대기 중인 스레드가 있다면, 
이 함수는 그 중 하나에게 대기를 해제하도록 신호를 보냅니다. 
이 함수를 호출하기 전에 LOCK이 보유되어 있어야 합니다.

인터럽트 핸들러는 락을 획득할 수 없으므로, 
인터럽트 핸들러 내에서 조건 변수에 신호를 보내려는 것은 의미가 없습니다.*/
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));
	/*조건 변수에서 대기 중인 가장 높은 우선 순위의 스레드에 신호를 보냅니다.*/
	if (!list_empty (&cond->waiters))
		/* sema_up 하기 전에 리스트를 정렬을 다시 한번 해준다.*/
		list_sort(&cond->waiters,cond_compare_waiter,0);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* COND에 대기 중인 (있는 경우) 모든 스레드를 깨웁니다 (LOCK으로 보호됨).
이 함수를 호출하기 전에 LOCK을 획득해야 합니다.

인터럽트 핸들러는 잠금을 획득할 수 없으므로 인터럽트 핸들러 내에서
조건 변수를 시그널하는 것은 의미가 없습니다. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	/*조건 변수에서 대기 중인 모든 스레드에 신호를 보냅니다.*/
	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
/* sema 안에 있는 리스트에 접근해서 우선순위를 비교한다*/
bool cond_compare_waiter(const struct list_elem *a,
							 const struct list_elem *b,
							 void *aux)
{
	struct semaphore_elem *sem_a = list_entry(a, struct semaphore_elem, elem);
    struct semaphore_elem *sem_b = list_entry(b, struct semaphore_elem, elem);
    
    struct thread *thread_a = NULL;
    if (!list_empty(&sem_a->semaphore.waiters))
    {
        thread_a = list_entry(list_front(&sem_a->semaphore.waiters), struct thread, elem);
    }
    
    struct thread *thread_b = NULL;
    if (!list_empty(&sem_b->semaphore.waiters))
    {
        thread_b = list_entry(list_front(&sem_b->semaphore.waiters), struct thread, elem);
    }
    // 두 리스트가 모두 비어있는 경우에도 처리를 추가
    if (thread_a == NULL && thread_b == NULL)
    {
        return false; 
    }
    // a 리스트가 비어있는 경우 처리를 추가
    if (thread_a == NULL)
    {
        return false; 
    }
    // b 리스트가 비어있는 경우 처리를 추가
    if (thread_b == NULL)
    {
        return true; 
    }
    return thread_a->priority > thread_b->priority;
}
