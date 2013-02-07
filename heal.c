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

#include <xlator.h>
#include <defaults.h>

#include "heal.h"
#include "heal-type-dict.h"

typedef struct _heal_inode_ctx
{
    int32_t healing;
    uint64_t size;
    uint64_t offset;
} heal_inode_ctx_t;

typedef struct _heal_fd_ctx
{
    int32_t healing;
} heal_fd_ctx_t;

int32_t __heal_inode_ctx_get(heal_inode_ctx_t ** ctx, xlator_t * xl, inode_t * inode)
{
    uint64_t value;

    if ((__inode_ctx_get(inode, xl, &value) != 0) || (value == 0))
    {
        return EIO;
    }

    *ctx = (heal_inode_ctx_t *)(uintptr_t)value;

    return 0;
}

int32_t heal_inode_ctx_get(heal_inode_ctx_t ** ctx, xlator_t * xl, inode_t * inode)
{
    int32_t error;

    LOCK(&inode->lock);

    error = __heal_inode_ctx_get(ctx, xl, inode);

    UNLOCK(&inode->lock);

    if (error != 0)
    {
        gf_log(xl->name, GF_LOG_ERROR, "Inode context not defined");
    }

    return error;
}

int32_t heal_inode_ctx_new(heal_inode_ctx_t ** ctx, xlator_t * xl, inode_t * inode, int32_t healing, uint64_t size)
{
    uint64_t value;
    int32_t error;

    LOCK(&inode->lock);

    error = __heal_inode_ctx_get(ctx, xl, inode);
    if (error != 0)
    {
        *ctx = GF_MALLOC(sizeof(heal_inode_ctx_t), gf_heal_mt_heal_inode_ctx_t);
        if (*ctx != NULL)
        {
            (*ctx)->healing = healing;
            (*ctx)->size = size;
            (*ctx)->offset = 0;
            value = (uint64_t)(uintptr_t)*ctx;
            if (__inode_ctx_put(inode, xl, value) != 0)
            {
                error = EIO;
            }
            else
            {
                error = 0;
            }
        }
        else
        {
            error = ENOMEM;
        }
    }

    UNLOCK(&inode->lock);

    return error;
}

void heal_inode_clear_healing(xlator_t * xl, inode_t * inode)
{
    heal_inode_ctx_t * ctx;
    int32_t error;

    LOCK(&inode->lock);

    error = __heal_inode_ctx_get(&ctx, xl, inode);
    if (error == 0)
    {
        ctx->healing = 0;
    }

    UNLOCK(&inode->lock);
}

int32_t __heal_fd_ctx_get(heal_fd_ctx_t ** ctx, xlator_t * xl, fd_t * fd)
{
    uint64_t value;

    if ((__fd_ctx_get(fd, xl, &value) != 0) || (value == 0))
    {
        return EIO;
    }

    *ctx = (heal_fd_ctx_t *)(uintptr_t)value;

    return 0;
}

int32_t heal_fd_ctx_get(heal_fd_ctx_t ** ctx, xlator_t * xl, fd_t * fd)
{
    int32_t error;

    LOCK(&fd->lock);

    error = __heal_fd_ctx_get(ctx, xl, fd);

    UNLOCK(&fd->lock);

    return error;
}

int32_t heal_fd_ctx_new(heal_fd_ctx_t ** ctx, xlator_t * xl, fd_t * fd)
{
    uint64_t value;
    int32_t error;

    LOCK(&fd->lock);

    error = __heal_fd_ctx_get(ctx, xl, fd);
    if (error != 0)
    {
        *ctx = GF_MALLOC(sizeof(heal_fd_ctx_t), gf_heal_mt_heal_fd_ctx_t);
        if (*ctx != NULL)
        {
            value = (uint64_t)(uintptr_t)*ctx;
            if (__fd_ctx_set(fd, xl, value) != 0)
            {
                GF_FREE(*ctx);
                error = EIO;
            }
            else
            {
                error = 0;
            }
        }
        else
        {
            error = ENOMEM;
        }
    }

    UNLOCK(&fd->lock);

    return error;
}

