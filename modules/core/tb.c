/* ircd-micro, core/tb -- TB command
   Copyright (C) 2014 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#include "ircd.h"

static int c_s_tb(u_sourceinfo *si, u_msg *msg)
{
	char *chan = msg->argv[0];
	char *topic = msg->argv[msg->argc - 1];
	int ts;
	u_chan *c;

	msg->propagate = CMD_DO_BROADCAST;

	if (!(c = u_chan_get(chan))) {
		u_log(LG_WARN, "%I sent TB for nonexistent %s", si, chan);
		return 0;
	}

	ts = atoi(msg->argv[1]);
	if (c->topic[0] && ts >= c->topic_time)
		return 0;

	c->topic_time = ts;

	if (msg->argc > 3) {
		u_strlcpy(c->topic_setter, msg->argv[2], MAXNICKLEN+1);
	} else {
		snf(FMT_USER, c->topic_setter, MAXNICKLEN+1, "%I", si);
	}

	if (streq(topic, c->topic))
		return 0;

	u_strlcpy(c->topic, topic, MAXTOPICLEN+1);

	u_sendto_chan(c, NULL, ST_USERS, ":%I TOPIC %C :%s", si, c, c->topic);

	return 0;
}

static u_cmd tb_cmdtab[] = {
	{ "TB", SRC_SERVER, c_s_tb, 3, CMD_PROP_BROADCAST },
	{ }
};

MICRO_MODULE_V1(
	"core/tb", "Alex Iadicicco", "TB command",
	NULL, NULL, tb_cmdtab);
