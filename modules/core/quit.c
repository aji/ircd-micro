/* ircd-micro, core/quit.c -- Provides QUIT command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_u_quit(u_sourceinfo *si, u_msg *msg)
{
	char *r1 = "", *r2 = msg->argc > 0 ? msg->argv[0] : "";

	if (si->u == NULL) {
		u_log(LG_SEVERE, "Received QUIT from nonexistent %s", si->id);
		return 0;
	}

	if (SRC_IS_LOCAL_USER(si))
		r1 = msg->argc > 0 ? "Quit: " : "Client Quit";

	u_sendto_visible(si->u, ST_USERS, ":%H QUIT :%s%s", si->u, r1, r2);
	u_link_f(si->local, ":%H QUIT :%s%s", si->u, r1, r2);
	u_sendto_servers(si->source, ":%H QUIT :%s%s", si->u, r1, r2);

	if (IS_LOCAL_USER(si->u))
		u_link_close(si->u->link);
	u_user_destroy(si->u);

	return 0;
}

static u_cmd quit_cmdtab[] = {
	{ "QUIT", SRC_USER, c_u_quit, 0 },
	{ }
};

MICRO_MODULE_V1(
	"core/quit", "Alex Iadicicco", "QUIT command",
	NULL, NULL, quit_cmdtab);
