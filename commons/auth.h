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
#ifndef AUTH_H_081011
#define AUTH_H_081011
/* The server needs to authenticate each user and the communication needs to be
 * crypted - communication which can be UDP.
 * Pre-shared key symetric cyphering is all we need.
 * Secrets are stored in a directory containing a file per key, named with the user
 * whom its the key (on the server as well as the client), so that it's easy to add
 * accounts.
 *
 * This common routines read this directory for the key, encrypt and decrypt
 * messages, and so on.
 * It would be a good idea to compress the flow _before_ encrypting.
 * Also, the same keys should serve to discuss with the file server(s), which then
 * must also know all users keys.
 */

#endif
