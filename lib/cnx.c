#include <netdb.h>
#include "scambio/cnx.h"

/*
 * Data Definitions
 */

struct sent_query {
	LIST_ENTRY(sent_query) def_entry;	// list head is in the definition of the query
	long long seqnum;
	void *user_data;
};

/*
 * Constructors for sent_query
 */

static void sent_query_ctor(struct sent_query *sq, struct mdir_cnx *cnx, struct query_def *def, long long seq, void *user_data)
{
	sq->seqnum = seq;
	sq->user_data = user_data;
	LIST_INSERT_HEAD(&def->sent_queries, sq, def_entry);
}

static void sent_query_dtor(struct sent_query *sq)
{
	LIST_REMOVE(sq, def_entry);
}

static struct sent_query *sent_query_new(struct mdir_cnx *cnx, struct query_def *def, long long seq, void *user_data)
{
	struct sent_query *sq = malloc(sizeof(*sq));
	if (! sq) with_error(ENOMEM, "malloc(sent_query)") return NULL;
	if_fail (sent_query_ctor(sq, cnx, def, seq, user_data)) {
		free(sq);
		sq = NULL;
	}
	return sq;
}

static void sent_query_del(struct sent_query *sq)
{
	sent_query_dtor(sq);
	free(sq);
}

/*
 * Register/Send a query
 */

static void query_def_ctor(struct query_def *qd, struct mdir_cnx *cnx, char const *keyword, mdir_cnx_cb *cb)
{
	LIST_INIT(&qd->sent_queries);
	qd->cb = cb;
	qd->def.cb = query_answer_cb;
	qd->def.keyword = keyword;
	qd->def.nb_arg_min = 1;	// the status
	qd->def.nb_arg_max = UINT_MAX;	// the complementary things
	qd->def.nb_types = 1;
	qd->def.types[0] = CMD_INTEGER;
	LIST_INSERT_HEAD(&cnx->query_defs, qd, cnx_entry);
}

static void query_def_dtor(struct query_def *qd)
{
	struct sent_query *sq;
	while (NULL != (sq = LIST_FIRST(&qd->sent_queries, def_entry))) {
		sent_query_del(sq);
	};
	LIST_REMOVE(qd, cnx_entry);
}

void mdir_cnx_register_query(struct mdir_cnx *cnx, char const *keyword, mdir_cnx_cb *cb, struct query_def *qd)
{
	// the user tell us he may send this query, so we must register the syntax
	// for the answer.
	query_def_ctor(qd, cnx, keyword, cb);
	unless_error mdir_syntax_register(&cnx->syntax, &qd->def);
}

void mdir_cnx_query(struct mdir_cnx *cnx, struct query_def *qd, bool answ, void *user_data, ...)
{
	struct varbuf vb;
	varbuf_ctor(&vb, 1024, true);
	long long seq = cnx->next_seq++;
	do {
		if (answ) {
			char buf[SEQ_BUF_LEN];
			if_fail (varbuf_append_strs(&vb, mdir_cmd_seq2str(buf, seq), " ", NULL)) break;
		}
		if_fail (varbuf_append_strs(&vb, qd->def.keyword, NULL)) break;
		va_list ap;
		va_start(ap, user_data);
		char const *param;
		while (NULL != (param = va_arg(ap))) {
			if_fail (varbuf_append_strs(&vb, " ", param, NULL)) break;
		}
		va_end(ap);
		on_error break;
		if_fail (varbuf_append_strs(&vb, "\n", NULL)) break;
		if_fail (Write(cnx->fd, vb.buf, vb.used-1)) break;	// do not output the final '\0'
		if_fail ((void)sent_query_new(cnx, qd, seq, user_data)) break;
	} while (0);
	varbuf_dtor(&vb);
}

/*
 * Constructors for mdir_cnx
 */

static struct mdir_cnx *cnx_alloc(void)
{
	struct mdir_cnx *cnx = malloc(sizeof(*cnx));
	if (! cnx) with_error(ENOMEM, "malloc(mdir_cnx)") return NULL;
	cnx->fd = -1;
	cnx->user = NULL;
	cnx->next_seq = 0;
	LIST_INIT(&cnx->query_defs);
	mdir_syntax_ctor(&cnx->syntax);
	return cnx;
}

