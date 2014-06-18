/* Tethys, chan.c -- channels
   Copyright (c) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

mowgli_patricia_t *all_chans;

static ulong cmode_get_flag_bits(u_modes *m)
{
	return ((u_chan*) m->target)->mode;
}

static bool cmode_set_flag_bits(u_modes *m, ulong fl)
{
	((u_chan*) m->target)->mode |= fl;
	return true;
}

static bool cmode_reset_flag_bits(u_modes *m, ulong fl)
{
	((u_chan*) m->target)->mode &= ~fl;
	return true;
}

static void *cmode_get_status_target(u_modes *m, char *user)
{
	u_chan *c = m->target;
	u_user *u;
	u_chanuser *cu;

	if (!(u = u_user_by_ref(m->setter->source, user))) {
		u_src_num(m->setter, ERR_NOSUCHNICK, user);
		return NULL;
	}

	if (!(cu = u_chan_user_find(c, u))) {
		u_src_num(m->setter, ERR_USERNOTINCHANNEL, c, u);
		return NULL;
	}

	return cu;
}

static bool cmode_set_status_bits(u_modes *m, void *tgt, ulong st)
{
	((u_chanuser*) tgt)->flags |= st;
	return true;
}

static bool cmode_reset_status_bits(u_modes *m, void *tgt, ulong st)
{
	((u_chanuser*) tgt)->flags &= ~st;
	return true;
}

static mowgli_list_t *cmode_get_list(u_modes *m, u_mode_info *info)
{
	u_chan *c = m->target;

	switch (info->ch) {
	case 'b': return &c->ban;
	case 'e': return &c->banex;
	case 'I': return &c->invex;
	case 'q': return &c->quiet;
	}

	return NULL;
}

static void cmode_sync(u_modes *m)
{
	u_chan *c = m->target;
	u_cookie_inc(&c->ck_flags);
}

static int cb_fwd(u_modes*, int, char*);
static int cb_key(u_modes*, int, char*);
static int cb_limit(u_modes*, int, char*);
static int cb_join(u_modes*, int, char*);

u_mode_info cmode_infotab[128] = {
	['c'] = { 'c', MODE_FLAG, 0, { .data = CMODE_NOCOLOR } },
	['g'] = { 'g', MODE_FLAG, 0, { .data = CMODE_FREEINVITE } },
	['i'] = { 'i', MODE_FLAG, 0, { .data = CMODE_INVITEONLY } },
	['m'] = { 'm', MODE_FLAG, 0, { .data = CMODE_MODERATED } },
	['n'] = { 'n', MODE_FLAG, 0, { .data = CMODE_NOEXTERNAL } },
	['p'] = { 'p', MODE_FLAG, 0, { .data = CMODE_PRIVATE } },
	['s'] = { 's', MODE_FLAG, 0, { .data = CMODE_SECRET } },
	['t'] = { 't', MODE_FLAG, 0, { .data = CMODE_TOPIC } },
	['z'] = { 'z', MODE_FLAG, 0, { .data = CMODE_OPMOD } },

	['f'] = { 'f', MODE_EXTERNAL, 0, { .fn = cb_fwd } },
	['k'] = { 'k', MODE_EXTERNAL, 0, { .fn = cb_key } },
	['l'] = { 'l', MODE_EXTERNAL, 0, { .fn = cb_limit } },
	['j'] = { 'j', MODE_EXTERNAL, 0, { .fn = cb_join } },

	['b'] = { 'b', MODE_LIST },
	['q'] = { 'q', MODE_LIST },
	['e'] = { 'e', MODE_LIST },
	['I'] = { 'I', MODE_LIST },
};

u_mode_ctx cmodes = {
	.infotab             = cmode_infotab,

	.get_flag_bits       = cmode_get_flag_bits,
	.set_flag_bits       = cmode_set_flag_bits,
	.reset_flag_bits     = cmode_reset_flag_bits,

	.get_status_target   = cmode_get_status_target,
	.set_status_bits     = cmode_set_status_bits,
	.reset_status_bits   = cmode_reset_status_bits,

	.get_list            = cmode_get_list,

	.sync                = cmode_sync,
};

u_bitmask_set cmode_flags;
u_bitmask_set cmode_cu_flags;

uint cmode_default = CMODE_TOPIC | CMODE_NOEXTERNAL;

mowgli_list_t cu_pfx_list;

u_cu_pfx *cu_pfx_op;
u_cu_pfx *cu_pfx_voice;

static int cb_fwd(u_modes *m, int on, char *arg)
{
	u_chan *tc, *c = m->target;
	u_chanuser *tcu;

	if (!u_mode_has_access(m))
		return on;

	if (!on) {
		if (c->forward) {
			free(c->forward);
			c->forward = NULL;
			u_mode_put(m, on, NULL);
		}
		return 0;
	}

	if (arg == NULL)
		return 0;

	if (!(tc = u_chan_get(arg)) && !(m->flags & MODE_FORCE_ALL)) {
		u_src_num(m->setter, ERR_NOSUCHCHANNEL, arg);
		return 1;
	}

	if (tc && m->setter->u && !(m->flags & MODE_FORCE_ALL)) {
		tcu = u_chan_user_find(tc, m->setter->u);

		if (tcu == NULL || !(tcu->flags & CU_PFX_OP)) {
			u_src_num(m->setter, ERR_CHANOPRIVSNEEDED, tc);
			return 1;
		}
	}

	if (c->forward)
		free(c->forward);
	c->forward = strdup(arg);
	u_mode_put(m, on, arg);

	return 1;
}

static int cb_key(u_modes *m, int on, char *arg)
{
	u_chan *c = m->target;

	if (!u_mode_has_access(m))
		return 1;

	if (!on) {
		if (c->key) {
			free(c->key);
			c->key = NULL;
			u_mode_put(m, on, "*");
		}
		return 1;
	}

	if (arg == NULL)
		return 0;

	if (c->key)
		free(c->key);
	c->key = strdup(arg);

	u_mode_put(m, on, c->key);
	return 1;
}

static int cb_limit(u_modes *m, int on, char *arg)
{
	char buf[128];
	u_chan *c = m->target;
	int lim;

	if (!u_mode_has_access(m))
		return on;

	if (!on) {
		if (c->limit > 0)
			u_mode_put(m, 0, NULL);
		c->limit = -1;
		return 0;
	}

	if (arg == NULL)
		return 0;

	lim = atoi(arg);
	if (lim < 1)
		return 1;

	c->limit = lim;
	snprintf(buf, 128, "%d", lim);
	u_mode_put(m, 1, buf);
	return 1;
}

static int cb_join(u_modes *m, int on, char *arg)
{
	/* TODO: +j. temporarily eat arg */
	return on;
}

