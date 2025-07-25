#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */

/// @brief 주어진 이름과 초기 크기로 새로운 파일 생성하는 함수
/// @param name 생성할 파일의 이름
/// @param initial_size 생성할 파일의 초기 크기
/// @return 성공 시 true, 실패 시 false
bool filesys_create (const char *name, off_t initial_size)
{
	// 해당 받은 inode 섹터 번호 저장할 변수
	disk_sector_t inode_sector = 0;

	// 루트 디렉토리 열기
	struct dir *dir = dir_open_root ();

	// 디렉토리 유효, 섹터맴에 섹터 할당에 성공, inode 생성, 디렉토리 파일 이름과 inode 섹터를 추가한느 작업이 성공하면 true
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	
	// 디렉토리 닫기
	dir_close (dir);

	// 성공 여부 반환
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode);
	dir_close (dir);

	return file_open (inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */

/// @brief 주어진 이름의 파일 삭제하는 함수
/// @param name 삭제할 파일의 이름
/// @return 성공 시 true, 실패 시 false
bool filesys_remove(const char *name)
{
	// 루트 디렉토리 열기
	struct dir *dir = dir_open_root ();

	// dir 이 유효하고 해당 디렉토라에서 파일 삭제에 성공했는지 여부 저장
	bool success = dir != NULL && dir_remove (dir, name);

	// 연 디렉토리 닫기
	dir_close (dir);

	// 성공 여부 반환
	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}
