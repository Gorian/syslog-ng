/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "value-pairs.h"
#include "vptransform.h"
#include "templates.h"
#include "cfg-parser.h"
#include "misc.h"

#include <string.h>

typedef const gchar *(*VPTransFunc)(ValuePairsTransform *t, const gchar *name);
typedef void (*VPTransDestroyFunc)(ValuePairsTransform *t);
typedef gboolean (*VPTransMatchFunc)(ValuePairsTransform *t, const gchar *name);

struct _ValuePairsTransform
{
  gchar *match_str;

  VPTransFunc transform;
  VPTransDestroyFunc destroy;
  VPTransMatchFunc match;
};

typedef struct
{
  ValuePairsTransform super;

  GPatternSpec *pattern;
  gint amount;
} VPTransShift;

typedef struct
{
  ValuePairsTransform super;

  GPatternSpec *pattern;
  gchar *prefix;

  GHashTable *cache;
} VPTransAddPrefix;

static void
vp_trans_init(ValuePairsTransform *t, const gchar *match_str,
	      VPTransFunc trans, VPTransDestroyFunc dest,
	      VPTransMatchFunc match)
{
  if (!t)
    return;

  t->match_str = g_strdup(match_str);
  t->transform = trans;
  t->destroy = dest;
  t->match = match;
}

void
value_pairs_transform_free(ValuePairsTransform *t)
{
  if (t->destroy)
    t->destroy(t);
  g_free(t->match);
  g_free(t);
}

const gchar *
value_pairs_transform_apply(ValuePairsTransform *t, const gchar *key)
{
  if (t->match(t, key))
    return t->transform(t, key);
  return key;
}

/* add_prefix() */

static const gchar *
vp_trans_add_prefix(ValuePairsTransform *t, const gchar *name)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;
  gpointer r;

  r = g_hash_table_lookup(self->cache, name);
  if (!r)
    {
      r = (gpointer)g_strconcat(self->prefix, name, NULL);
      g_hash_table_insert(self->cache, g_strdup(name), r);
    }
  return r;
}

static void
vp_trans_add_prefix_destroy(ValuePairsTransform *t)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;

  g_pattern_spec_free(self->pattern);
  if (self->cache)
    g_hash_table_destroy(self->cache);
  g_free(self->prefix);
}

static gboolean
vp_trans_add_prefix_match(ValuePairsTransform *t, const gchar *key)
{
  VPTransAddPrefix *self = (VPTransAddPrefix *)t;

  return g_pattern_match_string(self->pattern, key);
}

ValuePairsTransform *
value_pairs_new_transform_add_prefix (const gchar *glob, const gchar *prefix)
{
  VPTransAddPrefix *vpt;

  vpt = g_new(VPTransAddPrefix, 1);
  vp_trans_init((ValuePairsTransform *)vpt, glob, vp_trans_add_prefix, vp_trans_add_prefix_destroy,
		vp_trans_add_prefix_match);

  vpt->cache = g_hash_table_new_full(g_str_hash, g_str_equal,
				     g_free, g_free);
  vpt->pattern = g_pattern_spec_new(glob);
  vpt->prefix = g_strdup(prefix);

  return (ValuePairsTransform *)vpt;
}

/* shift() */

static const gchar *
vp_trans_shift(ValuePairsTransform *t, const gchar* name)
{
  VPTransShift *self = (VPTransShift *)t;

  if (self->amount <= 0)
    return name;
  return name + self->amount;
}

static void
vp_trans_shift_destroy(ValuePairsTransform *t)
{
  VPTransShift *self = (VPTransShift *)t;

  g_pattern_spec_free(self->pattern);
}

static gboolean
vp_trans_shift_match(ValuePairsTransform *t, const gchar *key)
{
  VPTransShift *self = (VPTransShift *)t;

  return g_pattern_match_string(self->pattern, key);
}

ValuePairsTransform *
value_pairs_new_transform_shift (const gchar *glob, gint amount)
{
  VPTransShift *vpt;

  vpt = g_new(VPTransShift, 1);
  vp_trans_init((ValuePairsTransform *)vpt, glob, vp_trans_shift, vp_trans_shift_destroy,
		vp_trans_shift_match);

  vpt->pattern = g_pattern_spec_new(glob);
  vpt->amount = amount;

  return (ValuePairsTransform *)vpt;
}
