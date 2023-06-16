#ifndef DEVICES_INTQ_H
#define DEVICES_INTQ_H

#include "threads/interrupt.h"
#include "threads/synch.h"

/* An "interrupt queue", a circular buffer shared between
   kernel threads and external interrupt handlers.

   Interrupt queue functions can be called from kernel threads or
   from external interrupt handlers.  Except for intq_init(),
   interrupts must be off in either case.

   The interrupt queue has the structure of a "monitor".  Locks
   and condition variables from threads/synch.h cannot be used in
   this case, as they normally would, because they can only
   protect kernel threads from one another, not from interrupt
   handlers. */
/* "인터럽트 큐"는 커널 스레드와 외부 인터럽트 핸들러 간에 공유되는 원형 버퍼입니다.

인터럽트 큐 함수는 커널 스레드나 외부 인터럽트 핸들러에서 호출될 수 있습니다.
intq_init()를 제외하고는 양쪽 경우 모두 인터럽트가 비활성화되어야 합니다.

인터럽트 큐는 "모니터"의 구조를 가지고 있습니다.
threads/synch.h의 락과 조건 변수는 일반적으로 사용되지만, 
인터럽트 핸들러로부터 커널 스레드를 보호할 수 없기 때문에 사용할 수 없습니다. */

/* Queue buffer size, in bytes. */
#define INTQ_BUFSIZE 64

/* A circular queue of bytes. */
struct intq {
	/* Waiting threads. */
	struct lock lock;           /* Only one thread may wait at once. */
	struct thread *not_full;    /* Thread waiting for not-full condition. */
	struct thread *not_empty;   /* Thread waiting for not-empty condition. */

	/* Queue. */
	uint8_t buf[INTQ_BUFSIZE];  /* Buffer. */
	int head;                   /* New data is written here. */
	int tail;                   /* Old data is read here. */
};

void intq_init (struct intq *);
bool intq_empty (const struct intq *);
bool intq_full (const struct intq *);
uint8_t intq_getc (struct intq *);
void intq_putc (struct intq *, uint8_t);

#endif /* devices/intq.h */
