#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

/* Interrupts on or off? */
enum intr_level {
	INTR_OFF,             /* Interrupts disabled. */
	INTR_ON               /* Interrupts enabled. */
};

enum intr_level intr_get_level (void);
enum intr_level intr_set_level (enum intr_level);
enum intr_level intr_enable (void);
enum intr_level intr_disable (void);

/* Interrupt stack frame. */
/* 인터럽트 스택 프레임입니다. */
struct gp_registers {
	uint64_t r15; // 범용 레지스터 15
	uint64_t r14; // 범용 레지스터 14
	uint64_t r13; // 범용 레지스터 13
	uint64_t r12; // 범용 레지스터 12
	uint64_t r11; // 범용 레지스터 11
	uint64_t r10; // 범용 레지스터 10
	uint64_t r9;  // 범용 레지스터 9 ★여섯번째(마지막)
	uint64_t r8;  // 범용 레지스터 8 ★다섯번째
	uint64_t rsi; // 소스 인덱스(Source index) 레지스터 ★두번째
	uint64_t rdi; // 대상 인덱스(Destination index) 레지스터 ★첫번째
	uint64_t rbp; // 베이스 포인터(Base Pointer) 레지스터
	uint64_t rdx; // 데이터 레지스터 (D Data Register) ★세번째
	uint64_t rcx; // 카운터 레지스터 (Counter Register) ★네번째
	uint64_t rbx; // 베이스 레지스터(Base Register)
	uint64_t rax; // 누산기(Accumulator) 레지스터
} __attribute__((packed)); 
/* packed 속성 컴파일러의 의해 패딩이 추가되지 않고 
	구조체 맴버가 연속적으로 메모리에 배치*/ 

struct intr_frame {
	/* Pushed by intr_entry in intr-stubs.S.
	   These are the interrupted task's saved registers. */
	/* intr-stubs.S 파일의 intr_entry에 의해 푸시됩니다.
		이는 인터럽트된 작업의 저장된 레지스터입니다. */
	struct gp_registers R;
	uint16_t es; /* ES(segment) 레지스터 값을 저장하는 16비트 멤버 */
	uint16_t __pad1; /* 패딩으로 사용되는 멤버로, 구조체의 정렬을 위해 추가된 비사용 멤버 */
	uint32_t __pad2; /* 패딩으로 사용되는 32비트 멤버 */
	uint16_t ds; /* DS(segment) 레지스터 값을 저장하는 16비트 멤버 */
	uint16_t __pad3; /* 패딩으로 사용되는 멤버 */
	uint32_t __pad4; /* 패딩으로 사용되는 32비트 멤버 */
	/* Pushed by intrNN_stub in intr-stubs.S. */
	uint64_t vec_no; /* Interrupt vector number. */ /* 인터럽트 벡터 번호를 저장하는 64비트 멤버 */
/* Sometimes pushed by the CPU,
   otherwise for consistency pushed as 0 by intrNN_stub.
   The CPU puts it just under `eip', but we move it here. */
/* CPU에 의해 때때로 푸시되며,
그렇지 않으면 일관성을 위해 intrNN_stub에서 0으로 푸시됩니다.
CPU는 이것을 eip 아래에 놓지만, 우리는 여기로 이동시킵니다. */
	uint64_t error_code; /* 인터럽트 발생 시 오류 코드를 저장하는 64비트 멤버 */
/* Pushed by the CPU.
   These are the interrupted task's saved registers. */
/* CPU에 의해 푸시됩니다.
이는 중단된 작업의 저장된 레지스터입니다. */
	uintptr_t rip; /* 인터럽트가 발생한 명령어의 주소를 저장하는 uintptr_t(포인터 크기에 맞는 정수형) 멤버 */
	uint16_t cs; /* CS(segment) 레지스터 값을 저장하는 16비트 멤버 */
	uint16_t __pad5; /* 패딩으로 사용되는 멤버 */
	uint32_t __pad6; /* 패딩으로 사용되는 32비트 멤버 */
	uint64_t eflags; /* 인터럽트 발생 시 CPU 플래그 값을 저장하는 64비트 멤버 */
	/* rep 레지스터는 프로세스가 User Mode이면 User Mode 스택의 top을 가리키다가 
	Kernel Mode로 전환이 되면 Kernel Mode 스택의 top을 가리킵니다. */
	uintptr_t rsp; /* 인터럽트 발생 시 스택 포인터 값을 저장하는 uintptr_t 멤버 */ 
	uint16_t ss; /* SS(segment) 레지스터 값을 저장하는 16비트 멤버 */
	uint16_t __pad7; /* 패딩으로 사용되는 멤버 */
	uint32_t __pad8; /* 패딩으로 사용되는 32비트 멤버 */
} __attribute__((packed)); 

typedef void intr_handler_func (struct intr_frame *);

void intr_init (void);
void intr_register_ext (uint8_t vec, intr_handler_func *, const char *name);
void intr_register_int (uint8_t vec, int dpl, enum intr_level,
                        intr_handler_func *, const char *name);
bool intr_context (void);
void intr_yield_on_return (void);

void intr_dump_frame (const struct intr_frame *);
const char *intr_name (uint8_t vec);

#endif /* threads/interrupt.h */