static u_chan *chan_create_real(const char *name)
{
	u_chan *chan;

	if (!strchr(CHANTYPES, name[0]))
		return NULL;

	chan = malloc(sizeof(*chan));
	u_strlcpy(chan->name, name, MAXCHANNAME+1);
	chan->ts = NOW.tv_sec;
	chan->topic[0] = '\0';
	chan->topic_setter[0] = '\0';
	chan->topic_time = 0;
	chan->mode = cmode_default;
	chan->flags = 0;
	u_cookie_reset(&chan->ck_flags);
	chan->members = u_map_new(0);
	mowgli_list_init(&chan->ban);
	mowgli_list_init(&chan->quiet);
	mowgli_list_init(&chan->banex);
	mowgli_list_init(&chan->invex);
	chan->invites = u_map_new(0);
	chan->forward = NULL;
	chan->key = NULL;
	chan->limit = -1;

	if (name[0] == '&')
		chan->flags |= CHAN_LOCAL;

	mowgli_patricia_add(all_chans, chan->name, chan);

	return chan;
}

u_chan *u_chan_get(char *name)
{
	return mowgli_patricia_retrieve(all_chans, name);
}

u_chan *u_chan_create(char *name)
{
	if (u_chan_get(name))
		return NULL;

	return chan_create_real(name);
}

