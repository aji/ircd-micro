/* ircd-micro, core/whois -- WHOIS command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root. */

#include "ircd.h"

static void whois_channels(u_sourceinfo *si, u_user *tu)
{
	u_map_each_state state;
	u_chan *c; u_chanuser *cu;
	u_strop_wrap wrap;
	mowgli_node_t *n;
	char *s;

	u_strop_wrap_start(&wrap,
	    510 - MAXSERVNAME - MAXNICKLEN - MAXNICKLEN - 9);

	U_MAP_EACH(&state, tu->channels, &c, &cu) {
		char *p, cbuf[MAXCHANNAME+3];
		int retrying = 0;

		if (c->mode & (CMODE_PRIVATE | CMODE_SECRET)
		    && !u_chan_user_find(c, si->u))
			continue;

		p = cbuf;
		MOWGLI_LIST_FOREACH(n, cu_pfx_list.head) {
			u_cu_pfx *cs = n->data;
			if (cu->flags & cs->mask)
				*p++ = cs->prefix;
		}
		strcpy(p, c->name);

		while ((s = u_strop_wrap_word(&wrap, cbuf)))
			u_src_num(si, RPL_WHOISCHANNELS, tu->nick, s);
	}

	if ((s = u_strop_wrap_word(&wrap, NULL))) /* leftovers */
		u_src_num(si, RPL_WHOISCHANNELS, tu->nick, s);
}

static int c_u_whois(u_sourceinfo *si, u_msg *msg)
{
	char *nick, *s;
	u_user *tu;
	u_server *sv;
	bool long_whois = false;

	nick = msg->argv[msg->argc - 1];
	if ((s = strchr(nick, ','))) /* cut at ',' idk why */
		*s = '\0';

	if (!(tu = u_user_by_ref(si->source, nick)))
		return u_src_num(si, ERR_NOSUCHNICK, nick);


	/* check hunted */

	sv = NULL;
	if (msg->argc > 1) {
		char *server = msg->argv[0];
		long_whois = true;
		if (!(sv = u_server_by_ref(si->source, server))
		    && u_user_by_ref(si->source, server) != tu) {
			u_src_num(si, ERR_NOSUCHSERVER, server);
			return 0;
		}
		if (sv == NULL)
			sv = tu->sv;
	}

	if (sv != NULL && sv != &me) {
		u_link_f(sv->link, ":%I WHOIS %S %s", si, sv, nick);
		return 0;
	}


	/* perform whois */

	sv = tu->sv;
	u_src_num(si, RPL_WHOISUSER, tu->nick, tu->ident, tu->host, tu->gecos);

	if (!IS_SERVICE(tu))
		whois_channels(si, tu);

	u_src_num(si, RPL_WHOISSERVER, tu->nick, sv->name, sv->desc);

	if (IS_AWAY(tu))
		u_src_num(si, RPL_AWAY, tu->nick, tu->away);

	if (IS_SERVICE(tu))
		u_src_num(si, RPL_WHOISOPERATOR, tu->nick, "a Network Service");
	else if (IS_OPER(tu))
		u_src_num(si, RPL_WHOISOPERATOR, tu->nick, "an IRC operator");

	if (IS_LOGGED_IN(tu))
		u_src_num(si, RPL_WHOISLOGGEDIN, tu->nick, tu->acct);

	/* TODO: use long_whois */

	u_src_num(si, RPL_ENDOFWHOIS, tu->nick);

	return 0;
}

static u_cmd whois_cmdtab[] = {
	{ "WHOIS", SRC_USER, c_u_whois, 1 },
	{ }
};

MICRO_MODULE_V1(
	"core/whois", "Alex Iadicicco", "WHOIS command",
	NULL, NULL, whois_cmdtab);
