/* file.c: Implementation of memory backed file object (mmaped object). */
/* file.c: 메모리 지원 파일 객체 (mmap된 객체)의 구현입니다. */
#include "vm/vm.h"

#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

static bool load_file (struct page *page, void *aux);

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
	struct file_page *file_page = &page->file;

	// file_page 의 변수들을 입력된 file_page 로 초기화 해준다.
/* 	struct file_page *aux_file = (struct file_page *)page->uninit.aux;
	file_page->file = aux_file->file;
	file_page->offset = aux_file->offset;
	file_page->read_bytes = aux_file->read_bytes;
	file_page->zero_bytes = aux_file->zero_bytes; */
	
	return true;
}

/* Swap in the page by read contents from the file. */
/* 파일에서 내용을 읽어 페이지를 스왑인합니다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	size_t page_read_bytes = file_page->read_bytes;
    size_t page_zero_bytes = file_page->zero_bytes;

	// 파일의 내용을 페이지에 입력한다
	if(file_read_at(file_page->file, kva, page_read_bytes, file_page->offset) != (int)page_read_bytes){
		// 제대로 입력이 안되면  false 반환
		return false;
	}
	// 나머지 부분을 0으로 입력
	memset(kva + page_read_bytes, 0, page_zero_bytes);
	return true;
}
/* Swap out the page by writeback contents to the file. */
/* 페이지의 내용을 파일로 기록하여 페이지를 스왑아웃합니다. */
/*내용을 다시 파일에 기록하여 페이지를 교체합니다. 
페이지가 더러운지 먼저 확인하는 것이 좋습니다. 
더럽지 않으면 파일의 내용을 수정할 필요가 없습니다. 
페이지를 교체한 후에는 페이지의 더티 비트를 꺼야 합니다.*/
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	/* PML4의 가상 페이지 VPAGE에 대한 PTE가 변경되었는지 확인하여, 
	변경되었다면 true를 반환합니다.
	만약 PML4에 VPAGE에 대한 PTE가 없다면 false를 반환합니다. */
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		if(file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset) <= 0){
			//PANIC("File_Write_Anything !!!!! in file_backed_swap_out");
		}
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}
	/* 사용자 가상 페이지 UPAGE를 페이지 디렉토리 PD에서 "프레젠트되지 않음"으로 표시합니다.
	이후에 페이지에 접근하면 페이지 폴트가 발생합니다.
	페이지 테이블 항목의 다른 비트는 보존됩니다.
	UPAGE는 매핑되어 있지 않아도 됩니다. */
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* 파일 백드 페이지를 파괴합니다. 페이지는 호출자에 의해 해제됩니다. */
static void
file_backed_destroy (struct page *page) {
	
	// page struct를 해제할 필요는 없습니다. (file_backed_destroy의 호출자가 해야 함)
	struct file_page *file_page UNUSED = &page->file;
	if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		if(file_write_at(file_page->file, page->va, 
				file_page->read_bytes, file_page->offset) <= 0){
				//PANIC("File_Write_Anything !!!!! in file_becked_destory");
		}
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
	// 파일의 정확한 상태와 위치를 알기 위해 file_reopen() 을 해준다.
	struct file *fr = file_reopen(file);
	void *map_addr = addr;
	
	size_t read_bytes = file_length(fr) < length ? file_length(fr) : length;
	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);

	while (read_bytes > 0) {

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_page *fp = (struct file_page *)malloc(sizeof(struct file_page));

		fp->file = fr;
		fp->offset = offset;
		fp->read_bytes = page_read_bytes;
		fp->zero_bytes = page_zero_bytes;
		
		if(!vm_alloc_page_with_initializer(VM_FILE,addr,writable,load_file, fp)){
			file_close(fr);
			return NULL;
		}
		/* 
		addr 부터 연속 가상 페이지가 만들어지니까 최초 페이지에 페이지들의 갯수만 알면
		확인 할 수 있다.
		*/
		struct page *mapped_page = spt_find_page(&thread_current()->spt, addr);
		mapped_page->mapped_page_count = length <= PGSIZE ? 1 : length % PGSIZE ? length / PGSIZE + 1 : length / PGSIZE;

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

	if(unmap_page->frame == NULL){
		vm_claim_page(unmap_page);
	}

	struct file *file = unmap_page->file.file;
	int page_count = unmap_page->mapped_page_count;

	for (int i = 0 ; i < page_count ; i++){
		if(unmap_page){
			destroy(unmap_page);
		}
		addr += PGSIZE;
		unmap_page = spt_find_page(spt, addr);
	}
	file_close(file);
}

static bool
load_file (struct page *page, void *aux) {
	ASSERT(page->frame != NULL);
    ASSERT(aux != NULL);

	struct file_page *fp = (struct file_page *)aux;

	page->file = (struct file_page){
        .file = fp->file,
        .offset = fp->offset,
        .read_bytes = fp->read_bytes,
        .zero_bytes = fp->zero_bytes
    };

	void *kpage = page->frame->kva;

	if(file_read_at(fp->file, kpage, fp->read_bytes, fp->offset) != (int)(fp->read_bytes)){
		palloc_free_page(kpage);
		return false;
	}

	memset(kpage + fp->read_bytes, 0, fp->zero_bytes);
	free(fp);
	return true;
}
