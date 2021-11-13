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

#include <atomic>

#include "BKE_process_cancel.h"

static std::atomic<bool> g_is_cancelled = false;

void BKE_process_cancel_request(void)
{
  g_is_cancelled = true;
}

bool BKE_process_cancel_requested(void)
{
  return g_is_cancelled;
}

void BKE_process_cancel_continue(void)
{
  g_is_cancelled = false;
}
