/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"

static struct list frame_list;
static struct lock frame_lock;

static uint64_t page_hash_func(const struct hash_elem *,void *);
static bool page_less_func(const struct hash_elem *, const struct hash_elem *,void *);
static void hash_destroy_func (struct hash_elem *, void *);
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
	list_init(&frame_list); // 프레임을 관리할 list (전역 선언 되어있음)
	lock_init(&frame_lock); // 프레임 동기화를 위한 lock (전역 선언 되어있음)
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

	/* upage가 이미 사용 중인지 확인합니다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화 프로그램을 가져옵니다.
		TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */
		/* TODO: 페이지를 spt에 삽입합니다. */
		// 새로운 페이지를 생성(할당)
		struct page *newpage = (struct page *)malloc(sizeof(struct page));
		// 다른 initilaizer 를 넣어주기 위한 틀 만들기 
		bool (*page_init)(struct page *, enum vm_type, void *);
		// 타입에 따른 각자 다른 initilaizer 넣어주기
		switch(VM_TYPE(type)){
			case VM_ANON:
				page_init = anon_initializer;
				break;
			case VM_FILE:
				page_init = file_backed_initializer;
				break;
		}
		/* 실제로 쓰기 전까지는 uninit 페이지로 있어야하니까 
			uninit 페이지로 만들기 */
		uninit_new(newpage,upage,init,type,aux,page_init);
		// 페이지 속성에 맞게 수정
		newpage->writable = writable;
		// 보조 페이지 테이블에 페이지 입력
	 	return spt_insert_page(spt,newpage);
	}
	
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */

	struct hash_elem *e;
	struct page tp;
	// 새로운 페이지에 찾을 페이지 주소로 변경
	tp.va = pg_round_down(va);
	lock_acquire(&spt->page_lock);
	// 보조 페이지 테이블에 찾는 페이지가
	// 있다면 hash_elem 값 반환 없다면 NULL 반환
	e = hash_find(&spt->pages,&tp.elem);

	if(e != NULL){
		//hash_elem 값이 있다면 페이지로 만들어서 전달
		page = hash_entry(e ,struct page, elem);
	}
	lock_release(&spt->page_lock);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	// spt 안에 페이지가 이미 있는지 확인
	if(spt_find_page(spt,page->va) != NULL){
		return succ;
	}
	// spt 에 페이지를 입력한다.
	lock_acquire(&spt->page_lock);
	if(hash_insert(&spt->pages,&page->elem)==NULL){
		succ = true;
	}
	lock_release(&spt->page_lock);
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if(page != NULL){
		vm_dealloc_page (page);
	}
}

