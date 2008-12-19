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
#include <assert.h>
#include <string.h>
#include <glib.h>
#include <gtkhtml/gtkhtml.h>
#include "merelib.h"
#include "meremail.h"
#include "varbuf.h"
#include "misc.h"

/*
 * Make a Window to display a mail
 */

// type is allowed to be NULL
static GtkWidget *make_view_widget(char const *type, char const *resource, GtkWindow *win)
{
	debug("type='%s', resource='%s'", type, resource);
	GtkWidget *widget = NULL;
	struct varbuf vb;
	// First try to get the resource
	char filename[PATH_MAX];
	if_fail (chn_get_file(&ccnx, filename, resource)) {
		widget = gtk_label_new(NULL);
		char *str = g_markup_printf_escaped("<b>Cannot fetch remote resource</b> <i>%s</i> : %s", resource, error_str());
		gtk_label_set_markup(GTK_LABEL(widget), str);
		g_free(str);
		error_clear();	// replace by an error text
		goto q;
	}
	wait_all_tx(&ccnx, win);
	// For text files, display in a text widget (after utf8 conversion)
	// For html, use GtkHtml,
	// For image, display as an image
	// For other types, display a launcher for external app
	if (type && 0 == strncmp(type, "text/html", 9)) {
		if_fail (varbuf_ctor_from_file(&vb, filename)) return NULL;
		widget = gtk_html_new_from_string(vb.buf, vb.used);
		varbuf_dtor(&vb);
	} else if (type && 0 == strncmp(type, "text/", 5)) {	// all other text types are treated the same
		GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
		char *charset = parameter_extract(type, "charset");
		if (! charset) charset = Strdup("us-ascii");
		do {
			if_fail (varbuf_ctor_from_file(&vb, filename)) break;
			// convert to UTF-8
			gsize utf8size;
			gchar *utf8text = g_convert_with_fallback(vb.buf, vb.used, "utf-8", charset, NULL, NULL, &utf8size, NULL);
			varbuf_dtor(&vb);
			if (! utf8text) with_error(0, "Cannot convert from '%s' to utf8", charset) break;
			gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), utf8text, utf8size);
			widget = gtk_text_view_new_with_buffer(buffer);
			g_object_unref(G_OBJECT(buffer));
			g_free(utf8text);
		} while (0);
		free(charset);
		on_error return NULL;
	} else if (type && 0 == strncmp(type, "image/", 6)) {
		widget = gtk_image_new_from_file(filename);	// Oh! All mighty gtk !
	}
	if (! widget) {
		widget = gtk_label_new(NULL);
		char *str = g_markup_printf_escaped("<b>Cannot display this part</b> which have type '<i>%s</i>'", type ? type:"NONE");
		gtk_label_set_markup(GTK_LABEL(widget), str);
		g_free(str);
	}
q:	return make_scrollable(widget);
}

GtkWidget *make_mail_window(struct msg *msg)
{
	/* The mail view is merely a succession of all the attached files,
	 * preceeded with our mail patch.
	 * It would be great if some of the files be closed by default
	 * (for the first one, the untouched headers, is not of much use).
	 */
	debug("for msg %s", msg->descr);
	GtkWidget *win = make_window(NULL, NULL);
	enum mdir_action action;
	struct header *h = mdir_read(&msg->maildir->mdir, msg->version, &action);
	on_error return NULL;
	assert(action == MDIR_ADD);

	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win), vbox);

	// The header
	GtkWidget *title = gtk_label_new(NULL);
	char *title_str = g_markup_printf_escaped(
		"<b>From</b> <i>%s</i>, <b>Received on</b> <i>%s</i>\n"
		"<b>Subject :</b> %s",
		msg->from, ts2staticstr(msg->date), msg->descr);
	gtk_label_set_markup(GTK_LABEL(title), title_str);
	gtk_misc_set_alignment(GTK_MISC(title), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(title), TRUE);
#	if TRUE == GTK_CHECK_VERSION(2, 10, 0)
	gtk_label_set_line_wrap_mode(GTK_LABEL(title), PANGO_WRAP_WORD_CHAR);
#	endif
	g_free(title_str);
	gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

	GtkWidget *notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
	// A Tab for each resources, except for the header which is put in an expander above
	struct header_field *resource = NULL;
	while (NULL != (resource = header_find(h, SC_RESOURCE_FIELD, resource))) {
		char *resource_stripped = parameter_suppress(resource->value);
		char *title = parameter_extract(resource->value, "name");
		bool is_header = !title;
		char *type = parameter_extract(resource->value, "type");
		GtkWidget *widget = make_view_widget(type, resource_stripped, GTK_WINDOW(win));
		if (is_header) {
			gtk_box_pack_start(GTK_BOX(vbox), make_expander("Headers", widget), FALSE, FALSE, 0);
		} else {
			(void)gtk_notebook_append_page(GTK_NOTEBOOK(notebook), widget, gtk_label_new(title ? title:"Untitled"));
		}
		FreeIfSet(&title);
		FreeIfSet(&type);
		FreeIfSet(&resource_stripped);
	}
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	GtkWidget *toolbar = make_toolbar(3,
		GTK_STOCK_JUMP_TO, NULL,     NULL,	// Forward
		GTK_STOCK_DELETE,  NULL,     NULL,	// Delete
		GTK_STOCK_QUIT,    close_cb, win);

	gtk_box_pack_end(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	header_unref(h);
	return win;
}