u_chan *u_chan_get_or_create(char *name, bool *created)
{
	u_chan *chan;

	*created = false;

	chan = u_chan_get(name);
	if (chan != NULL)
		return chan;

	if (name[0] != '#') /* TODO: hnggg!!! */
		return NULL;

	*created = true;

	return chan_create_real(name);
}

static void drop_list(mowgli_list_t *list)
{
	mowgli_node_t *n, *tn;

	MOWGLI_LIST_FOREACH_SAFE(n, tn, list->head) {
		mowgli_node_delete(n, list);
		free(n->data);
	}
}

static void drop_param(char **p)
{
	if (*p != NULL)
		free(*p);
	*p = NULL;
}

void u_chan_drop(u_chan *chan)
{
	/* TODO: u_map_free callback! */
	/* TODO: send PART to all users in this channel! */
	u_map_free(chan->members);
	drop_list(&chan->ban);
	drop_list(&chan->quiet);
	drop_list(&chan->banex);
	drop_list(&chan->invex);
	u_clr_invites_chan(chan);
	drop_param(&chan->forward);
	drop_param(&chan->key);

	mowgli_patricia_delete(all_chans, chan->name);
	free(chan);
}

char *u_chan_modes(u_chan *c, int on_chan)
{
	static char buf[512];
	char chs[64], args[512];
	const char *bit = CMODE_BITS;
	char *s = chs, *p = args;
	ulong mode = c->mode;

	*s++ = '+';
	for (; mode; bit++, mode >>= 1) {
		if (mode & 1)
			*s++ = *bit;
	}

	if (c->forward) {
		*s++ = 'f';
		p += sprintf(p, " %s", c->forward);
	}
	if (c->key) {
		*s++ = 'k';
		if (on_chan)
			p += sprintf(p, " %s", c->key);
	}
	if (c->limit >= 0) {
		*s++ = 'l';
		p += sprintf(p, " %d", c->limit);
	}

	*s = *p = '\0';

	sprintf(buf, "%s%s", chs, args);
	return buf;
}

int u_chan_mode_register(u_mode_info *info, ulong *flag_ret)
{
	u_mode_info *tbl;
	ulong flag = 0;

	if (info->ch < 0 || info->ch > 128) /* invalid char */
		return -1;

	tbl = cmode_infotab + info->ch;

	if (tbl->ch) /* char in use */
		return -1;

	if (info->type == MODE_FLAG &&
	    !(flag = u_bitmask_alloc(&cmode_flags)))
		return -1;

	if (info->type == MODE_STATUS &&
	    !(flag = u_bitmask_alloc(&cmode_cu_flags)))
		return -1;

	memcpy(tbl, info, sizeof(*tbl));

	if (tbl->type == MODE_FLAG ||
	    tbl->type == MODE_STATUS)
		tbl->arg.data = flag;

	if (flag_ret != NULL)
		*flag_ret = flag;

	return 0;
}

void u_chan_mode_unregister(u_mode_info *info)
{
	u_mode_info *tbl;

	if (info->ch < 0 || info->ch > 128) /* invalid char */
		return;

	tbl = cmode_infotab + info->ch;

	if (tbl->type == MODE_FLAG)
		u_bitmask_free(&cmode_flags, tbl->arg.data);

	if (tbl->type == MODE_STATUS)
		u_bitmask_free(&cmode_cu_flags, tbl->arg.data);

	memset(tbl, 0, sizeof(*tbl));
}

