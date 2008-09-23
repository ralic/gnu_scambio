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
#ifndef COMMAND_H_080923
#define COMMAND_H_080923

// commands that were sent for which we wait an answer
struct command {
	LIST_ENTRY(command) entry;
	struct mdirc *mdirc;	// backlink
	char filename[PATH_MAX];	// associated file (for put/rem)
	long long seqnum;
	time_t creation;
};
enum command_type {
	SUB_CMD_TYPE, UNSUB_CMD_TYPE, PUT_CMD_TYPE, REM_CMD_TYPE, NB_CMD_TYPES
};
extern struct command_types {
	char const *const keyword;
	void (*finalize)(struct command *cmd, int status);
} command_types[NB_CMD_TYPES];

struct commands;
// give relative folder (ie mdir name) and absolute filename
struct command *command_new(enum command_type type, struct mdirc *mdirc, char const *folder, char const *filename);
void command_del(struct command *cmd);
struct command *command_get_by_seqnum(struct commands *list, long long seqnum);
bool command_timeouted(struct command *cmd);

#endif