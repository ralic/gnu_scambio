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
/*
 * Interface to folders.
 * Beware that many threads may access those files concurrently,
 * although they are non preemptibles.
 */
#ifndef MDIR_H_080912
#define MDIR_H_080912

#include <stdbool.h>
#include <pth.h>
#include <limits.h>
#include <inttypes.h>
#include <scambio/queue.h>

enum mdir_action { MDIR_ADD, MDIR_REM };

typedef uint64_t mdir_version;
#define PRIversion PRIu64
#define MDIR_VERSION_G_TYPE G_TYPE_UINT64	// must include gtk.h to use this

// Do not use these but inherit from them
struct jnl {
	STAILQ_ENTRY(jnl) entry;
	int patch_fd;	// to the patch file
	int idx_fd;	// to the index file
	mdir_version version;	// version of the first patch in this journal
	unsigned nb_patches;	// number of patches in this file
	struct mdir *mdir;
};
struct mdir {
	LIST_ENTRY(mdir) entry;	// entry in the list of cached dirs
	STAILQ_HEAD(jnls, jnl) jnls;	// list all jnl in this directory (refreshed from time to time), ordered by first_version
	pth_rwlock_t rwlock;
	char path[PATH_MAX];	// absolute path to the dir (actual one, not one of the symlinks)
};

// provides these allocators for previous structures (default ones being malloc/free)
extern struct jnl *(*jnl_alloc)(void);
extern void (*jnl_free)(struct jnl *);
extern struct mdir *(*mdir_alloc)(void);
extern void (*mdir_free)(struct mdir *);

struct header;
void mdir_begin(void);
void mdir_end(void);

// add/remove a header into a mdir
// do not use this in plugins : only the server decides how and when to apply a patch
// plugins use mdir_patch_request instead
// returns the new version number
mdir_version mdir_patch(struct mdir *, enum mdir_action, struct header *);

// Ask for the addition of this patch to the mdir. Actually the patch will be
// saved in a tempfile in subfolder ".tmp" with a tempname starting with '+'
// for addition and '-' for removal of the herein header. There it will be
// found by the client synchronizer mdirc (because mdir_list will report them
// as unsynched patches) and sent to the server whenever possible, then when acked
// removed to another class of tempfile, which name is "+/-=version" (the
// version is received with the ack).  mdir_list will also report these acked
// files if they are not already in the journal (if they are it will silently
// remove them).  This also works if the patch is received before the ack
// (which may happen on some transport even if the server sends the ack before
// the patch).
//
// If the patch creates/removes a subfolder to an existing dirId, the patch is
// added as above, but also the symlink is performed so that following lookups
// will work even when not connected. Later when the patch will be eventually
// received it is thus not an error if the symlink already exist.
//
// If the patch adds a _new_ subfolder (no dirId specified), a transient dirId
// is created and symlinked as above. This transient dirId uses a "_" prefix.
// When the patch is eventually received, the client will look if a symlink with
// the same name already exists. If so, it will rename the dirId to the
// server's one, and rebuilt the symlink.
//
// If two clients propose a patch for the addition of the same new directory
// (same name), then one of them will receive a non fatal error, which does not
// prevent him from adding patches to it (since it does exist). Both directories
// will be the same, and content de facto merged.
//
// Only one symlink refers to this transient dirId because it is forbidden to
// use a temporary dirId (this dirId being unknown for the server). So, it is
// forbidden to link a temporary dirId into another folder, and patch_request
// will prevent this to happen.  This should not be too problematic for the
// client, since it still can add patches to this directory, even patches that
// creates other directories, for patches are added to names and not to dirId.
void mdir_patch_request(struct mdir *, enum mdir_action, struct header *);
// This is a helper to request a patch that will remove the patch identified
// with this version
void mdir_del_request(struct mdir *mdir, mdir_version to_del);

// abort a patch request if its not already aborted.
// If the patch found is of type: dir, unlink also the dir from its parent.
void mdir_patch_request_abort(struct mdir *, enum mdir_action, mdir_version version);

// returns the mdir for this name (relative to mdir_root, must exists)
struct mdir *mdir_lookup(char const *name);
struct mdir *mdir_lookup_by_id(char const *id, bool create);

// list all patches of a mdir
// (will also list unconfirmed patches)
union mdir_list_param {
	mdir_version version;	// if not new (ie. not on server yet)
	char const *path;	// if new
};
void mdir_patch_list(struct mdir *, mdir_version from, bool new_only, void (*cb)(struct mdir *, struct header *, enum mdir_action action, bool new, union mdir_list_param, void *data), void *data);

// returns only the symlinks.
void mdir_folder_list(struct mdir *, bool new_only, void (*cb)(struct mdir *parent, struct mdir *child, bool new, char const *name, void *data), void *data);

// returns the header, action and version following the given version
// or NULL if no other patches are found
struct header *mdir_read_next(struct mdir *, mdir_version *, enum mdir_action *);

// returns the last version of this mdir
mdir_version mdir_last_version(struct mdir *);
char const *mdir_id(struct mdir *);
// use a static buffer
char const *mdir_version2str(mdir_version);
mdir_version mdir_str2version(char const *str);
char const *mdir_action2str(enum mdir_action action);
enum mdir_action mdir_str2action(char const *str);
bool mdir_is_transient(struct mdir *mdir);
unsigned mdir_size(struct mdir *);

#endif
