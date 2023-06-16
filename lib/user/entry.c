#include <syscall.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);
// 사용자 프로그램의 진입점
void
_start (int argc, char *argv[]) {
	exit (main (argc, argv));
}
