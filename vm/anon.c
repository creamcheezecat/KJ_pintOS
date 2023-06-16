/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
/* anon.c: 디스크 이미지가 아닌 페이지 (익명 페이지)의 구현입니다. */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
/* 익명 페이지의 데이터를 초기화하세요. */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	/* 할 일: swap_disk를 설정하세요. */
	swap_disk = NULL;

	
}

/* Initialize the file mapping */
/* 파일 매핑을 초기화하세요. */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	/* 핸들러를 설정하세요. */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;


}

/* Swap in the page by read contents from the swap disk. */
/* 스왑 디스크에서 내용을 읽어 페이지를 스왑 인하세요. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

}

/* Swap out the page by writing contents to the swap disk. */
/* 페이지의 내용을 스왑 디스크에 쓰고 페이지를 스왑 아웃하세요. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
/* 익명 페이지를 파괴하세요. 페이지는 호출자에 의해 해제될 것입니다. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

}
