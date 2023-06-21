/* file.c: Implementation of memory backed file object (mmaped object). */
/* file.c: 메모리 지원 파일 객체 (mmap된 객체)의 구현입니다. */
#include "vm/vm.h"

#include "include/threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

static bool lazy_load_segment (struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {

}

/* Initialize the file backed page */
/* 파일을 기반으로 한 페이지를 초기화합니다. */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// 먼저 page->operations에 file-backed pages에 대한 핸들러를 설정합니다.
	page->operations = &file_ops;
	//pritnf("file_backed_initializer 마지막 이네\n");
	struct file_page *file_page = &page->file;

	struct file_page *aux_file = (struct file_page *)page->uninit.aux;
	file_page->file = aux_file->file;
	file_page->offset = aux_file->offset;
	file_page->read_bytes = aux_file->read_bytes;
	file_page->zero_bytes = aux_file->zero_bytes;
	
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* 파일 백드 페이지를 파괴합니다. 페이지는 호출자에 의해 해제됩니다. */
static void
file_backed_destroy (struct page *page) {
	
	// page struct를 해제할 필요는 없습니다. (file_backed_destroy의 호출자가 해야 함)
	//printf("file_backed_destroy 오는거 맞아 ?");
	struct file_page *file_page UNUSED = &page->file;

	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	pml4_clear_page(thread_current()->pml4, page->va);

}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	/*
	offset 에서부터 length 만큼을 파일에서 꺼내서 가상 주소 공간에 매핑한다.
	전체 파일은 addr에서 시작하는 연속 가상 페이지에 매핑된다
	파일 길이가 PGSIZE의 배수가 아닌 경우 최종 매핑된 페이지의 일부 바이트가 돌출 된다
	페이지에 오류가 발생하면 0바이트로 설정하고 페이지를 디스크에 다시 쓸때 버린다
	성공하면 파일에 매핑된 가상 주소를 반환
	실패하면 NULL 반환
	*/
	struct file *fr = file_reopen(file);
	void *map_addr = addr;
	
	size_t read_bytes = file_length(fr) < length ? file_length(fr) : length;
	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);

	while (read_bytes > 0 || zero_bytes > 0) {

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_page *fp = (struct file_page *)malloc(sizeof(struct file_page));

		fp->file = file;
		fp->offset = offset;
		fp->read_bytes = page_read_bytes;
		fp->zero_bytes = page_zero_bytes;
		
		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,lazy_load_segment, fp)){
			return NULL;
		}

		struct page *mapped_page = spt_find_page(&thread_current()->spt, addr);
		mapped_page->mapped_page_count = length <= PGSIZE ? 1 : length % PGSIZE ? length / PGSIZE + 1 : length / PGSIZE;
		/* if(length <= PGSIZE){
			mapped_page->mapped_page_count = 1;
		}else{
			if(length % PGSIZE != 0){
				mapped_page->mapped_page_count = (int)length / PGSIZE + 1;
			}else{
				mapped_page->mapped_page_count = (int)length / PGSIZE;
			}
		} */

		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}
	
	return map_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	/*
	지정된 주소 범위에 대한 매핑을 매핑 해제합니다 addr. 
	이 주소 범위는 아직 매핑 해제되지 않은 
	동일한 프로세스에서 mmap에 대한 이전 호출에서 반환된 가상 주소여야 합니다.
	*/
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *unmap_page =  spt_find_page(spt, addr);
	//printf("진입한거 확실한거지??\n");
	int page_count = unmap_page->mapped_page_count;
	for (int i = 0 ; i < page_count ; i++){
		//printf("여기몇번오나??\n");
		if(unmap_page){
			//printf("unmap_page 삭제\n");
			destroy(unmap_page);
			spt_remove_page(spt, unmap_page);
		}
		addr += PGSIZE;
		unmap_page = spt_find_page(spt, addr);
	}
}

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

	free(aux);

	if(file_read_at(file, kpage, page_read_bytes, offset) != (int)page_read_bytes){
		palloc_free_page(page->frame->kva);
		return false;
	}

	memset(kpage + page_read_bytes, 0, page_zero_bytes);

	return true;
}
