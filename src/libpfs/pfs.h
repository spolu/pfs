#ifndef _PFS_H
#define _PFS_H

#include <sys/stat.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#define PFS_NAME_LEN 255
#define PFS_ID_LEN 32

struct pfs_instance;
struct pfs_updt;

/** File Open operaion.
*/
int pfs_open (struct pfs_instance * pfs,
	      const char * path,
	      int flags);

/** Write data to an open file.
 */
ssize_t pfs_pwrite (struct pfs_instance * pfs,
		    int pfs_fd,
		    const void * buf,
		    size_t len,
		    off_t offset);

/** Read data from an open file.
 */
ssize_t pfs_pread (struct pfs_instance * pfs,
		   int pfs_fd,
		   void * buf,
		   size_t len,
		   off_t offset);

/** Fsync file.
 */
int pfs_fsync (struct pfs_instance * pfs,
	       int pfs_fd);

/** Close an open file.
 *
 * Must be called once for each open call.
 */
int pfs_close (struct pfs_instance * pfs,
	       int pfs_sd);


/** Get file attributes.
 */
int pfs_stat (struct pfs_instance * pfs,
	      const char * path,
	      struct stat * buf);

/** Read a symbolic link.
 */
int pfs_readlink (struct pfs_instance * pfs,
		  const char * path,
		  char * buf,
		  size_t bufsize);


/** Change file size.
 */
int pfs_truncate (struct pfs_instance * pfs,
		  const char * path,
		  off_t len);


/** Change open file size.
 */
int pfs_ftruncate (struct pfs_instance * pfs,
		   int pfs_fd,
		   off_t len);


/** Change open file size.
 *
 * Returns a NULL terminated array of NULL terminated string
 * containing the name of the entries.
 */
char ** pfs_readdir (struct pfs_instance * pfs,
		     const char * path);


/** Create a directory.
 */
int pfs_mkdir (struct pfs_instance * pfs,
	       const char * path);

/** Delete a file.
 */
int pfs_unlink (struct pfs_instance * pfs,
		const char * path);

/** Delete a directory.
 */
int pfs_rmdir (struct pfs_instance * pfs,
	       const char * path);

/** Creates a symbolic link.
 */
int pfs_symlink (struct pfs_instance * pfs,
		 const char * from,
		 const char * to);

/** Rename a file/directory
 */
int pfs_rename (struct pfs_instance * pfs,
		const char * old,
		const char * new);

/** Get fs stat
 */
int pfs_statfs (struct pfs_instance * pfs,
		struct statvfs * buf);

int pfs_utimens (struct pfs_instance * pfs,
		 const char * path,
		 const struct timespec tv[2]);


/* SPECIAL OPERATIONS */


struct pfs_instance * pfs_init (const char * root_path);

int pfs_destroy (struct pfs_instance * pfs);

int pfs_sync_cache (struct pfs_instance * pfs);

int pfs_set_updt_cb (struct pfs_instance * pfs,
		     int(*updt_cb)(struct pfs_instance *, struct pfs_updt *));

int pfs_create_group (struct pfs_instance * pfs,
		      const char * grp);

int pfs_add_sd (struct pfs_instance * pfs,
		const char * grp,
		const char * sd_owner,
		const char * sd_name,
		const char * sd_id);

int pfs_bootstrap (const char * root_path,
		   const char * sd_owner,
		   const char * sd_name);







#endif
