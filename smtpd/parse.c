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
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <ctype.h>
#include "scambio.h"
#include "smtpd.h"
#include "misc.h"
#include "scambio/header.h"
#include "mime.h"

#ifndef HAVE_STRNSTR
static char *strnstr(const char *big_, const char *little, size_t max_len)
{
	char *big = (char *)big_;	// here we have non-const strings
	assert(max_len <= strlen(big));
	char old_val = big[max_len];
	big[max_len] = '\0';
	debug("looking for %s in %s", little, big);
	char *ret = strstr(big, little);
	debug("found @ %p", ret);
	big[max_len] = old_val;
	return ret;
}
#endif

/*
 * Parse a mail from a file descriptor into a struct msg_tree
 */

static struct msg_tree *parse_mail_rec(char *msg, size_t size);

// We ignore preamble and epilogue.
static void parse_multipart(struct msg_tree *node, char *msg, size_t size, char *boundary)
{
	char *msg_end = msg+size;
	node->type = CT_MULTIPART;
	SLIST_INIT(&node->content.parts);
	size_t const boundary_len = strlen(boundary);
	bool last_boundary = false;
	bool first_boundary = true;
	// Go straight to the first boundary for first component
	do {
		char *delim_pos = strnstr(msg, boundary, msg_end-msg);
		if (! delim_pos) {
			with_error(0, "multipart boundary not found (%s)", boundary) return;
		}
		assert(delim_pos < msg_end);
		if (! first_boundary) {
			struct msg_tree *part = parse_mail_rec(msg, delim_pos - msg);
			unless_error SLIST_INSERT_HEAD(&node->content.parts, part, entry);
			error_clear();
		}
		first_boundary = false;
		msg = delim_pos + boundary_len;
		// boundary may be followed by "--" if it's the last one, then some optional spaces then CRLF.
		if (msg[0] == '-' && msg[1] == '-') {
			msg += 2;
			last_boundary = true;
		}
		while (isblank(*msg)) msg++;
		if (*msg != '\n') {	// CRLF are converted to '\n' by varbuf_read_line()
			with_error(0, "Badly formated multipart message : boundary not followed by new line (%s)", boundary) return;
		}
		msg ++;	// skip NL
	} while (msg < msg_end && !last_boundary);
}

static void decode_quoted(struct varbuf *vb, size_t size, char *msg)
{
	unsigned char quoted = 0;
	enum { READ_UNQUOTED, READ_QUOTE1, READ_QUOTE2 } state = READ_UNQUOTED;
	while (size > 0) {
		if (state == READ_UNQUOTED) {
			if (*msg == '=') {
				state = READ_QUOTE1;
				quoted = 0;
			} else {
				if_fail (varbuf_append(vb, 1, msg)) return;
			}
		} else {
			if (*msg >= '0' && *msg <= '9') {
				quoted = quoted*16 + *msg - '0';
			} else if (*msg >= 'A' && *msg <= 'F') {
				quoted = quoted*16 + 10 + *msg - 'A';
			} else {
				// Either a soft break or an error. In both cases, ignore.
				state = READ_UNQUOTED;
			}
			if (state == READ_QUOTE1) {
				state = READ_QUOTE2;
			} else if (state == READ_QUOTE2) {
				if_fail (varbuf_append(vb, 1, (char *)&quoted)) return;
				state = READ_UNQUOTED;
			}
		}
		msg++;
		size--;
	}
}

