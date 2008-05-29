#ifndef _PFSD_SRV_H
#define _PFSD_SRV_H

#include "pfsd.h"

void * start_srv (void * v);
void * handle_client (void * sd);

int handle_status (int cli_sd);
int handle_online (int cli_sd);
int handle_offline (int cli_sd);
int handle_updt (int cli_sd);
int handle_add_sd (int cli_sd);


#endif
