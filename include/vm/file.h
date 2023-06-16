#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	
	struct file *file;					/* 가상 주소와 맵핑된 파일 */

	off_t offset;						/* 읽어야 할 파일 오프셋 */
	size_t read_bytes;					/* 가상 페이지에 쓰여져 있는 데이터 크기 */
	size_t zero_bytes;					/* 0으로 채울 남은 페이지의 바이트 */

	size_t swap_slot;					/* ????? */
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