u_cu_pfx *u_chan_status_add(char ch, char prefix)
{
	u_mode_info info;
	u_cu_pfx *cs;
	ulong mask;

	memset(&info, 0, sizeof(info));
	info.ch = ch;
	info.type = MODE_STATUS;

	if (u_chan_mode_register(&info, &mask) < 0)
		return NULL;

	cs = calloc(1, sizeof(*cs));
	cs->info = cmode_infotab + ch;
	cs->prefix = prefix;
	cs->mask = mask;

	mowgli_node_add(cs, &cs->n, &cu_pfx_list);

	return cs;
}

int u_chan_send_topic(u_chan *c, u_user *u)
{
	if (c->topic[0]) {
		u_user_num(u, RPL_TOPIC, c, c->topic);
		u_user_num(u, RPL_TOPICWHOTIME, c, c->topic_setter,
		           c->topic_time);
	} else {
		u_user_num(u, RPL_NOTOPIC, c);
	}

	return 0;
}

/* :my.name 353 nick = #chan :...
   *       *****    ***     **  = 11 */
int u_chan_send_names(u_chan *c, u_user *u)
{
	u_map_each_state st;
	u_strop_wrap wrap;
	u_user *tu;
	u_chanuser *cu;
	mowgli_node_t *n;
	char *s, pfx;
	int sz;

	pfx = c->mode & CMODE_PRIVATE ? '*'
	    : c->mode & CMODE_SECRET ? '@'
	    : '=';

	sz = strlen(me.name) + strlen(u->nick) + strlen(c->name) + 11;
	u_strop_wrap_start(&wrap, 510 - sz);
	U_MAP_EACH(&st, c->members, &tu, &cu) {
		char *p, nbuf[MAXNICKLEN+3];

		p = nbuf;
		MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
			u_cu_pfx *cs = n->data;
			if ((cu->flags & cs->mask) &&
			    (p == nbuf || u->flags & CAP_MULTI_PREFIX))
				*p++ = cs->prefix;
		}
		strcpy(p, tu->nick);

		while ((s = u_strop_wrap_word(&wrap, nbuf)) != NULL)
			u_user_num(u, RPL_NAMREPLY, pfx, c, s);
	}
	if ((s = u_strop_wrap_word(&wrap, NULL)) != NULL)
		u_user_num(u, RPL_NAMREPLY, pfx, c, s);

	u_user_num(u, RPL_ENDOFNAMES, c);

	return 0;
}

int u_chan_send_list(u_chan *c, u_user *u, mowgli_list_t *list)
{
	mowgli_node_t *n;
	u_chanuser *cu;
	u_listent *ban;
	bool opsonly = false;
	int entry, end;

	if (list == &c->quiet) {
		entry = RPL_QUIETLIST;
		end = RPL_ENDOFQUIETLIST;
	} else if (list == &c->invex) {
		entry = RPL_INVITELIST;
		end = RPL_ENDOFINVITELIST;
		opsonly = true;
	} else if (list == &c->banex) {
		entry = RPL_EXCEPTLIST;
		end = RPL_ENDOFEXCEPTLIST;
		opsonly = true;
	} else {
		/* shrug */
		entry = RPL_BANLIST;
		end = RPL_ENDOFBANLIST;
	}

	/* don't send +eI to non-ops */
	cu = u_chan_user_find(c, u);
	if (opsonly && (!cu || !(cu->flags & CU_PFX_OP))) {
		u_user_num(u, ERR_CHANOPRIVSNEEDED, c);
		return 0;
	}

	MOWGLI_LIST_FOREACH(n, list->head) {
		ban = n->data;
		u_user_num(u, entry, c, ban->mask, ban->setter, ban->time);
	}

	u_user_num(u, end, c);

	return 0;
}

void u_add_invite(u_chan *c, u_user *u)
{
	/* TODO: check invite limits */
	u_map_set(c->invites, u, u);
	u_map_set(u->invites, c, c);
}

void u_del_invite(u_chan *c, u_user *u)
{
	u_map_del(c->invites, u);
	u_map_del(u->invites, c);
}

int u_has_invite(u_chan *c, u_user *u)
{
	return !!u_map_get(c->invites, u);
}