int32_t heal_inode_ctx_check(xlator_t * xl, inode_t * inode)
{
    heal_inode_ctx_t * ctx;
    int32_t error;

    if (inode->ia_type != IA_IFREG)
    {
        return 0;
    }

    error = heal_inode_ctx_get(&ctx, xl, inode);
    if (error == 0)
    {
        if (ctx->healing == 0)
        {
            return 0;
        }

        error = EPERM;
    }

    return error;
}

int32_t heal_inode_ctx_check_range(xlator_t * xl, inode_t * inode, uint64_t start, uint64_t end)
{
    heal_inode_ctx_t * ctx;
    int32_t error;

    if (inode->ia_type != IA_IFREG)
    {
        return 0;
    }

    error = heal_inode_ctx_get(&ctx, xl, inode);
    if (error == 0)
    {
        if ((ctx->healing == 0) || (end <= ctx->offset) || (start >= ctx->size))
        {
            return 0;
        }

        error = EPERM;
    }

    return error;
}

int32_t heal_access(call_frame_t * frame, xlator_t * xl, loc_t * loc, int32_t mask, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check(xl, loc->inode);
    if (error == 0)
    {
        STACK_WIND(frame, default_access_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->access, loc, mask, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(access, frame, -1, error, NULL);

    return 0;
}

int32_t heal_create_cbk(call_frame_t * frame, void * cookie, xlator_t * xl, int32_t result, int32_t code, fd_t * fd, inode_t * inode, struct iatt * attr, struct iatt * attr_ppre, struct iatt * attr_ppost, dict_t * xdata)
{
    inode_t * base;
    heal_fd_ctx_t * fd_ctx;
    int32_t error;

    base = cookie;
    if (base != NULL)
    {
        if ((result < 0) || (inode != base))
        {
            gf_log(xl->name, GF_LOG_WARNING, "inode changed in create");

            heal_inode_clear_healing(xl, base);
        }
        if (result >= 0)
        {
            error = heal_fd_ctx_new(&fd_ctx, xl, fd);
            if (error != 0)
            {
                code = error;
                result = -1;
            }
            else
            {
                fd_ctx->healing = 1;
            }
        }
    }

    STACK_UNWIND_STRICT(create, frame, result, code, fd, inode, attr, attr_ppre, attr_ppost, xdata);

    if (base != NULL)
    {
        inode_unref(base);
    }

    return 0;
}

int32_t heal_create(call_frame_t * frame, xlator_t * xl, loc_t * loc, int32_t flags, mode_t mode, mode_t umask, fd_t * fd, dict_t * xdata)
{
    heal_inode_ctx_t * ctx;
    uint64_t size;
    uint32_t value;
    int32_t healing, error;

    error = 0;
    healing = 0;
    size = 0;
    if (xdata != NULL)
    {
        if (heal_dict_get_uint32(xdata, HEAL_KEY_FLAGS, &value) == 0)
        {
            if (value != 0)
            {
                if (heal_dict_get_uint64(xdata, HEAL_KEY_SIZE, &size) == 0)
                {
                    healing = 1;
                }
                else
                {
                    error = EIO;
                }
            }
        }
    }
    gf_log(xl->name, GF_LOG_DEBUG, "Heal create: %u, error=%d", healing, error);
    if (error == 0)
    {
        error = heal_inode_ctx_new(&ctx, xl, loc->inode, healing, size);
        if (error == 0)
        {
            STACK_WIND_COOKIE(frame, heal_create_cbk, healing ? inode_ref(loc->inode) : NULL, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->create, loc, flags, mode, umask, fd, xdata);

            return 0;
        }
    }

    STACK_UNWIND_STRICT(create, frame, -1, error, NULL, NULL, NULL, NULL, NULL, NULL);

    return 0;
}

int32_t heal_getxattr(call_frame_t * frame, xlator_t * xl, loc_t * loc, const char * name, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check(xl, loc->inode);
    if (error == 0)
    {
        STACK_WIND(frame, default_getxattr_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->getxattr, loc, name, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(getxattr, frame, -1, error, NULL, NULL);

    return 0;
}

int32_t heal_fgetxattr(call_frame_t * frame, xlator_t * xl, fd_t * fd, const char * name, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check(xl, fd->inode);
    if (error == 0)
    {
        STACK_WIND(frame, default_fgetxattr_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->fgetxattr, fd, name, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(fgetxattr, frame, -1, error, NULL, NULL);

    return 0;
}

int32_t heal_lookup(call_frame_t * frame, xlator_t * xl, loc_t * loc, dict_t * xdata)
{
    heal_inode_ctx_t * ctx;
    int32_t error;

    error = heal_inode_ctx_new(&ctx, xl, loc->inode, 0, 0);
    if (error == 0)
    {
        STACK_WIND(frame, default_lookup_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->lookup, loc, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(lookup, frame, -1, error, NULL, NULL, NULL, NULL);

    return 0;
}

int32_t heal_rchecksum(call_frame_t * frame, xlator_t * xl, fd_t * fd, off_t offset, int32_t len, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check_range(xl, fd->inode, offset, offset + len);
    if (error == 0)
    {
        STACK_WIND(frame, default_rchecksum_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->rchecksum, fd, offset, len, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(rchecksum, frame, -1, error, 0, NULL, NULL);

    return error;
}

int32_t heal_readv(call_frame_t * frame, xlator_t * xl, fd_t * fd, size_t size, off_t offset, uint32_t flags, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check_range(xl, fd->inode, offset, offset + size);
    if (error == 0)
    {
        STACK_WIND(frame, default_readv_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->readv, fd, size, offset, flags, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(readv, frame, -1, error, NULL, 0, NULL, NULL, NULL);

    return error;
}

int32_t heal_stat(call_frame_t * frame, xlator_t * xl, loc_t * loc, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check(xl, loc->inode);
    if (error == 0)
    {
        STACK_WIND(frame, default_stat_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->stat, loc, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(stat, frame, -1, error, NULL, NULL);

    return 0;
}

int32_t heal_fstat(call_frame_t * frame, xlator_t * xl, fd_t * fd, dict_t * xdata)
{
    int32_t error;

    error = heal_inode_ctx_check(xl, fd->inode);
    if (error == 0)
    {
        STACK_WIND(frame, default_fstat_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->fstat, fd, xdata);

        return 0;
    }

    STACK_UNWIND_STRICT(fstat, frame, -1, error, NULL, NULL);

    return 0;
}

int32_t heal_truncate_cbk(call_frame_t * frame, void * cookie, xlator_t * xl, int32_t result, int32_t code, struct iatt * attr_pre, struct iatt * attr_post, dict_t * xdata)
{
    inode_t * inode;
    heal_inode_ctx_t * inode_ctx;
    int32_t error;

    inode = cookie;
    if (result >= 0)
    {
        LOCK(&inode->lock);

        error = __heal_inode_ctx_get(&inode_ctx, xl, inode);
        if (error == 0)
        {
            if (inode_ctx->healing == 1)
            {
                if (inode_ctx->size > attr_post->ia_size)
                {
                    inode_ctx->size = attr_post->ia_size;
                }
            }
        }
        else
        {
            code = error;
            result = -1;
        }

        UNLOCK(&inode->lock);
    }

    inode_unref(inode);

    STACK_UNWIND_STRICT(truncate, frame, result, code, attr_pre, attr_post, xdata);

    return 0;
}

int32_t heal_truncate(call_frame_t * frame, xlator_t * xl, loc_t * loc, off_t offset, dict_t * xdata)
{
    STACK_WIND_COOKIE(frame, heal_truncate_cbk, inode_ref(loc->inode), FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->truncate, loc, offset, xdata);

    return 0;
}

int32_t heal_ftruncate_cbk(call_frame_t * frame, void * cookie, xlator_t * xl, int32_t result, int32_t code, struct iatt * attr_pre, struct iatt * attr_post, dict_t * xdata)
{
    inode_t * inode;
    heal_inode_ctx_t * inode_ctx;
    int32_t error;

    inode = cookie;
    if (result >= 0)
    {
        LOCK(&inode->lock);

        error = __heal_inode_ctx_get(&inode_ctx, xl, inode);
        if (error == 0)
        {
            if (inode_ctx->healing == 1)
            {
                if (inode_ctx->size > attr_post->ia_size)
                {
                    inode_ctx->size = attr_post->ia_size;
                }
            }
        }
        else
        {
            code = error;
            result = -1;
        }

        UNLOCK(&inode->lock);
    }

    inode_unref(inode);

    STACK_UNWIND_STRICT(ftruncate, frame, result, code, attr_pre, attr_post, xdata);

    return 0;
}

int32_t heal_ftruncate(call_frame_t * frame, xlator_t * xl, fd_t * fd, off_t offset, dict_t * xdata)
{
    STACK_WIND_COOKIE(frame, heal_ftruncate_cbk, inode_ref(fd->inode), FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->ftruncate, fd, offset, xdata);

    return 0;
}

int32_t heal_unlink_cbk(call_frame_t * frame, void * cookie, xlator_t * xl, int32_t result, int32_t code, struct iatt * attr_ppre, struct iatt * attr_ppost, dict_t * xdata)
{
    inode_t * inode;
    heal_inode_ctx_t * inode_ctx;
    int32_t error;

    inode = cookie;
    if (result >= 0)
    {
        LOCK(&inode->lock);

        error = __heal_inode_ctx_get(&inode_ctx, xl, inode);
        if (error == 0)
        {
            // TODO: if the removed link was the last one, any healing in
            //       progress should be cancelled.
        }
        else
        {
            code = error;
            result = -1;
        }

        UNLOCK(&inode->lock);
    }

    inode_unref(inode);

    STACK_UNWIND_STRICT(unlink, frame, result, code, attr_ppre, attr_ppost, xdata);

    return 0;
}

int32_t heal_unlink(call_frame_t * frame, xlator_t * xl, loc_t * loc, int xflags, dict_t * xdata)
{
    STACK_WIND_COOKIE(frame, heal_unlink_cbk, inode_ref(loc->inode), FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->unlink, loc, xflags, xdata);

    return 0;
}

int32_t heal_writev_cbk(call_frame_t * frame, void * cookie, xlator_t * xl, int32_t result, int32_t code, struct iatt * attr_pre, struct iatt * attr_post, dict_t * xdata)
{
    heal_inode_ctx_t * inode_ctx;
    inode_t * inode;
    int32_t error;

    inode = cookie;
    if (result >= 0)
    {
        LOCK(&inode->lock);

        error = __heal_inode_ctx_get(&inode_ctx, xl, inode);
        if (error == 0)
        {
            inode_ctx->offset += result;
        }
        else
        {
            code = error;
            result = -1;
        }

        UNLOCK(&inode->lock);
    }
    inode_unref(inode);

    STACK_UNWIND_STRICT(writev, frame, result, code, attr_pre, attr_post, xdata);

    return 0;
}

int32_t heal_writev(call_frame_t * frame, xlator_t * xl, fd_t * fd, struct iovec * vector, int32_t count, off_t offset, uint32_t flags, struct iobref * iobref, dict_t * xdata)
{
    heal_inode_ctx_t * inode_ctx;
    heal_fd_ctx_t * fd_ctx;
    int32_t error, fd_healing;

    LOCK(&fd->inode->lock);

    fd_healing = 0;
    error = __heal_inode_ctx_get(&inode_ctx, xl, fd->inode);
    if (error == 0)
    {
        if (heal_fd_ctx_get(&fd_ctx, xl, fd) == 0)
        {
            fd_healing = fd_ctx->healing;
        }
        if (inode_ctx->healing == 0)
        {
            if (fd_healing != 0)
            {
                gf_log(xl->name, GF_LOG_ERROR, "Heal request to non healing file");

                error = EPERM;

                goto failed;
            }
        }
        else
        {
            if (fd_healing == 0)
            {
                if (inode_ctx->offset < offset + iov_length(vector, count))
                {
                    gf_log(xl->name, GF_LOG_ERROR, "Bad offset writing to file being healed (%lX - %lX)", offset, inode_ctx->offset);

                    error = EPERM;

                    goto failed;
                }
            }
            else
            {
                if (offset != inode_ctx->offset)
                {
                    gf_log(xl->name, GF_LOG_ERROR, "Bad offset healing (%lX - %lX)", offset, inode_ctx->offset);

                    error = EPERM;

                    goto failed;
                }

                UNLOCK(&fd->inode->lock);

                STACK_WIND_COOKIE(frame, heal_writev_cbk, inode_ref(fd->inode), FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->writev, fd, vector, count, offset, flags, iobref, xdata);

                return 0;
            }
        }
    }

    if (error == 0)
    {
        UNLOCK(&fd->inode->lock);

        STACK_WIND(frame, default_writev_cbk, FIRST_CHILD(xl), FIRST_CHILD(xl)->fops->writev, fd, vector, count, offset, flags, iobref, xdata);

        return 0;
    }

failed:
    UNLOCK(&fd->inode->lock);

    STACK_UNWIND_STRICT(writev, frame, -1, error, NULL, NULL, NULL);

    return 0;
}

int32_t fini(xlator_t * xl)
{
    return 0;
}

int32_t init(xlator_t * xl)
{
    return 0;
}

int32_t heal_forget(xlator_t * xl, inode_t * inode)
{
    heal_inode_ctx_t * inode_ctx;
    uint64_t value;
    int32_t error;

    error = inode_ctx_del(inode, xl, &value);
    if ((error == 0) && (value != 0))
    {
        inode_ctx = (heal_inode_ctx_t *)(uintptr_t)value;

        GF_FREE(inode_ctx);
    }
    else
    {
        gf_log(xl->name, GF_LOG_ERROR, "inode does not have a context");
    }

    return 0;
}

int32_t heal_release(xlator_t * xl, fd_t * fd)
{
    heal_fd_ctx_t * fd_ctx;
    uint64_t value;
    int32_t error;

    error = fd_ctx_del(fd, xl, &value);
    if ((error == 0) && (value != 0))
    {
        fd_ctx = (heal_fd_ctx_t *)(uintptr_t)value;
        if (fd_ctx->healing != 0)
        {
            heal_inode_clear_healing(xl, fd->inode);
        }
        GF_FREE(fd_ctx);
    }
    else
    {
        gf_log(xl->name, GF_LOG_ERROR, "fd does not have a context");
    }

    return 0;
}

struct xlator_fops fops =
{
    .access       = heal_access,
    .create       = heal_create,
    .entrylk      = NULL,
    .fentrylk     = NULL,
    .flush        = NULL,
    .fsync        = NULL,
    .fsyncdir     = NULL,
    .getspec      = NULL,
    .getxattr     = heal_getxattr,
    .fgetxattr    = heal_fgetxattr,
    .inodelk      = NULL,
    .finodelk     = NULL,
    .link         = NULL,
    .lk           = NULL,
    .lookup       = heal_lookup,
    .mkdir        = NULL,
    .mknod        = NULL,
    .open         = NULL,
    .opendir      = NULL,
    .rchecksum    = heal_rchecksum,
    .readdir      = NULL,
    .readdirp     = NULL,
    .readlink     = NULL,
    .readv        = heal_readv,
    .removexattr  = NULL,
    .fremovexattr = NULL,
    .rename       = NULL,
    .rmdir        = NULL,
    .setattr      = NULL,
    .fsetattr     = NULL,
    .setxattr     = NULL,
    .fsetxattr    = NULL,
    .stat         = heal_stat,
    .fstat        = heal_fstat,
    .statfs       = NULL,
    .symlink      = NULL,
    .truncate     = heal_truncate,
    .ftruncate    = heal_ftruncate,
    .unlink       = heal_unlink,
    .writev       = heal_writev,
    .xattrop      = NULL,
    .fxattrop     = NULL
};

struct xlator_cbks cbks =
{
    .forget       = heal_forget,
    .release      = heal_release,
    .releasedir   = NULL
};
