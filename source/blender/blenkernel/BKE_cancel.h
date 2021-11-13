/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Try to stop a running operation so that the user can take back control over Blender which may be
 * frozen.
 */
void BKE_cancel_request(void);

/**
 * Return true when the caller should try to stop the processing it is doing as quickly as
 * possible to stop Blender from freezing. The caller should leave everything in a valid state
 * though.
 */
bool BKE_cancel_requested(void);

/**
 * Disable canceling again so that everything behaves normally.
 */
void BKE_cancel_continue(void);

#ifdef __cplusplus
}
#endif
