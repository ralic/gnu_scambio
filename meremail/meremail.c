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
#include <stdlib.h>
#include <stdio.h>
#include <pth.h>
#include "meremail.h"
#include "auth.h"

/*
 * Data Definitions
 */

struct chn_cnx ccnx;
struct mdir_user *user;
struct mdir *outbox;

/*
 * Init
 */

void ccnx_init(void)
{
	if_fail (chn_begin(false)) return;
	atexit(chn_end);
	// TODO: we could also put the filed host/port on the resource line, and use a pool of ccnx ?
	conf_set_default_str("SC_FILED_HOST", "localhost");
	conf_set_default_str("SC_FILED_PORT", DEFAULT_FILED_PORT);
	on_error return;
	if_fail (chn_cnx_ctor_outbound(&ccnx, conf_get_str("SC_FILED_HOST"), conf_get_str("SC_FILED_PORT"), conf_get_str("SC_USERNAME"))) return;
}

int main(int nb_args, char *args[])
{
	if_fail (init("meremail", nb_args, args)) return EXIT_FAILURE;
	if_fail (maildir_init()) return EXIT_FAILURE;
	conf_set_default_str("SC_USERNAME", "Alice");
	if_fail (auth_begin()) return EXIT_FAILURE;
	atexit(auth_end);
	if_fail (user = mdir_user_load(conf_get_str("SC_USERNAME"))) return EXIT_FAILURE;
	char const *outbox_name = header_search(mdir_user_header(user), "smtp-outbox");
	on_error return EXIT_FAILURE;
	if (outbox_name) {
		if_fail (outbox = mdir_lookup(conf_get_str("SC_OUTBOX"))) return EXIT_FAILURE;
	} else {
		warning("No outbox defined : Cannot send");
	}
	if_fail (ccnx_init()) return EXIT_FAILURE;
	GtkWidget *folder_window = make_folder_window("/");
	if (! folder_window) return EXIT_FAILURE;
	exit_when_closed(folder_window);
	gtk_widget_show_all(folder_window);
	gtk_main();
	return EXIT_SUCCESS;
}