static void inv_chan_cb(u_map *map, u_user *u, u_user *u_, u_chan *c)
{
	u_del_invite(c, u);
}
void u_clr_invites_chan(u_chan *c)
{
	u_map_each(c->invites, (u_map_cb_t*)inv_chan_cb, c);
}

static void inv_user_cb(u_map *map, u_chan *c, u_chan *c_, u_user *u)
{
	u_del_invite(c, u);
}
void u_clr_invites_user(u_user *u)
{
	u_map_each(u->invites, (u_map_cb_t*)inv_user_cb, u);
}

/* XXX: assumes the chanuser doesn't already exist */
u_chanuser *u_chan_user_add(u_chan *c, u_user *u)
{
	u_chanuser *cu;

	cu = malloc(sizeof(*cu));
	cu->flags = 0;
	u_cookie_reset(&cu->ck_flags);
	cu->c = c;
	cu->u = u;

	u_map_set(c->members, u, cu);
	u_map_set(u->channels, c, cu);

	return cu;
}

void u_chan_user_del(u_chanuser *cu)
{
	u_chan *c = cu->c;
	u_user *u = cu->u;

	u_map_del(c->members, u);
	u_map_del(u->channels, c);

	free(cu);

	if (c->members->size == 0) {
		u_log(LG_DEBUG, "u_chan_user_del: %C empty, dropping...", c);
		u_chan_drop(c);
	}
}

u_chanuser *u_chan_user_find(u_chan *c, u_user *u)
{
	return u_map_get(c->members, u);
}

typedef struct extban extb_t;

struct extban {
	char ch;
	int (*cb)(struct extban*, u_chan*, u_user*, char *data);
	void *priv;
};

static int ex_oper(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	return IS_OPER(u);
}

static int ex_account(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	if (!IS_LOGGED_IN(u))
		return 0;
	if (data == NULL)
		return 1;
	return streq(u->acct, data);
}

static int ex_channel(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	u_chan *tc;
	if (data == NULL)
		return 0;
	tc = u_chan_get(data);
	if (tc == NULL || u_chan_user_find(tc, u) == NULL)
		return 0;
	return 1;
}

static int ex_gecos(extb_t *ex, u_chan *c, u_user *u, char *data)
{
	if (data == NULL)
		return 0;
	return match(data, u->gecos);
}

static extb_t extbans[] = {
	{ 'o', ex_oper, NULL },
	{ 'a', ex_account, NULL },
	{ 'c', ex_channel, NULL },
	{ 'r', ex_gecos, NULL },
	{ 0 }
};

static int matches_ban(u_chan *c, u_user *u, char *mask, char *host)
{
	char *data;
	extb_t *extb = extbans;
	int invert, banned;

	if (*mask == '$') {
		data = strchr(mask, ':');
		if (data != NULL)
			data++;
		mask++;

		invert = 0;
		if (*mask == '~') {
			invert = 1;
			mask++;	
		}

		for (; extb->ch; extb++) {
			if (extb->ch == *mask)
				break;
		}

		if (!extb->ch)
			return 0;

		banned = extb->cb(extb, c, u, data);
		if (invert)
			banned = !banned;
		return banned;
	}

	return match(mask, host);
}

static int is_in_list(u_chan *c, u_user *u, char *host, mowgli_list_t *list)
{
	mowgli_node_t *n;
	u_listent *ban;

	MOWGLI_LIST_FOREACH(n, list->head) {
		ban = n->data;
		if (matches_ban(c, u, ban->mask, host))
			return 1;
	}

	return 0;
}