static int gaierr2errno(int err)
{
	switch (err) {
		case EAI_SYSTEM: return errno;
		case EAI_MEMORY: return ENOMEM;
	}
	return -1;	// FIXME
}

static void cnx_connect(struct mdir_cnx *cnx, char const host, char const *service)
{
	// Resolve hostname into sockaddr
	struct addrinfo *info_head, *ainfo;
	int err;
	if (0 != (err = getaddrinfo(host, service, NULL, &info_head))) {
		// TODO: check that freeaddrinfo is not required in this case
		with_error(gaierr2errno(err), "Cannot getaddrinfo") return;
	}
	err = ENOENT;
	for (ainfo = info_head; ainfo; ainfo = ainfo->ai_next) {
		cnx->fd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
		if (cnx->fd == -1) continue;
		if (0 == connect(cnx->fd, ainfo->ai_addr, ainfo->ai_addrlen)) {
			info("Connected to %s:%s", host, service);
			break;
		}
		err = errno;
		(void)close(cnx->fd);
		cnx->fd = -1;
	}
	if (! ainfo) error_push(err, "No suitable address found for host %s:%s", host, service);
	freeaddrinfo(info_head);
}

static void auth_answ(char const *kw, int status, char const *compl, void *data)
{
	bool *done = (bool *)data;
	assert(kw == kw_auth);
	(void)compl;
	if (status == 200) *done = true;
}

void mdir_cnx_ctor_outbound(struct mdir_cnx *cnx, char const host, char const *service, char const *username)
{
	cnx->outbound = true;
	if_fail (cnx_connect(cnx, host, service)) return;
	do {
		bool done = false;
		if_fail (mdir_cnx_query(cnx, auth_answ, username, kw_auth, &done)) break;
		if_fail (mdir_cnx_read(cnx, NULL, NULL)) break;
		if (! done) with_error (0, "no answer to auth") break;
		cnx->user = user_load(username);
	} while (0);
	on_error {
		(void)close(cnx->fd);
		cnx->fd = -1;
	}
}

static void serve_auth(struct mdir_cmd *cmd, void *user_data)
{
	struct mdir_cnx *cnx = user_data;
	cnx->user = mdir_user_load(cmd.args[0].string);
	mdir_cnx_answer(cnx, cmd, is_error() ? 403:200, is_error() ? error_str():"OK");
}

void mdir_cnx_ctor_inbound(struct mdir_cnx *cnx, int fd, struct mdir_cmd_def *auth_def)
{
	cnx->outbound = false;
	cnx->user = NULL;
	cnx->fd = fd;
	// Now wait for user auth and set user or return error
	auth_def.keyword = kw_auth;
	auth_def.cb = serve_auth;
	auth_def.nb_arg_min = 1;
	auth_def.nb_arg_max = 1;
	auth_def.nb_types = 1;
	auth_def.types[0] = CMD_STRING;
	mdir_cnx_register_service(cnx, &auth_def);	// and this will stay registered. Identity is allowed to change within a connection.
	mdir_cnx_read(cnx);
	if (! cnx->user) with_error(0, "No auth received") return;
}

void mdir_cnx_dtor(struct mdir_cnx *cnx)
{
	if (cnx->fd != -1) {
		(void)close(cnx->fd);
		cnx->fd = -1;
	}
	// Free pending sent_queries by destroying the query_defs
	struct query_def *qd;
	while (NULL != (qd = LIST_FIRST(&cnx->query_defs))) {
		query_def_dtor(qd);
	}
	mdir_syntax_dtor(&cnx->syntax);
}

/*
 * Read
 */

void mdir_cnx_read(struct mdir_cnx *cnx)
{
	// New command will be handled by the user registered callback.
	// Query answers will be handled by our answer callback, which will then
	// now that the cmd->def is not merely a mdir_cmd_def but a query_def,
	// where it can look for the dedicated callback.
	mdir_cmd_read(cnx->syntax, cnx->fd, cnx);
}

