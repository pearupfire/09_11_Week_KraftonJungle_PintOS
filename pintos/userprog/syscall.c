#include "userprog/syscall.h"
#include "lib/user/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler (struct intr_frame *f UNUSED) 
{
	int sys_num = f->R.rax;

	switch (sys_num)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi);
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	default:
		exit(-1);
		break;
	}
}

void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread *cur = thread_current(); // 현재 스레드 저장
	cur->exit_status = status; // 종료 상태 저장
	printf("%s: exit(%d)\n", cur->name, cur->exit_status); // 디버깅
	thread_exit(); // 현재 스레드 종료
}

pid_t fork(const char *thread_name)
{

}

int exec(const char *file)
{

}

int wait(pid_t)
{

}

bool create (const char *file, unsigned initial_size)
{
	// 파일이 널인지 확인
	if (file == NULL)
		exit(-1);

	// 주어진 이름과 초기 크기로 새로운 파일 생성하는 함수
	return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
	// 파일이 널인지 확인
	if (file == NULL)
		exit(-1);

	// 주어진 이름의 파일 삭제하는 함수
	return filesys_remove(file);
}

int open (const char *file)
{

}

int filesize (int fd)
{

}

int read (int fd, void *buffer, unsigned length)
{
	
}

int write (int fd, const void *buffer, unsigned length)
{

}

void seek (int fd, unsigned position)
{

}

unsigned tell (int fd)
{

}

void close (int fd)
{

}