/* Get the struct frame, that will be evicted. */
/* 죽을 때 제거될 구조체 프레임을 가져옵니다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	/* TODO: 제거 정책은 여러분에게 달려 있습니다. */
	/* victim = list_entry(list_pop_front(&frame_list),struct frame, elem); */
	struct list_elem *e;
	// 프레임 교체시 동기화를 하기위한 락
	lock_acquire(&frame_lock);
	// 프레임이 담겨있는 리스트에 처음부터 마지막 까지 비교
	for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)){
		// 하나의 페이지를 꺼낸다.
		victim = list_entry(e,struct frame,elem);
		uint64_t *pml4 = thread_current()->pml4;

		if(victim->page == NULL){
			break;
		}
		/* 
		그 페이지에 접근했는지 확인한다. 
		PTE가 설치된 시간과 마지막으로 지워진 시간 사이에 접근된 경우			
		(최근에 접근한 경우 True, pte가 없는 경우 false)
			*/
		else if(pml4_is_accessed(pml4,victim->page->va)){
				// 최근에 접근 했다면 false 로 바꾸고 다음 페이지로 변경
				pml4_set_accessed(pml4,victim->page->va,0);
		}else{
				// false 이 뜨면 희생 페이지로 함수를 반환한다.
			break;
		}
	}
	lock_release(&frame_lock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* 페이지를 하나 대체하고 해당하는 프레임을 반환합니다.
오류 발생 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame (void) {
	// 희생 페이지가 정해지면
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	/* TODO: 희생자를 교체하고 제거된 프레임을 반환합니다. */
	if (victim->page != NULL){
		// 어디로 가는걸까?
		/*
		anon or file 타입의 페이지가 물리 메모리를 할당받고 있으니 
		둘의 swap out에 가는게 아닐까?
		*/		
        swap_out(victim->page);
	}
	//깨끗한 페이지를 반환해준다.
	return victim;
	
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
	// 페이지 하나를 할당한다.
	void *kva = palloc_get_page(PAL_USER | PAL_ZERO);
	// 만약 페이지가 할당이 안됬다면 페이지가 꽉찼다는 말과 같으니까
	if(kva == NULL){
		// 프레임 중에 하나 선택해서 페이지 맵핑을 초기화 하고 그 프레임을 반환한다. 
		frame = vm_evict_frame();
		memset(frame->kva, 0, PGSIZE);
		frame->page = NULL;
		return frame;
	}
	//프레임 페이지 할당이 되었다면 프레임을 할당한다.
	frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = kva;
	frame->page = NULL;
	//프레임들을 관리하기위에 리스트에넣는다
	lock_acquire(&frame_lock);
	list_push_back(&frame_list,&frame->elem);
	lock_release(&frame_lock);

	ASSERT(frame != NULL);
  	ASSERT(frame->kva != NULL);
 	ASSERT(frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	vm_alloc_page(VM_ANON | VM_MARKER_0,pg_round_down(addr),true);
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
	enum vm_type vmtype;
    bool stack_growth = false;
	bool success = false;
	// 유효성 검사
	/* addr = Fault address. */
	/* not_present == True: not-present page, false: writing r/o page. */
	if(addr == NULL || is_kernel_vaddr(addr) || !not_present){
		return success;
	}
	
	void *rsp = f->rsp;
	/* user == True: access by user, false: access by kernel. */
	if(!user){
		rsp = thread_current()->rsp;
	}

	/* 스택 확장으로 처리해야하는 폴트일 경우*/
	/* pintos 스택 크기를 최대 1MB 로 제한되어야 한다.*/
	if ((USER_STACK - (1 << 20) <= rsp - 8 && //  반환 주소가 스택 크기 안에 있는가?
		rsp - 8 == addr && // 폴트 주소가 현재 스택 프레임의 반환 주소인가?
		addr <= USER_STACK) || // 폴트 주소가 유저 스택 안에 있는가?
		// 스택 포인터 가 폴트 주소와 유저 스택 사이에 있는가?
		(USER_STACK - (1 << 20) <= rsp && rsp <= addr &&
		addr <= USER_STACK)) // 폴트 주소가 유저 스택 안에 있는가?
	{
		vm_stack_growth(addr);
	}
	
	page = spt_find_page(spt,addr);
	
	if(page == NULL){
		return success;
	}
	// write 불가능 페이지인데 요청 한 경우
	if(write == true && page->writable == false){
		return success;
	}
	
	success = vm_do_claim_page(page);
	
	return success;
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

	if(page == NULL){
		return false;
	}

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
	//pml4_set_page (t->pml4, page->va, frame->kva, page->writable);
	if(pml4_get_page (t->pml4, page->va) == NULL){//물리 주소가 비어있다.
		if(!pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
			// 할당하지 못했다면
			return false;
		}
	}

	return swap_in (page, frame->kva); // uninit_initialize
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->pages,page_hash_func,page_less_func,NULL);
	lock_init(&spt->page_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// TODO: 보조 페이지 테이블을 src에서 dst로 복사합니다.
	// TODO: src의 각 페이지를 순회하고 dst에 해당 entry의 사본을 만듭니다.
	// TODO: uninit page를 할당하고 그것을 즉시 claim해야 합니다.
    struct hash_iterator i;
	lock_acquire(&src->page_lock);
	hash_first(&i, &src->pages);
	while(hash_next(&i)){
		// src_page 정보
		struct page *src_page = hash_entry(hash_cur(&i), struct page, elem);
		struct page *dst_page = NULL;
		struct file_page *file_aux = NULL;
		enum vm_type type = src_page->operations->type;
		void *aux = NULL;
		
		switch(VM_TYPE(type)){
			case VM_UNINIT:
				//uninit 타입은 본래 타입으로 할당해야한다? 
				aux = src_page->uninit.aux;
				type = page_get_type(src_page);
				struct file_page *fp = NULL;
			
				if(aux != NULL){
					struct file_page *fd = (struct file_page *)aux;
					fp = (struct file_page *)malloc(sizeof(struct file_page));
					if(VM_TYPE(type) == VM_FILE){
						fp->file = file_reopen(fd->file);
					}else{
						fp->file = fd->file;
					}
					fp->offset = fd->offset;
					fp->read_bytes = fd->read_bytes;
					fp->zero_bytes = fd->zero_bytes;
				}

				vm_alloc_page_with_initializer(type,
						src_page->va,src_page->writable,src_page->uninit.init,fp);
				break;
			case VM_ANON:
				if(!vm_alloc_page(type,src_page->va,src_page->writable)){
					lock_release(&src->page_lock);
					return false;
				}
				
				if(!vm_claim_page(src_page->va)){
					lock_release(&src->page_lock);
					return false;
				}

				// 매핑된 프레임에 내용 로딩
				dst_page = spt_find_page(dst,src_page->va);
				memcpy(dst_page->frame->kva,src_page->frame->kva,PGSIZE);
				break;
			case VM_FILE:
				break;
		}
	}
	lock_release(&src->page_lock);
    return true;
}

/* Free the resource hold by the supplemental page table */
/* 보충 페이지 테이블에 의해 보유된 리소스를 해제합니다. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* TODO: 스레드가 보유한 모든 보조 페이지 테이블을 파괴하고
	TODO: 수정된 내용을 저장소에 씁니다. */
	//hash_destroy(&spt->pages,hash_destroy_func);
	hash_clear(&spt->pages,hash_destroy_func);
	/*
	hash_destroy가 아닌 hash_clear를 사용해야 하는 이유
	여기서 hash_destroy 함수를 사용하면 hash가 사용하던 메모리(hash->bucket) 자체도 반환한다.
	process가 실행될 때 hash table을 생성한 이후에 process_clean()이 호출되는데,
	이때는 hash table은 남겨두고 안의 요소들만 제거되어야 한다.
	따라서, hash의 요소들만 제거하는 hash_clear를 사용해야 한다.
	*/
}
/*================================================*/

void 
vm_free_frame(struct frame *frame)
{
    lock_acquire(&frame_lock);
    list_remove(&frame->elem);
    lock_release(&frame_lock);
    free(frame);
}

static uint64_t
page_hash_func(const struct hash_elem *e,void *aux UNUSED){
/* 	struct page *page = hash_entry(e,struct page, elem);
	return hash_bytes(&page->va,sizeof(page->va)); */
	int *vaddr_int = (int *)hash_entry(e,struct page, elem)->va;
	return hash_int(vaddr_int);
}
//hash_bytes(&hash_entry(e,struct page, elem)->va,sizeof(hash_entry(e,struct page, elem)->va));

static bool
page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	return hash_entry(a,struct page, elem)->va < hash_entry(b,struct page, elem)->va;
}

static void 
hash_destroy_func (struct hash_elem *e, void *aux){
	vm_dealloc_page(hash_entry(e,struct page,elem));
}

/*================================================*/