int u_entry_blocked(u_chan *c, u_user *u, char *key)
{
	char host[BUFSIZE];
	int invited = u_has_invite(c, u);

	snf(FMT_USER, host, BUFSIZE, "%H", u);

	if ((c->mode & CMODE_INVITEONLY)) {
		if (!is_in_list(c, u, host, &c->invex) && !invited)
			return ERR_INVITEONLYCHAN;
	}

	if (c->key != NULL) {
		if (key == NULL || !streq(c->key, key))
			return ERR_BADCHANNELKEY;
	}

	if (is_in_list(c, u, host, &c->ban)) {
		if (!is_in_list(c, u, host, &c->banex))
			return ERR_BANNEDFROMCHAN;
	}

	if (c->limit > 0 && c->members->size >= c->limit && !invited)
		return ERR_CHANNELISFULL;

	/* TODO: an invite also allows +j and +r to be bypassed */

	return 0;
}

u_chan *u_find_forward(u_chan *c, u_user *u, char *key)
{
	int forwards_left = 30;

	while (forwards_left-- > 0) {
		if (c->forward == NULL)
			return NULL;

		c = u_chan_get(c->forward);

		if (c == NULL)
			return NULL;
		if (!u_entry_blocked(c, u, key))
			return c;
	}

	return NULL;
}

int u_is_muted(u_chanuser *cu)
{
	char buf[512];

	snf(FMT_USER, buf, 512, "%H", cu->u);

	if (u_cookie_cmp(&cu->ck_flags, &cu->c->ck_flags) >= 0)
		return cu->flags & CU_MUTED;

	u_cookie_cpy(&cu->ck_flags, &cu->c->ck_flags);
	cu->flags &= ~CU_MUTED;

	if (cu->flags & (CU_PFX_OP | CU_PFX_VOICE))
		return 0;

	if (!is_in_list(cu->c, cu->u, buf, &cu->c->quiet)
	    && !(cu->c->mode & CMODE_MODERATED))
		return 0;

	cu->flags |= CU_MUTED;
	return CU_MUTED; /* not 1, to mimic cu->flags & CU_MUTED */
}

