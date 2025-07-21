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
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "include/userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
struct lock filesys_lock; // 파일 읽기 쓰기 lock

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
		
	lock_init(&filesys_lock);
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

int wait(pid_t child_tid)
{
	return process_wait(child_tid);
}

void check_address(void *address)
{	
	// 커널영역이거나, address가 null이거나, 가상페이지에 할당되었는지
	if (is_kernel_vaddr(address) || address == NULL || pml4_get_page(thread_current()->pml4, address))
		exit(-1);
}

bool create (const char *file, unsigned initial_size)
{
	check_address(file);

	// 주어진 이름과 초기 크기로 새로운 파일 생성하는 함수
	return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
	check_address(file);

	// 주어진 이름의 파일 삭제하는 함수
	return filesys_remove(file);
}

int open (const char *file)
{
	check_address(file);

	struct file *open_file = filesys_open(file);

	if (open_file == NULL)
		return -1;

	int fd = process_add_file(open_file);

	if (fd == -1)
	{
		file_close(open_file);
		return -1;
	}
	
	return fd;
}

int filesize (int fd)
{
	struct file *file = process_get_file(fd);
	
	if (file == NULL || fd < 3)
		return;

	return file_length(file);
}

int read (int fd, void *buffer, unsigned length)
{
	int read_bytes = 0;

	if (fd == 0) // 파일 디스크립터가 0이면
	{
		char c;
		unsigned char *buf = buffer;

		for (int i = 0; i < length; i++) // 길이만큼 반복하고
		{
			c = input_getc(); // input_getc() = 키보드 하나하나 입력이 가능
			*buf++ = c; // c를 buf에 저장하고 포인터 늘리기
			read_bytes++; // read_bytes 증가
		}
		
		return read_bytes; 
	}
	else if (fd == 1 || fd == 2) // 파일 디스크립터 1, 2 -> 표준 출력, 표준 에러는 읽을 수 없으므로
	{
		return -1; 
	}
	else // 3 이상 이라면
	{
		struct file *file = process_get_file(fd); 

		if (file == NULL)
			return -1;

		lock_acquire(&filesys_lock); // race condition 방지를 위해 잠금
		read_bytes = file_read(file, buffer, length);
		lock_release(&filesys_lock); // 해제
		return read_bytes;
	}
}

int write (int fd, const void *buffer, unsigned length)
{
	if (fd <= 0)
	{
		return -1;
	}
	else if (fd == 1 || fd == 2)
	{
		putbuf(buffer, length);
		return length;
	}
	else
	{
		struct file *file = process_get_file(fd);
		int write_bytes = -1;

		if (file == NULL)
			return -1;
		
		lock_acquire(&filesys_lock); // race condition 방지 락
		write_bytes = file_write(file, buffer, length);
		lock_release(&filesys_lock); // 락 해제

		return write_bytes;
	}
}

void seek (int fd, unsigned position)
{
	struct file *file = process_get_file(fd);

	if (file == NULL || fd < 3)
		return;
	
	file_seek(file, position);
}

unsigned tell (int fd)
{
	struct file *file = process_get_file(fd);

	if (file == NULL || fd < 3)
		return;

	return file_tell(file);
}

void close (int fd)
{
	struct file *file = process_get_file(fd);

	if (file == NULL || fd < 3)
		return;

	process_close_file(fd);	
	file_close(file);
}