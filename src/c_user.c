/* ircd-micro, c_user.c -- user commands
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

/* XXX this is wrong */
static void m_ping(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	if (msg->command[1] == 'O') /* PONG */
		return;

	u_conn_f(conn, ":%s PONG %s :%s", me.name, me.name, msg->argv[0]);
}

static void m_version(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user *u = conn->priv;
	u_user_num(u, RPL_VERSION, PACKAGE_FULLNAME, me.name, "hi");
}

static void m_motd(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user *u = conn->priv;
	u_user_send_motd(u);
}

static void m_message(conn, msg)
struct u_conn *conn;
struct u_msg *msg;
{
	struct u_user *src = conn->priv;
	struct u_user *tgt;

	if (msg->argv[0][0] == '#') {
		u_user_num(src, ERR_GENERIC, "Can't send messages to channels yet");
		return;
	}

	tgt = u_user_by_nick(msg->argv[0]);
	if (tgt == NULL) {
		u_user_num(src, ERR_NOSUCHNICK, msg->argv[0]);
		return;
	}

	u_log(LG_DEBUG, "[%s -> %s] %s", src->nick, tgt->nick, msg->argv[1]);

	if (tgt->flags & USER_IS_LOCAL) {
		u_conn_f(((struct u_user_local*)tgt)->conn,
		         ":%s!%s@%s %s %s :%s", src->nick, src->ident, src->host,
		         msg->command, tgt->nick, msg->argv[1]);
	} else {
		u_user_num(src, ERR_GENERIC, "Can't send messages to remote users yet");
	}
}

struct u_cmd c_user[] = {
	{ "PING",    CTX_USER, m_ping,    1 },
	{ "PONG",    CTX_USER, m_ping,    0 },
	{ "VERSION", CTX_USER, m_version, 0 },
	{ "MOTD",    CTX_USER, m_motd,    0 },
	{ "PRIVMSG", CTX_USER, m_message, 2 },
	{ "NOTICE",  CTX_USER, m_message, 2 },
	{ "" },
};