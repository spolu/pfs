#ifndef _PFSD_PROP_H
#define _PFSD_PROP_H

struct prop_arg;
struct pfs_updt;

int show_time (void);
int updater (struct prop_arg * prop_arg);
int update_status (int tun_sd,
		   char * grp_id,
		   char * sd_id);
int net_prop_updt (int tun_sd,
		   char * grp_id,
		   char * sd_id,
		   struct pfs_updt * updt);
int net_write_updt (int tun_sd,
		    struct pfs_updt * updt);

#endif
