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

#include "byte-order.h"
#include <xlator.h>

#include "heal.h"
#include "heal-type-dict.h"

int32_t heal_dict_special(const char * name)
{
    if ((strcmp(HEAL_KEY_FLAGS, name) == 0) ||
        (strcmp(HEAL_KEY_SIZE, name) == 0))
    {
        return 1;
    }

    return 0;
}

int32_t heal_dict_data_compare(data_t * dst, data_t * src)
{
    if (dst->len < src->len)
    {
        return -1;
    }
    if (dst->len > src->len)
    {
        return 1;
    }
    return memcmp(dst->data, src->data, dst->len);
}

static int heal_dict_equal_enum(dict_t * dst, char * key, data_t * value, void * arg)
{
    data_t * tmp;

    tmp = dict_get(arg, key);
    if (tmp == NULL)
    {
        return -1;
    }
    else
    {
        if (heal_dict_data_compare(value, tmp) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int32_t heal_dict_equal(dict_t * dst, dict_t * src)
{
    if (dst == src)
    {
        return 1;
    }

    if (dst->count != src->count)
    {
        return 0;
    }

    return (dict_foreach(dst, heal_dict_equal_enum, src) == 0);
}

int32_t heal_dict_set_cow(dict_t ** dst, char * key, data_t * value)
{
    dict_t * new;
    data_t * tmp;

    if ((*dst)->refcount != 1)
    {
        tmp = dict_get(*dst, key);
        if ((tmp == NULL) || (heal_dict_data_compare(value, tmp) != 0))
        {
            new = dict_copy(*dst, NULL);
            if (new == NULL)
            {
                return -1;
            }
            dict_unref(*dst);
            dict_ref(new);
            *dst = new;
        }
    }

    return dict_set(*dst, key, value);
}

int32_t heal_dict_set_bin_cow(dict_t ** dst, char * key, void * value, uint32_t length)
{
    dict_t * new;
    data_t * tmp;

    if ((*dst)->refcount != 1)
    {
        tmp = dict_get(*dst, key);
        if ((tmp == NULL) || (heal_dict_data_compare(value, tmp) != 0))
        {
            new = dict_copy(*dst, NULL);
            if (new == NULL)
            {
                return -1;
            }
            dict_unref(*dst);
            dict_ref(new);
            *dst = new;
        }
    }

    return dict_set_bin(*dst, key, value, length);
}

int32_t heal_dict_get_bin(dict_t * src, char * key, void * value, uint32_t * length)
{
    data_t * data;
    int32_t error, size;

    error = ENOENT;
    data = dict_get(src, key);
    if (data != NULL)
    {
        error = ENOBUFS;
        size = *length;
        *length = data->len;
        if (size >= data->len)
        {
            error = 0;
            size = data->len;
        }
        memcpy(value, data->data, size);
    }

    return error;
}

#define hton8(_x) _x
#define ntoh8(_x) _x

#define HEAL_DICT_SET_COW(_type, _size) \
    int32_t heal_dict_set_##_type##_size##_cow(dict_t ** dst, char * key, _type##_size##_t value) \
    { \
        int32_t error; \
        typeof(value) * ptr = GF_MALLOC(sizeof(value), gf_heal_mt_uint8_t); \
        if (ptr == NULL) \
        { \
            return ENOMEM; \
        } \
        *ptr = hton##_size(value); \
        error = heal_dict_set_bin_cow(dst, key, ptr, sizeof(value)); \
        if (error != 0) \
        { \
            GF_FREE(ptr); \
        } \
        return error; \
    }

#define HEAL_DICT_GET(_type, _size) \
    int32_t heal_dict_get_##_type##_size(dict_t * src, char * key, _type##_size##_t * value) \
    { \
        typeof(*value) tmp; \
        uint32_t size = sizeof(tmp); \
        int32_t error = heal_dict_get_bin(src, key, &tmp, &size); \
        if (error == 0) \
        { \
            *value = ntoh##_size(tmp); \
        } \
        return error; \
    }

HEAL_DICT_SET_COW(int, 8)
HEAL_DICT_SET_COW(int, 16)
HEAL_DICT_SET_COW(int, 32)
HEAL_DICT_SET_COW(int, 64)
HEAL_DICT_SET_COW(uint, 16)
HEAL_DICT_SET_COW(uint, 32)
HEAL_DICT_SET_COW(uint, 64)

HEAL_DICT_GET(int, 8)
HEAL_DICT_GET(int, 16)
HEAL_DICT_GET(int, 32)
HEAL_DICT_GET(int, 64)
HEAL_DICT_GET(uint, 16)
HEAL_DICT_GET(uint, 32)
HEAL_DICT_GET(uint, 64)

int32_t heal_dict_del_cow(dict_t ** dst, char * key)
{
    dict_t * new;

    if ((*dst)->refcount != 1)
    {
        if (dict_get(*dst, key) != NULL)
        {
            new = dict_copy(*dst, NULL);
            if (new == NULL)
            {
                return -1;
            }
            dict_unref(*dst);
            dict_ref(new);
            *dst = new;
        }
    }

    dict_del(*dst, key);

    return 0;
}

int heal_dict_clean_enum(dict_t * src, char * key, data_t * value, void * arg)
{
    dict_t ** dict;

    dict = arg;

    if (heal_dict_special(key))
    {
        if (heal_dict_del_cow(dict, key) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int32_t heal_dict_clean_cow(dict_t ** dst)
{
    return dict_foreach(*dst, heal_dict_clean_enum, dst);
}

int heal_dict_combine_enum(dict_t * src, char * key, data_t * value, void * arg)
{
    dict_t ** dict;
    data_t * tmp;

    dict = arg;

    tmp = dict_get(*dict, key);
    if (tmp != NULL)
    {
        if (heal_dict_special(key))
        {
            if (value->len != tmp->len)
            {
                return -1;
            }
        }
        else if (heal_dict_data_compare(value, tmp) != 0)
        {
            return -1;
        }
    }
    else
    {
        return -1;
/*
        if (heal_dict_special(key))
        {
            goto failed;
        }

        if (data->dict->refcount != 1)
        {
            new = dict_copy(data->dict, NULL);
            if (new == NULL)
            {
                goto failed;
            }
            dict_unref(data->dict);
            dict_ref(new);
            data->dict = new;
        }

        if (dict_set(data->dict, key, value) != 0)
        {
            goto failed;
        }
*/
    }

    return 0;
}
