#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  buffer_close();
  
  // char buffer[512];
  // disk_read(filesys_disk, ROOT_DIR_SECTOR, buffer);
  // hex_dump (0, buffer, 512, true);
  // free(buffer);
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  disk_sector_t inode_sector = 0;
  // struct dir *dir = dir_open_root ();
  // struct dir *dir = dir_open(inode_open(thread_current()->dir_sector));
  char *last_name = NULL;
  struct dir *cur_dir = dir_find(name, true, &last_name);

  bool success = (cur_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir,
                   inode_get_inumber(dir_get_inode(cur_dir)))
                  && dir_add (cur_dir, last_name, inode_sector));
  // printf("creating %p, %d, %s@%d\n", cur_dir, inode_get_inumber(dir_get_inode(cur_dir)),
  // last_name, inode_sector);
  // char buffer[512];
  // disk_read(filesys_disk, ROOT_DIR_SECTOR, buffer);
  // hex_dump (0, buffer, 512, true);
  // free(buffer);


  
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  if (cur_dir != NULL)
  {
    dir_close(cur_dir);
    free(last_name);
  }

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  // struct dir *dir = dir_open_root ();
  // struct dir *dir = dir_open(inode_open(thread_current()->dir_sector));
  char *last_name = NULL;
  struct dir *cur_dir = dir_find(name, true, &last_name);
  struct inode *inode = NULL;
  // printf("opening %s, %p@%d, %s\n", name, cur_dir,
  //        inode_get_inumber(dir_get_inode(cur_dir)), last_name);
  // char buffer[512];
  // disk_read(filesys_disk, ROOT_DIR_SECTOR, buffer);
  // hex_dump (0, buffer, 512, true);
  if (cur_dir == NULL)
    return NULL;

  if (dir_check_root(cur_dir) && last_name == NULL)
    return file_open(dir_get_inode(cur_dir));

  if (strcmp(last_name, ".") == NULL)
    return file_open(dir_get_inode(cur_dir));

  if (last_name == NULL)
    dir_lookup(cur_dir, "", &inode);
  else
    dir_lookup(cur_dir, last_name, &inode);

  dir_close(cur_dir);

  if (last_name != NULL)
    free(last_name);

  // printf("result %p\n", inode);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  // struct dir *dir = dir_open_root ();
  // struct dir *dir = dir_open(inode_open(thread_current()->dir_sector));
  char *last_name = NULL;
  struct dir *cur_dir = dir_find(name, true, &last_name);
  // printf("removing %s, %p, %s\n", name, cur_dir, last_name);
  if (dir_check_root(cur_dir) && last_name == NULL) {
    dir_close(cur_dir);
    return false;
  }
  

  bool success = cur_dir != NULL && dir_remove (cur_dir, last_name);
  if (cur_dir != NULL)
  {
    dir_close(cur_dir);
    free(last_name);
  }


  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool filesys_chdir(char *dir)
{
  struct dir *target_dir = dir_find(dir, false, NULL);
  // printf("target dir %s, %p, %d\n", dir,target_dir, inode_get_inumber(dir_get_inode(target_dir)));
  if (target_dir == NULL)
  {
    return false;
  }
  else
  {
    thread_current()->dir_sector = inode_get_inumber(dir_get_inode(target_dir));
  }
  return true;
}

bool filesys_readdir(struct file *file, char *name)
{
  // struct dir *target_dir = dir_find(dir, false, NULL);
  struct dir *target_dir = (struct dir*) file;

  if (target_dir == NULL)
  {
    return false;
  }
  bool returnVal = dir_readdir(target_dir, name);
  // dir_close(target_dir);

  return returnVal;
}

int filesys_inumber(struct file *file)
{
  // struct dir *target_dir = dir_find(dir, false, NULL);
  struct dir *target_dir = (struct dir*) file;

  if (target_dir == NULL)
  {
    return -1;
  }

  int returnVal = inode_get_inumber(dir_get_inode(target_dir));

  return returnVal;
}