static void decode_base64(struct varbuf *vb, size_t size, char *msg)
{
	static const unsigned char b64[] = {
		['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,  ['E'] = 4,  ['F'] = 5, 
		['G'] = 6,  ['H'] = 7,  ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
		['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17,
		['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
		['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
		['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35,
		['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
		['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
		['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53,
		['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
		['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
  	};
	unsigned nb_vals = 0;
	unsigned nb_pads = 0;
	unsigned vals[4];
	while (size > 0) {
		unsigned const c = *msg;
		if (c == '=') {
			nb_pads ++;
			if (nb_pads > 2) with_error(0, "Invalid base64 encoding : too many padding") return;
		} else if (c > sizeof_array(b64)) {	// ignore
			debug("Invalid char '%c' in base64 encoding", c);
		} else {
			unsigned char val = b64[c];
			if (val == 0 && c != 'A') {	// ignore
				debug("Invalid char '%c' in base64 encoding", c);
			} else {
				if (nb_pads > 0) with_error(0, "Invalid base64 encoding : padding chars within body") return;
				vals[nb_vals++] = val;
			}
		}
		if (nb_vals + nb_pads >= sizeof_array(vals)) {	// output the result
			unsigned char output[3] = {
				(vals[0] << 2) | (vals[1] >> 4),
				(vals[1] << 4) | (vals[2] >> 2),
				(vals[2] << 6) | vals[3],
			};
			if_fail (varbuf_append(vb, sizeof(output)-nb_pads, output)) return;
			nb_vals = 0;
		}
		msg++;
		size--;
	}
}

static void decode_in_varbuf(struct varbuf *vb, size_t size, char *msg, struct header *header)
{
	struct header_field *encoding = header_find(header, "content-transfer-encoding", NULL);
	on_error return;
	if (encoding) {
		static const struct {
			char const *name;
			void (*decode)(struct varbuf *, size_t, char *);
		} possible_encodings[] = {
			{ "quoted-printable", decode_quoted },
			{ "base64", decode_base64 },
		};
		for (unsigned e=0; e<sizeof_array(possible_encodings); e++) {
			if (0 != strcasecmp(encoding->value, possible_encodings[e].name)) continue;
			debug("decoding %s, size = %zu", encoding->value, size);
			possible_encodings[e].decode(vb, size, msg);
			debug("output size : %zu", vb->used);
			return;
		}
	}
	// Nothing found, or no encoding : just copy
	varbuf_append(vb, size, msg);
}

static size_t append_param(char *params, size_t maxlen, size_t len, char const *pname, char const *pval, size_t plen)
{
	if (len >= maxlen) {
		warning("parameter string too short to fit all params");
		return len;
	}
	len += snprintf(params+len, maxlen-len, "%s%s=\"%.*s\"", len ? "; ":"", pname, (int)plen, pval);
	return len;
}

static void build_params(char *params, size_t maxlen, struct header *header)
{
	struct header_field *type = header_find(header, "content-type", NULL);
	struct header_field *disp = header_find(header, "content-disposition", NULL);
	char const *filename = NULL;
	char const *filetype = NULL;
	char buf[PATH_MAX];
	size_t len = 0, plen = 0;
	if (disp) {
		plen = parameter_find(disp->value, "filename", &filename);
		on_error {
			filename = NULL;
			error_clear();	// lets try something else
		}
	}
	if (! filename && type) {
		plen = parameter_find(type->value, "name", &filename);
		on_error {
			filename = NULL;
			error_clear();
		}
	}
	if (! filename && type) {	// build from scratch based on type
		char const *ext = mime_type2ext(type->value);
		plen = snprintf(buf, sizeof(buf), "noname.%s", ext ? ext:"bin");
		filename = buf;
	}
	if (! filename) {
		filename = "noname";
		plen = strlen(filename);
	}
	len += append_param(params, maxlen, len, "name", filename, plen);
	if (type) {	// take the whole value including charset etc...
		// TODO: be more selective
		filetype = type->value;
	}
	if (filetype) {
		len += append_param(params, maxlen, len, "type", type->value, strlen(type->value));
	}
}

static void parse_mail_node(struct msg_tree *node, char *msg, size_t size)
{
	// First read the header
	if_fail (node->header = header_new()) return;
	size_t header_size;
	if_fail (header_size = header_parse(node->header, msg)) return;
	assert(header_size <= size);
	// Find out weither the body is total with a decoding method, or
	// is another message up to a given boundary.
	struct header_field *content_type = header_find(node->header, "content-type", NULL);
	if (content_type && 0 == strncasecmp(content_type->value, "multipart/", 10)) {
		debug("message is multipart");
#		define PREFIX "\n--"
#		define PREFIX_LENGTH 3
		char boundary[PREFIX_LENGTH+MAX_BOUNDARY_LENGTH] = PREFIX;
		char *b = parameter_extract(content_type->value, "boundary");
		on_error return;
		if (b) {
			snprintf(boundary+PREFIX_LENGTH, sizeof(boundary)-PREFIX_LENGTH, "%s", b);
			free(b);
			parse_multipart(node, msg, size, boundary);
			return;
		} else {
			warning("multipart message without boundary ?");	// proceed as a single file
		}
	}
	// Process mail as a single file
	debug("message is a single file");
	// read the file in node->content.file
	build_params(node->content.file.params, sizeof(node->content.file.params), node->header);
	if_fail (varbuf_ctor(&node->content.file.data, 1024, true)) return;
	node->type = CT_FILE;
	if (msg[header_size] == '\n') header_size++;	// A SMTP header is supposed to be ended because of en empty line that we dont want on the file
	if_fail (decode_in_varbuf(&node->content.file.data, size-header_size, msg+header_size, node->header)) return;
}

static void msg_tree_dtor(struct msg_tree *node)
{
	if (node->header) {
		header_unref(node->header);
		node->header = NULL;
	}
	switch (node->type) {
		case CT_NONE:
			break;
		case CT_FILE:
			varbuf_dtor(&node->content.file.data);
			break;
		case CT_MULTIPART:
			{
				struct msg_tree *sub, *tmp;
				SLIST_FOREACH_SAFE(sub, &node->content.parts, entry, tmp) {
					msg_tree_del(sub);
				}
			}
			break;
	}
	node->type = CT_NONE;
}

void msg_tree_del(struct msg_tree *node)
{
	msg_tree_dtor(node);
	free(node);
}

// do not look msg after size
static struct msg_tree *parse_mail_rec(char *msg, size_t size)
{
	struct msg_tree *node = calloc(1, sizeof(*node));
	if (! node) with_error(ENOMEM, "calloc mail") return NULL;
	if_fail (parse_mail_node(node, msg, size)) {
		msg_tree_del(node);
		node = NULL;
	}
	return node;
}

static void read_whole_mail(struct varbuf *vb, int fd)
{
	debug("read_whole_mail(vb=%p, fd=%d)", vb, fd);
	char *line;
	do {
		varbuf_read_line(vb, fd, MAX_MAILLINE_LENGTH, &line);
		on_error break;
		if (line_match(line, ".")) {	// chop this line
			varbuf_cut(vb, line);
			break;
		}
	} while (1);
}

struct msg_tree *msg_tree_read(int fd)
{
	struct msg_tree *root = NULL;
	struct varbuf vb;
	if_fail (varbuf_ctor(&vb, 10240, true)) return NULL;
	read_whole_mail(&vb, fd);
	unless_error root = parse_mail_rec(vb.buf, vb.used);
	varbuf_dtor(&vb);
	return root;
}

