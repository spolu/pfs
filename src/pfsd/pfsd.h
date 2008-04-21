#ifndef _PFSD_H
#define _PFSD_H

#include "global.h"

void * start_write_back (void * tid);

int updt_cb (struct pfs_instance * pfs,
	     struct pfs_updt * updt);

#endif
