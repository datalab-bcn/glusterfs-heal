/*
  Copyright (c) 2012-2013 DataLab, S.L. <http://www.datalab.es>

  This file is part of the features/heal translator for GlusterFS.

  The features/heal translator for GlusterFS is free software: you can
  redistribute it and/or modify it under the terms of the GNU General
  Public License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  The features/heal translator for GlusterFS is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE. See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the features/heal translator for GlusterFS. If not, see
  <http://www.gnu.org/licenses/>.
*/

#ifndef __HEAL_H__
#define __HEAL_H__

#include <mem-types.h>

#define HEAL_KEY_FLAGS "trusted.heal.flags"
#define HEAL_KEY_SIZE  "trusted.heal.size"

enum gf_heal_mem_types_
{
    gf_heal_mt_heal_inode_ctx_t = gf_common_mt_end + 1,
    gf_heal_mt_heal_fd_ctx_t,
    gf_heal_mt_uint8_t,
    gf_heal_mt_end
};

#endif /* __HEAL_H__ */
