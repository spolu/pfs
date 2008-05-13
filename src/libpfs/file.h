/*
 * File Helper functions
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_FILE_H
#define _PFS_FILE_H

#include "instance.h"

int pfs_file_link_new_id (struct pfs_instance * pfs,
			  const char * file_id,
			  char * new_id);

int pfs_file_unlink (struct pfs_instance * pfs,
		      char * file_id);

int pfs_file_create (struct pfs_instance * pfs,
		     char * id,
		     int flags);

int pfs_file_copy_id (struct pfs_instance * pfs,
		      const char * file_id,
		      char * new_id);


#endif
