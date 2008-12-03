/* Copyright 2008 Cedric Cellier.
 *
 * This file is part of Scambio.
 *
 * Scambio is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Scambio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Scambio.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SENDMAIL_H_081203
#define SENDMAIL_H_081203

#include <stdbool.h>
#include <time.h>
#include "scambio/channel.h"
#include "scambio/mdir.h"

extern bool terminate;
struct chn_cnx ccnx;

void forwarder_begin(void);
void forwarder_end(void);
void crawler_begin(void);
void crawler_end(void);

// Forwarder API consists of the management of the forward queues
TAILQ_HEAD(forwards, forward);
struct forward {
	// Description of the email
	char *from, *to, *subject;	// strduped
	// etc...
	// SMTP status
	struct forwards *list;	// the list we are on, or NULL if we are still constructed
	TAILQ_ENTRY(forward) entry;
	int status;	// 0 if still unknown
	mdir_version version;
	time_t submited;
};

struct forward *forward_new(mdir_version version, char const *from, char const *to, char const *subject);
void forward_part_new(struct forward *fwd, char const *resource);
void forward_submit(struct forward *fwd);
struct forward *forward_oldest_completed(void);
void forward_del(struct forward *fwd);

#endif
