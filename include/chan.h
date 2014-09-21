/* ircd-micro, chan.h -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_CHAN_H__
#define __INC_CHAN_H__

#define MAXCHANNAME 50
#define MAXTOPICLEN 250

#define CHANTYPES "#&"

/* channel flags */
#define CHAN_LOCAL         0x00000001
#define CHAN_PERMANENT     0x00000002

/* channel modes */
#define CMODE_NOCOLOR      0x00000001  /* +c */
#define CMODE_FREEINVITE   0x00000002  /* +g */
#define CMODE_INVITEONLY   0x00000004  /* +i */
#define CMODE_MODERATED    0x00000008  /* +m */
#define CMODE_NOEXTERNAL   0x00000010  /* +n */
#define CMODE_PRIVATE      0x00000020  /* +p */
#define CMODE_SECRET       0x00000040  /* +s */
#define CMODE_TOPIC        0x00000080  /* +t */
#define CMODE_OPMOD        0x00000100  /* +z */

#define CMODE_BITS "cgimnpstz" /* kind of terrible */

#define CM_DENY   0x01

/* prefixes */
#define CU_MUTED           0x00010000

#define CU_FLAGS_USED      0x00010000

typedef struct u_chan u_chan;
typedef struct u_chanuser u_chanuser;
typedef struct u_cu_pfx u_cu_pfx;

#include "chan.h"
#include "user.h"
#include "mode.h"

struct u_chan {
	u_ts_t ts;
	char name[MAXCHANNAME+1];
	char topic[MAXTOPICLEN+1];
	char topic_setter[MAXNICKLEN+1];
	u_ts_t topic_time;
	uint mode, flags;
	u_cookie ck_flags;
	u_map *members;
	mowgli_list_t ban, quiet, banex, invex;
	u_map *invites;
	char *forward, *key;
	int limit;
};

struct u_chanuser {
	uint flags;
	u_cookie ck_flags;
	u_chan *c;
	u_user *u;
};

struct u_cu_pfx {
	mowgli_node_t n;

	u_mode_info *info;
	ulong mask;
	char prefix;
};

extern mowgli_patricia_t *all_chans;

extern u_mode_info cmode_infotab[128];
extern u_mode_ctx cmodes;
extern u_bitmask_set cmode_flags;
extern u_bitmask_set cmode_cu_flags;
extern uint cmode_default;

extern mowgli_list_t cu_pfx_list;
extern u_cu_pfx *cu_pfx_op;
extern u_cu_pfx *cu_pfx_voice;
#define CU_PFX_OP (cu_pfx_op->mask)
#define CU_PFX_VOICE (cu_pfx_voice->mask)

extern u_chan *u_chan_get(char*);
extern u_chan *u_chan_create(char*);
extern u_chan *u_chan_get_or_create(char*, bool *created);
extern void u_chan_drop(u_chan*);

extern char *u_chan_modes(u_chan*, int un_chan);

extern int u_chan_mode_register(u_mode_info*, ulong *mask);
extern void u_chan_mode_unregister(u_mode_info*);
extern u_cu_pfx *u_chan_status_add(char mode, char prefix);

extern int u_chan_send_topic(u_chan*, u_user*);
extern int u_chan_send_names(u_chan*, u_user*);
extern int u_chan_send_list(u_chan*, u_user*, mowgli_list_t*);

extern void u_add_invite(u_chan*, u_user*);
extern void u_del_invite(u_chan*, u_user*);
extern int u_has_invite(u_chan*, u_user*);
extern void u_clr_invites_chan(u_chan*);
extern void u_clr_invites_user(u_user*);

extern u_chanuser *u_chan_user_add(u_chan*, u_user*);
extern void u_chan_user_del(u_chanuser*);
extern u_chanuser *u_chan_user_find(u_chan*, u_user*);

extern int u_entry_blocked(u_chan*, u_user*, char *key);
extern u_chan *u_find_forward(u_chan*, u_user*, char *key);
extern int u_is_muted(u_chanuser*);

extern int init_chan(void);
extern int dump_chan(void);
extern int restore_chan(void);

#endif
