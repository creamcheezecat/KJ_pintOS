/* file.c: Implementation of memory backed file object (mmaped object). */
/* file.c: 메모리 지원 파일 객체 (mmap된 객체)의 구현입니다. */
#include "vm/vm.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
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
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
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
	//ASSERT (length % PGSIZE == 0);
	if(writable == 0){
		return NULL;
	}

	if(file_read_at(file,addr,length,offset) != length){
		return NULL;
	}

	
	return ;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	/*
	지정된 주소 범위에 대한 매핑을 매핑 해제합니다 addr. 
	이 주소 범위는 아직 매핑 해제되지 않은 
	동일한 프로세스에서 mmap에 대한 이전 호출에서 반환된 가상 주소여야 합니다.
	*/
	
}
