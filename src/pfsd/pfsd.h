/*
 * pFS Daemon
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */


#ifndef _PFSD_H
#define _PFSD_H

void * start_write_back (void * tid);

int updt_cb (struct pfs_instance * pfs,
	     struct pfs_updt * updt);

#endif
