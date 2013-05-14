/* ircd-micro, entity.c -- generic entity targeting
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

void make_server(e) u_entity *e;
{
	u_server *sv = e->v.sv;

	e->flags = ENT_SERVER;

	e->name = sv->name;
	e->id = sv->sid;

	u_log(LG_FINE, "*** make_server(): e->name=%s", e->name);
	u_log(LG_FINE, "*** make_server(): e->id=%s", e->id);

	e->link = sv->conn;
	e->loc = NULL;
	if (sv->hops == 1)
		e->loc = e->link;
}

void make_user(e) u_entity *e;
{
	u_user *u = e->v.u;

	e->flags = ENT_USER;

	e->name = u->nick;
	e->id = u->uid;

	u_log(LG_FINE, "*** make_user(): e->name=%s", e->name);
	u_log(LG_FINE, "*** make_user(): e->id=%s", e->id);

	if (u->flags & USER_IS_LOCAL) {
		e->loc = USER_LOCAL(u)->conn;
		e->link = e->loc;
	} else {
		e->loc = NULL;
		e->link = USER_REMOTE(u)->server->conn;
	}
}

u_entity *u_entity_from_name(e, s) u_entity *e; char *s;
{
	u_log(LG_FINE, "u_entity_from_name(%s)", s);

	if (strchr(s, '.')) {
		if (!(e->v.sv = u_server_by_name(s)))
			return NULL;
		make_server(e);
	} else {
		if (!(e->v.u = u_user_by_nick(s)))
			return NULL;
		make_user(e);
	}

	return e;
}

u_entity *u_entity_from_id(e, s) u_entity *e; char *s;
{
	u_log(LG_FINE, "u_entity_from_id(%s)", s);

	if (s[3]) {
		if (!(e->v.u = u_user_by_uid(s)))
			return NULL;
		make_user(e);
	} else {
		if (!(e->v.sv = u_server_by_sid(s)))
			return NULL;
		make_server(e);
	}

	return e;
}

u_entity *u_entity_from_ref(e, s) u_entity *e; char *s;
{
	return u_entity_from_id(e, ref_to_id(s));
}

u_entity *u_entity_from_user(e, u) u_entity *e; u_user *u;
{
	e->v.u = u;
	make_user(e);
	return e;
}

u_entity *u_entity_from_server(e, sv) u_entity *e; u_server *sv;
{
	e->v.sv = sv;
	make_server(e);
	return e;
}
