/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"


static struct list frame_list;


static unsigned page_hash_func(const struct hash_elem *,void *);
static bool page_less_func(const struct hash_elem *, const struct hash_elem *);
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_list);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
/* 페이지의 유형을 가져옵니다.
이 함수는 페이지가 초기화된 후의 유형을 알고 싶을 때 유용합니다.
이 함수는 현재 완전히 구현되어 있습니다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 초기화 프로그램과 함께 대기 중인 페이지 객체를 생성합니다.
페이지를 생성하려면 직접 생성하지 말고 
이 함수나 vm_alloc_page를 통해 생성하세요. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	/* upage가 이미 사용 중인지 확인합니다. */
	if (spt_find_page (spt, upage) == NULL) {// 페이지 생성????
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		/* TODO: Insert the page into the spt. */
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 프로그램을 가져옵니다.
		TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */
		/* TODO: 페이지를 spt에 삽입합니다. */

		struct page *newpage = (struct page *)malloc(sizeof(struct page));

		bool (*page_init)(struct page *, enum vm_type, void *) = type == VM_ANON 
		? anon_initializer : file_backed_initializer;

		uninit_new(newpage,upage,init,type,aux,page_init);
		newpage->writable = writable;

	 	spt_insert_page(spt,newpage);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	struct hash_iterator i;

	hash_first(&i, &spt->pages);
	while (hash_next(&i)){
		struct page *target_page = hash_entry(hash_cur(&i), struct page, elem);

		if(target_page->va == va){
			page = target_page;
			break;
		}
	}

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (spt == NULL || page == NULL) {
        return succ;
    }

	if(hash_insert(&spt->pages,&page->elem) != NULL){
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* 페이지를 하나 대체하고 해당하는 프레임을 반환합니다.
오류 발생 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc() 및 프레임 가져오기. 
사용 가능한 페이지가 없으면 페이지를 대체하고 해당 페이지를 반환합니다. 
이 함수는 항상 유효한 주소를 반환합니다. 
즉, 사용자 풀 메모리가 가득 차있는 경우에도 
이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 대체합니다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame->kva  = palloc_get_page(PAL_ZERO);
	
	list_push_back(&frame_list,&frame->elem);

	if(frame->kva == NULL){
		// 페이지 대체??
	}
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	list_push_back(&frame_list,&frame->elem);
	


	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {

}

/* Handle the fault on write_protected page */
/* 쓰기 보호된 페이지에서 발생한 오류를 처리합니다. */
static bool
vm_handle_wp (struct page *page UNUSED) {

}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
			
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: 페이지 폴트를 유효성 검사합니다. */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
/* 페이지를 해제합니다.
이 함수를 수정하지 마십시오. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* VA에 할당된 페이지를 차지합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

	page = spt_find_page(&thread_current()->spt,va);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* 페이지를 차지하고 MMU를 설정합니다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* TODO: 페이지 테이블 엔트리를 삽입하여 
	페이지의 가상 주소(VA)를 프레임의 물리 주소(PA)와 매핑합니다. */
	struct thread *t = thread_current();

	if(pml4_get_page (t->pml4, page->va) == NULL){//물리 주소가 비어있다.
		if(!pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
			// 할당하지 못했다면
			return false;
		}
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->pages,page_hash_func,page_less_func,NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	
}

/* Free the resource hold by the supplemental page table */
/* 보충 페이지 테이블에 의해 보유된 리소스를 해제합니다. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* TODO: 스레드가 보유한 모든 보조 페이지 테이블을 파괴하고
	TODO: 수정된 내용을 저장소에 씁니다. */
	

}
/*================================================*/

static unsigned
page_hash_func(const struct hash_elem *e,void *aux UNUSED){
	int vaddr_int = (int)hash_entry(e,struct page, elem)->va;
	return hash_int(vaddr_int);
}

static bool
page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	return hash_entry(a,struct page, elem)->va < hash_entry(b,struct page, elem)->va;
}

/*================================================*/