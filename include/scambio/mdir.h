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

struct header;
void mdir_begin(bool server);
void mdir_end(void);

// create a new mdir (you will want to mdir_link() it somewhere)
struct mdir *mdir_create(void);

// add/remove a header into a mdir
// do not use this in plugins : only the server decides how and when to apply a patch
// plugins use mdir_patch_request instead
// returns the new version number
mdir_version mdir_patch(struct mdir *, enum mdir_action, struct header *);

// ask for the addition of this patch to the mdir
// actually the patch will be saved in a tempfile where it will be found by the client
// synchronizer mdirc, sent to the server whenever possible, then when acked removed to a
// doted name with the version (received with the ack). Then, when the patch is
// eventually synchronized down the dotted version file is removed.
// So this does not require a connection to mdird.
// The only problem arises when the patch is received before the ack. Even if the server
// sends the ack before the patch, the packets may be received the other way round.
// So it's better to remove those dotted version files when a client ask for a listing (then
// we know if the dotted version file is also in the journal and delete it.
// If the patch creates/remove a subfolder for an existing dirId, the patch is added as above,
// but also the symlink is performed so that the lookup works. Later when the patch is
// received it is thus not an error if the symlink is already there.
// If the patch adds a new subfolder (no dirId specified), a temporary dirId is created
// and symlinked as above. When the patch is eventually received, the directory is renamed
// to the actual dirId provided with the patch, and the symlink is rebuild. No other
// symlinks refer to this temporary dirId because it is forbidden to use a temporary dirId
// (this dirId being unknown for the server). So, it is forbidden to link a temporary dirId
// into another folder, and patch_request will cprevent this to happen. This should
// not be too problematic for the client, since it still can add patches to this directory,
// enven patches that creates other directories, for patches are added to names and not to
// dirId.
void mdir_patch_request(struct mdir *, enum mdir_action, struct header *);

// abort a patch request if its not already aborted.
// If the patch found is of type: dir, unlink also the dir from its parent.
void mdir_patch_request_abort(struct mdir *, enum mdir_action, mdir_version version);

// listeners
// will be called whenever a patch is applied to the mdir
struct mdir_listener {
	struct mdir_listener_ops {
		void (*del)(struct mdir_listener *, struct mdir *);	// must unregister if its registered
		void (*notify)(struct mdir_listener *, struct mdir *, struct header *h);
	} const *ops;
	LIST_ENTRY(mdir_listener) entry;
};
static inline void mdir_listener_ctor(struct mdir_listener *l, struct mdir_listener_ops const *ops)
{
	l->ops = ops;
}
static inline void mdir_listener_dtor(struct mdir_listener *l) { (void)l; }
void mdir_register_listener(struct mdir *mdir, struct mdir_listener *l);
void mdir_unregister_listener(struct mdir *mdir, struct mdir_listener *l);

// returns the mdir for this name (relative to mdir_root, must exists)
struct mdir *mdir_lookup(char const *name);
struct mdir *mdir_lookup_by_id(char const *id, bool create);

// list all patches of a mdir
// (will also list unconfirmed patches)
void mdir_patch_list(struct mdir *, bool confirmed, bool unconfirmed, void (*cb)(struct mdir *, struct header *, enum mdir_action action, bool confirmed, mdir_version version));

// returns the header, action and version following the given version
// or NULL if no other patches are found
struct header *mdir_read_next(struct mdir *, mdir_version *, enum mdir_action *);

// returns the last version of this mdir
mdir_version mdir_last_version(struct mdir *);
char const *mdir_id(struct mdir *);
// use a static buffer
char const *mdir_version2str(mdir_version);
mdir_version mdir_str2version(char const *str);

#endif
