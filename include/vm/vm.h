#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "include/lib/kernel/hash.h"
#include "include/lib/string.h"

enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	/* 파일과 관련되지 않은 페이지, 즉 익명 페이지 */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	/*저장 정보를 위한 보조 비트 플래그 마커입니다. 
	더 많은 마커를 추가할 수 있으며, 값이 int에 맞을 때까지 추가할 수 있습니다.*/
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
/* "페이지"의 표현입니다.
이는 "부모 클래스"로, uninit_page, file_page, anon_page 및 page cache (project4)라는
네 개의 "자식 클래스"를 가지고 있습니다.
이 구조체의 미리 정의된 멤버를 제거하거나 수정하지 마십시오. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */
	
	/* Your implementation */
	struct hash_elem elem;

	bool writable;
	int mapped_page_count;
	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	/* 각 유형의 데이터가 union에 바인딩됩니다.
	각 함수는 자동으로 현재 union을 감지합니다. */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation(표현) of "frame" */
struct frame {
	void *kva;					/*kernel virtual address*/
	struct page *page;

	struct list_elem elem;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* 페이지 작업을 위한 함수 테이블.
이는 C에서 "인터페이스"를 구현하는 하나의 방법입니다.
"메서드"의 테이블을 구조체의 멤버로 넣고,
필요할 때마다 호출합니다. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *);
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)


/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* 현재 프로세스의 메모리 공간 표현입니다.
이 struct에 대해 특정한 디자인을 강제할 필요는 없습니다.
이에 대한 모든 디자인은 여러분에게 달려 있습니다. */
//보조 페이지 테이블
/*
각 페이지마다 보조 데이터를 추적하는 프로세스별 데이터 구조입니다. 
데이터의 위치 (프레임/디스크/스왑), 
해당하는 커널 가상 주소에 대한 포인터, 
활성 상태 vs. 비활성 상태 등을 추적합니다.
*/
struct supplemental_page_table {
	struct hash pages;
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init ();
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

void vm_free_frame(struct frame *frame);

#endif  /* VM_VM_H */
