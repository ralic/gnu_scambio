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
#ifndef CONTACT_H_081226
#define CONTACT_H_081226

void contact_init(void);

struct field_dialog {
	GtkWidget *dialog;
	GtkWidget *cat_combo, *field_combo, *value_entry;
};

struct field_dialog *field_dialog_new(GtkWindow *parent, char const *cat_name, char const *field_name, char const *value);
void field_dialog_del(struct field_dialog *fd);

struct name_dialog {
	GtkWidget *dialog;
	GtkWidget *name_entry;
};

struct name_dialog *name_dialog_new(GtkWindow *parent, char const *default_name);
void name_dialog_del(struct name_dialog *);

#endif