static int restore_specific_chan(const char *name, mowgli_json_t *jch)
{
	int err;
	int i;
	u_chan *ch = NULL;
	mowgli_json_t *jmasks, *jckflags, *jmems, *jmem, *jmemckflags, *jiuid;
	mowgli_list_t *maska, *invites;
	mowgli_string_t *jstopic, *jstopicsetter, *jsforward, *jskey, *jsmask, *jssetter, *jsiuid;
	mowgli_json_t *jmask;
	mowgli_node_t *n;
	mowgli_patricia_iteration_state_t state;
	u_listent *le;
	u_ts_t time = 0;
	u_user *u;
	u_chanuser *cu;
	const char *k;
	char uid[10] = {};

	if (strlen(name) > MAXCHANNAME)
		return -1;

	ch = chan_create_real(name);

	jmasks = json_ogeto(jch, "masks");
	if (!jmasks)
		return -1;

	if ((err = json_ogettime(jch, "ts", &ch->ts)) < 0)
		return err;
	if ((err = json_ogetu(jch, "flags", &ch->flags)) < 0)
		return err;
	if ((err = json_ogeti(jch, "limit", &ch->limit)) < 0)
		return err;
	if ((err = json_ogetu(jch, "mode", &ch->mode)) < 0)
		return err;
	if ((err = json_ogettime(jch, "topic_time", &ch->topic_time)) < 0)
		return err;
	jckflags = json_ogeto(jch, "ck_flags");
	if (!jckflags) {
		err = -1;
		return err;
	}
	if ((err = u_cookie_from_json(jckflags, &ch->ck_flags)) < 0)
		return err;

	jstopic = json_ogets(jch, "topic");
	if (!jstopic || jstopic->pos > MAXTOPICLEN)
		return err;
	memcpy(ch->topic, jstopic->str, jstopic->pos);

	jstopicsetter = json_ogets(jch, "topic_setter");
	if (!jstopicsetter || jstopicsetter->pos > MAXNICKLEN)
		return err;
	memcpy(ch->topic_setter, jstopicsetter->str, jstopicsetter->pos);

	jsforward = json_ogets(jch, "forward");
	if (jsforward) {
		ch->forward = malloc(jsforward->pos + 1);
		memcpy(ch->forward, jsforward->str, jsforward->pos);
		ch->forward[jsforward->pos] = '\0';
	}

	jskey = json_ogets(jch, "key");
	if (jskey) {
		ch->key = malloc(jskey->pos + 1);
		memcpy(ch->key, jskey->str, jskey->pos);
		ch->key[jskey->pos] = '\0';
	}

	/* MASKS */
	struct masklist {
		mowgli_list_t *list;
		const char *type;
	} masklists[] = {
		{&ch->ban,   "b"},
		{&ch->quiet, "q"},
		{&ch->banex, "e"},
		{&ch->invex, "I"},
	};

	for (i=0;i<arraylen(masklists);++i) {
		maska = json_ogeta(jmasks, masklists[i].type);
		if (!maska)
			continue;

		MOWGLI_LIST_FOREACH(n, maska->head) {
			jmask    = n->data;
			jsmask   = json_ogets  (jmask, "mask");
			if (!jsmask || jsmask->pos > 255) {
				err = -1;
				return err;
			}

			jssetter = json_ogets  (jmask, "setter");
			if (!jssetter || jssetter->pos > 255) {
				err = -1;
				return err;
			}

			if ((err = json_ogettime(jmask, "time", &time)) < 0)
				return err;

			le = malloc(sizeof(*le));
			memcpy(le->mask, jsmask->str, jsmask->pos);
			le->mask[jsmask->pos] = '\0';
			memcpy(le->setter, jssetter->str, jssetter->pos);
			le->setter[jssetter->pos] = '\0';
			le->time = time;

			mowgli_node_add(le, &le->n, masklists[i].list);
		}
	}

	jmems = json_ogeto_c(jch, "members");
	if (!jmems) {
		err = -1;
		return err;
	}

	MOWGLI_PATRICIA_FOREACH(jmem, &state, MOWGLI_JSON_OBJECT(jmems)) {
		k = mowgli_patricia_elem_get_key(state.pspare[0]);
		u = u_user_by_uid(k);
		if (!u) {
			err = -1;
			return err;
		}
		cu = u_chan_user_add(ch, u);
		if ((err = json_ogetu(jmem, "flags", &cu->flags)) < 0)
			return err;
		jmemckflags = json_ogeto(jmem, "ck_flags");
		if ((err = u_cookie_from_json(jmemckflags, &cu->ck_flags)) < 0)
			return err;
	}

	invites = json_ogeta(jch, "invites");
	if (!invites) {
		err = -1;
		return err;
	}

	MOWGLI_LIST_FOREACH(n, invites->head) {
		jiuid = n->data;
		if (!jiuid || MOWGLI_JSON_TAG(jiuid) != MOWGLI_JSON_TAG_STRING) {
			err = -1;
			return err;
		}

		jsiuid = MOWGLI_JSON_STRING(jiuid);
		if (jsiuid->pos != 9) {
			err = -1;
			return err;
		}

		memcpy(uid, jsiuid->str, 9);

		u = u_user_by_uid(uid);
		if (!u) {
			err = -1;
			return err;
		}

		u_add_invite(ch, u);
	}

	return 0;
}

int restore_chan(void)
{
	int err;
	mowgli_json_t *jchans = json_ogeto(upgrade_json, "channels");
	const char *k;
	mowgli_json_t *jch;
	mowgli_patricia_iteration_state_t state;

	if (!jchans)
		return -1;

	u_log(LG_DEBUG, "Restoring channels...");
	MOWGLI_PATRICIA_FOREACH(jch, &state, MOWGLI_JSON_OBJECT(jchans)) {
		k = mowgli_patricia_elem_get_key(state.pspare[0]);
		if ((err = restore_specific_chan(k, jch)) < 0)
			return err;
	}
	u_log(LG_DEBUG, "Done restoring channels");

	return 0;
}

