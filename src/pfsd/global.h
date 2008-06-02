#ifndef _PFSD_GLOBAL_H
#define _PFSD_GLOBAL_H

#define LAN_CONN 0x01
#define BTH_CONN 0x02

#define TIMEOUT 500000
#define LISTENQ 20


                              /* ARGS */
#define GRP_STAT "GRP_STAT"   /* grp_id */
#define ONLINE   "ONLINE"     /* tun_conn tun_port sd_id sd_owner sd_name */
#define OFFLINE  "OFFLINE"    /* tun_conn sd_id */
#define UPDT     "UPDT"       /* updt */
#define GET_DATA "GET_DATA"   /* dst_id */
#define DATA     "DATA"       /* len data */
#define OK       "OK"         /* */
#define ADD_SD   "ADD_SD"     /* grp_name sd_owner sd_name */
#define CLOSE    "CLOSE"      /* */
#define LIST_SD  "LIST_SD"    /* */
#define ADD_GRP  "ADD_GRP"    /* grp_id ... */
#define CREAT_GRP "CREAT_GRP" /* grp_name */
#define ERROR "ERROR"         /* */

#endif