static int dump_specific_chan(u_chan *ch, mowgli_json_t *j_chans)
{
	int i;
	mowgli_node_t *n;
	struct u_listent *m;
	u_map_each_state st;
	u_user *u;
	u_chanuser *cu;
	mowgli_json_t *jch, *jmask, *jmasks, *jmasktype,
	              *jinvites, *jinvite,
	              *jmems, *jmem;

	struct masklist {
		mowgli_list_t *list;
		const char *type;
	} masklists[] = {
		{&ch->ban,   "b"},
		{&ch->quiet, "q"},
		{&ch->banex, "e"},
		{&ch->invex, "I"},
	};

	jch = mowgli_json_create_object();
	json_oseto  (j_chans, ch->name, jch);

	json_osets  (jch, "topic",         ch->topic);
	json_osets  (jch, "topic_setter",  ch->topic_setter);
	json_osettime(jch, "topic_time",    ch->topic_time);
	json_oseti  (jch, "mode",          ch->mode);
	json_oseti  (jch, "flags",         ch->flags);
	json_osets  (jch, "forward",       ch->forward);
	json_osets  (jch, "key",           ch->key);
	json_oseti  (jch, "limit",         ch->limit);
	json_osettime(jch, "ts",            ch->ts);
	json_oseto  (jch, "ck_flags",      u_cookie_to_json(&ch->ck_flags));

	jmasks = mowgli_json_create_object();
	json_oseto  (jch, "masks",         jmasks);

	/* MASKS */
	for (i=0; i<arraylen(masklists); ++i) {
		jmasktype = mowgli_json_create_array();
		json_oseto(jmasks, masklists[i].type, jmasktype);

		MOWGLI_LIST_FOREACH(n, masklists[i].list->head) {
			m = n->data;

			jmask = mowgli_json_create_object();
			json_append(jmasktype, jmask);
			json_osets  (jmask, "mask",    m->mask);
			json_osets  (jmask, "setter",  m->setter);
			json_osettime(jmask, "time",    m->time);
		}
	}

	/* INVITES */
	jinvites = mowgli_json_create_array();
	json_oseto  (jch, "invites",       jinvites);


	U_MAP_EACH(&st, ch->invites, &u, &u) {
		jinvite = mowgli_json_create_string(u->uid);
		json_append(jinvites, jinvite);
	}

	/* MEMBERS */
	jmems = mowgli_json_create_object();
	json_oseto  (jch, "members",       jmems);

	U_MAP_EACH(&st, ch->members, &u, &cu) {
		jmem = mowgli_json_create_object();
		json_oseto(jmems, u->uid, jmem);
		json_oseti(jmem,  "flags", cu->flags);
		json_oseto(jmem,  "ck_flags", u_cookie_to_json(&cu->ck_flags));
	}

	return 0;
}

int dump_chan(void)
{
	int err;
	u_chan *ch;
	mowgli_patricia_iteration_state_t state;

	mowgli_json_t *j_chans = mowgli_json_create_object();
	json_oseto(upgrade_json, "channels", j_chans);

	MOWGLI_PATRICIA_FOREACH(ch, &state, all_chans) {
		if ((err = dump_specific_chan(ch, j_chans)) < 0)
			return err;
	}

	return 0;
}

/* Initialization
 * --------------
 */
int init_chan(void)
{
	int i;

	if (!(all_chans = mowgli_patricia_create(ascii_canonize)))
		return -1;

	u_bitmask_reset(&cmode_flags);
	for (i=0; i<128; i++) {
		u_mode_info *info = cmode_infotab + i;
		if (!info->ch || info->type != MODE_FLAG)
			continue;
		u_bitmask_used(&cmode_flags, info->arg.data);
	}

	u_bitmask_reset(&cmode_cu_flags);
	u_bitmask_used(&cmode_cu_flags, CU_FLAGS_USED);

	mowgli_list_init(&cu_pfx_list);

	cu_pfx_op    = u_chan_status_add('o', '@');
	cu_pfx_voice = u_chan_status_add('v', '+');

	return 0;
}

/* vim: set noet: */
