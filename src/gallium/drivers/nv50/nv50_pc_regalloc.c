/*
 * Copyright 2010 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* #define NV50PC_DEBUG */

/* #define NV50_RA_DEBUG_LIVEI */
/* #define NV50_RA_DEBUG_LIVE_SETS */
/* #define NV50_RA_DEBUG_JOIN */

#include "nv50_context.h"
#include "nv50_pc.h"

#include "util/u_simple_list.h"

#define NUM_REGISTER_FILES 4

struct register_set {
   struct nv_pc *pc;

   uint32_t last[NUM_REGISTER_FILES];
   uint32_t bits[NUM_REGISTER_FILES][8];
};

struct nv_pc_pass {
   struct nv_pc *pc;

   struct nv_instruction **insns;
   int num_insns;

   uint pass_seq;
};

static void
ranges_coalesce(struct nv_range *range)
{
   while (range->next && range->end >= range->next->bgn) {
      struct nv_range *rnn = range->next->next;
      assert(range->bgn <= range->next->bgn);
      range->end = MAX2(range->end, range->next->end);
      FREE(range->next);
      range->next = rnn;
   }
}

static boolean
add_range_ex(struct nv_value *val, int bgn, int end, struct nv_range *new_range)
{
   struct nv_range *range, **nextp = &val->livei;

   for (range = val->livei; range; range = range->next) {
      if (end < range->bgn)
         break; /* insert before */

      if (bgn > range->end) {
         nextp = &range->next;
         continue; /* insert after */
      }

      /* overlap */
      if (bgn < range->bgn) {
         range->bgn = bgn;
         if (end > range->end)
            range->end = end;
         ranges_coalesce(range);
         return TRUE;
      }
      if (end > range->end) {
         range->end = end;
         ranges_coalesce(range);
         return TRUE;
      }
      assert(bgn >= range->bgn);
      assert(end <= range->end);
      return TRUE;
   }

   if (!new_range)
      new_range = CALLOC_STRUCT(nv_range);

   new_range->bgn = bgn;
   new_range->end = end;
   new_range->next = range;
   *(nextp) = new_range;
   return FALSE;
}

static void
add_range(struct nv_value *val, struct nv_basic_block *b, int end)
{
   int bgn;

   if (!val->insn) /* ignore non-def values */
      return;
   assert(b->entry->serial <= b->exit->serial);
   assert(b->phi->serial <= end);
   assert(b->exit->serial + 1 >= end);

   bgn = val->insn->serial;
   if (bgn < b->entry->serial || bgn > b->exit->serial)
      bgn = b->entry->serial;

   assert(bgn <= end);

   add_range_ex(val, bgn, end, NULL);
}

#if defined(NV50_RA_DEBUG_JOIN) || defined(NV50_RA_DEBUG_LIVEI)
static void
livei_print(struct nv_value *a)
{
   struct nv_range *r = a->livei;

   debug_printf("livei %i: ", a->n);
   while (r) {
      debug_printf("[%i, %i) ", r->bgn, r->end);
      r = r->next;
   }
   debug_printf("\n");
}
#endif

static void
livei_unify(struct nv_value *dst, struct nv_value *src)
{
   struct nv_range *range, *next;

   for (range = src->livei; range; range = next) {
      next = range->next;
      if (add_range_ex(dst, range->bgn, range->end, range))
         FREE(range);
   }
   src->livei = NULL;
}

static void
livei_release(struct nv_value *val)
{
   struct nv_range *range, *next;

   for (range = val->livei; range; range = next) {
      next = range->next;
      FREE(range);
   }
}

static boolean
livei_have_overlap(struct nv_value *a, struct nv_value *b)
{
   struct nv_range *r_a, *r_b;

   for (r_a = a->livei; r_a; r_a = r_a->next) {
      for (r_b = b->livei; r_b; r_b = r_b->next) {
         if (r_b->bgn < r_a->end &&
             r_b->end > r_a->bgn)
            return TRUE;
      }
   }
   return FALSE;
}

static int
livei_end(struct nv_value *a)
{
   struct nv_range *r = a->livei;

   assert(r);
   while (r->next)
      r = r->next;
   return r->end;
}

static boolean
livei_contains(struct nv_value *a, int pos)
{
   struct nv_range *r;

   for (r = a->livei; r && r->bgn <= pos; r = r->next)
      if (r->end > pos)
         return TRUE;
   return FALSE;
}

static boolean
reg_assign(struct register_set *set, struct nv_value **def, int n)
{
   int i, id, s;
   uint m;
   int f = def[0]->reg.file;

   s = n << (nv_type_order(def[0]->reg.type) - 1);
   m = (1 << s) - 1;

   id = set->last[f];

   for (i = 0; i * 32 < set->last[f]; ++i) {
      if (set->bits[f][i] == 0xffffffff)
         continue;

      for (id = 0; id < 32; id += s)
         if (!(set->bits[f][i] & (m << id)))
            break;
      if (id < 32)
         break;
   }
   if (i * 32 + id > set->last[f])
      return FALSE;

   set->bits[f][i] |= m << id;

   id += i * 32;

   set->pc->max_reg[f] = MAX2(set->pc->max_reg[f], id + s - 1);

   id >>= nv_type_order(def[0]->reg.type) - 1;

   for (i = 0; i < n; ++i)
      if (def[i]->livei)
         def[i]->reg.id = id++;

   return TRUE;
}

static INLINE void
reg_occupy(struct register_set *set, struct nv_value *val)
{
   int s, id = val->reg.id, f = val->reg.file;
   uint m;

   if (id < 0)
      return;
   s = nv_type_order(val->reg.type) - 1;
   id <<= s;
   m = (1 << (1 << s)) - 1;

   set->bits[f][id / 32] |= m << (id % 32);

   if (set->pc->max_reg[f] < id)
      set->pc->max_reg[f] = id;
}

static INLINE void
reg_release(struct register_set *set, struct nv_value *val)
{
   int s, id = val->reg.id, f = val->reg.file;
   uint m;

   if (id < 0)
      return;

   s = nv_type_order(val->reg.type) - 1;
   id <<= s;
   m = (1 << (1 << s)) - 1;

   set->bits[f][id / 32] &= ~(m << (id % 32));
}

static INLINE boolean
join_allowed(struct nv_pc_pass *ctx, struct nv_value *a, struct nv_value *b)
{
   int i;
   struct nv_value *val;

   if (a->reg.file != b->reg.file ||
       nv_type_sizeof(a->reg.type) != nv_type_sizeof(b->reg.type))
      return FALSE;

   if (a->join->reg.id == b->join->reg.id)
      return TRUE;

#if 1
   /* either a or b or both have been assigned */

   if (a->join->reg.id >= 0 && b->join->reg.id >= 0)
      return FALSE;
   else
   if (b->join->reg.id >= 0) {
      if (a->join->reg.id >= 0)
         return FALSE;
      val = a;
      a = b;
      b = val;
   }

   for (i = 0; i < ctx->pc->num_values; ++i) {
      val = &ctx->pc->values[i];

      if (val->join->reg.id != a->join->reg.id)
         continue;
      if (val->join != a->join && livei_have_overlap(val->join, b->join))
         return FALSE;
   }
   return TRUE;
#endif
   return FALSE;
}

static INLINE void
do_join_values(struct nv_pc_pass *ctx, struct nv_value *a, struct nv_value *b)
{
   int j;
   struct nv_value *bjoin = b->join;

   if (b->join->reg.id >= 0)
      a->join->reg.id = b->join->reg.id;

   livei_unify(a->join, b->join);

#ifdef NV50_RA_DEBUG_JOIN
   debug_printf("joining %i to %i\n", b->n, a->n);
#endif
   
   /* make a->join the new representative */
   for (j = 0; j < ctx->pc->num_values; ++j) 
      if (ctx->pc->values[j].join == bjoin)
         ctx->pc->values[j].join = a->join;

   assert(b->join == a->join);
}

static INLINE void
try_join_values(struct nv_pc_pass *ctx, struct nv_value *a, struct nv_value *b)
{
   if (!join_allowed(ctx, a, b)) {
#ifdef NV50_RA_DEBUG_JOIN
      debug_printf("cannot join %i to %i: not allowed\n", b->n, a->n);
#endif
      return;
   }
   if (livei_have_overlap(a->join, b->join)) {
#ifdef NV50_RA_DEBUG_JOIN
      debug_printf("cannot join %i to %i: livei overlap\n", b->n, a->n);
      livei_print(a);
      livei_print(b);
#endif
      return;
   }

   do_join_values(ctx, a, b);
}

static INLINE boolean
need_new_else_block(struct nv_basic_block *b, struct nv_basic_block *p)
{
   int i = 0, n = 0;

   for (; i < 2; ++i)
      if (p->out[i] && !IS_LOOP_EDGE(p->out_kind[i]))
         ++n;

   return (b->num_in > 1) && (n == 2);
}

static int
phi_opnd_for_bb(struct nv_instruction *phi, struct nv_basic_block *b,
                struct nv_basic_block *tb)
{
   int i, j;

   for (j = -1, i = 0; i < 4 && phi->src[i]; ++i) {
      if (!nvbb_reachable_by(b, phi->src[i]->value->insn->bb, tb))
         continue;
      /* NOTE: back-edges are ignored by the reachable-by check */
      if (j < 0 || !nvbb_reachable_by(phi->src[j]->value->insn->bb,
                                      phi->src[i]->value->insn->bb, tb))
         j = i;
   }
   return j;
}

/* For each operand of each PHI in b, generate a new value by inserting a MOV
 * at the end of the block it is coming from and replace the operand with its
 * result. This eliminates liveness conflicts and enables us to let values be
 * copied to the right register if such a conflict exists nonetheless.
 *
 * These MOVs are also crucial in making sure the live intervals of phi srces
 * are extended until the end of the loop, since they are not included in the
 * live-in sets.
 */
static int
pass_generate_phi_movs(struct nv_pc_pass *ctx, struct nv_basic_block *b)
{
   struct nv_instruction *i, *ni;
   struct nv_value *val;
   struct nv_basic_block *p, *pn;
   int n, j;

   b->pass_seq = ctx->pc->pass_seq;

   for (n = 0; n < b->num_in; ++n) {
      p = pn = b->in[n];
      assert(p);

      if (need_new_else_block(b, p)) {
         pn = new_basic_block(ctx->pc);

         if (p->out[0] == b)
            p->out[0] = pn;
         else
            p->out[1] = pn;

         if (p->exit->target == b) /* target to new else-block */
            p->exit->target = pn;

         b->in[n] = pn;

         pn->out[0] = b;
         pn->in[0] = p;
         pn->num_in = 1;
      }
      ctx->pc->current_block = pn;

      for (i = b->phi; i && i->opcode == NV_OP_PHI; i = i->next) {
         if ((j = phi_opnd_for_bb(i, p, b)) < 0)
            continue;
         val = i->src[j]->value;

         if (i->src[j]->flags) {
            val = val->insn->src[0]->value;
            while (j < 4 && i->src[j])
               ++j;
            assert(j < 4);
         }

         ni = new_instruction(ctx->pc, NV_OP_MOV);

         /* TODO: insert instruction at correct position in the first place */
         if (ni->prev && ni->prev->target)
            nv_nvi_permute(ni->prev, ni);

         ni->def[0] = new_value(ctx->pc, val->reg.file, val->reg.type);
         ni->def[0]->insn = ni;
         ni->src[0] = new_ref(ctx->pc, val);

         nv_reference(ctx->pc, &i->src[j], ni->def[0]);

         i->src[j]->flags = 1;
      }

      if (pn != p && pn->exit) {
         ctx->pc->current_block = b->in[n ? 0 : 1];
         ni = new_instruction(ctx->pc, NV_OP_BRA);
         ni->target = b;
         ni->is_terminator = 1;
      }
   }

   for (j = 0; j < 2; ++j)
      if (b->out[j] && b->out[j]->pass_seq < ctx->pc->pass_seq)
         pass_generate_phi_movs(ctx, b->out[j]);

   return 0;
}

static int
pass_join_values(struct nv_pc_pass *ctx, int iter)
{
   int c, n;

   for (n = 0; n < ctx->num_insns; ++n) {
      struct nv_instruction *i = ctx->insns[n];

      switch (i->opcode) {
      case NV_OP_PHI:
         if (iter != 2)
            break;
         for (c = 0; c < 4 && i->src[c]; ++c)
            try_join_values(ctx, i->def[0], i->src[c]->value);
         break;
      case NV_OP_MOV:
         if ((iter == 2) && i->src[0]->value->insn &&
             !nv_is_vector_op(i->src[0]->value->join->insn->opcode))
            try_join_values(ctx, i->def[0], i->src[0]->value);
         break;
      case NV_OP_SELECT:
         if (iter != 1)
            break;
         for (c = 0; c < 4 && i->src[c]; ++c) {
            assert(join_allowed(ctx, i->def[0], i->src[c]->value));
            do_join_values(ctx, i->def[0], i->src[c]->value);
         }
         break;
      case NV_OP_TEX:
      case NV_OP_TXB:
      case NV_OP_TXL:
      case NV_OP_TXQ:
         if (iter)
            break;
         for (c = 0; c < 4; ++c) {
            if (!i->src[c])
               break;
            do_join_values(ctx, i->def[c], i->src[c]->value);
         }
         break;
      default:
         break;
      }
   }
   return 0;
}

/* Order the instructions so that live intervals can be expressed in numbers. */
static void
pass_order_instructions(void *priv, struct nv_basic_block *b)
{
   struct nv_pc_pass *ctx = (struct nv_pc_pass *)priv;
   struct nv_instruction *i;

   b->pass_seq = ctx->pc->pass_seq;

   assert(!b->exit || !b->exit->next);
   for (i = b->phi; i; i = i->next) {
      i->serial = ctx->num_insns;
      ctx->insns[ctx->num_insns++] = i;
   }
}

static void
bb_live_set_print(struct nv_pc *pc, struct nv_basic_block *b)
{
#ifdef NV50_RA_DEBUG_LIVE_SETS
   int j;
   struct nv_value *val;

   debug_printf("LIVE-INs of BB:%i: ", b->id);

   for (j = 0; j < pc->num_values; ++j) {
      if (!(b->live_set[j / 32] & (1 << (j % 32))))
         continue;
      val = &pc->values[j];
      if (!val->insn)
         continue;
      debug_printf("%i ", val->n);
   }
   debug_printf("\n");
#endif
}

static INLINE void
live_set_add(struct nv_basic_block *b, struct nv_value *val)
{
   if (!val->insn) /* don't add non-def values */
      return;
   b->live_set[val->n / 32] |= 1 << (val->n % 32);
}

static INLINE void
live_set_rem(struct nv_basic_block *b, struct nv_value *val)
{
   b->live_set[val->n / 32] &= ~(1 << (val->n % 32));
}

static INLINE boolean
live_set_test(struct nv_basic_block *b, struct nv_ref *ref)
{
   int n = ref->value->n;
   return b->live_set[n / 32] & (1 << (n % 32));
}

/* The live set of a block contains those values that are live immediately
 * before the beginning of the block, so do a backwards scan.
 */
static int
pass_build_live_sets(struct nv_pc_pass *ctx, struct nv_basic_block *b)
{
   struct nv_instruction *i;
   int j, n, ret = 0;

   if (b->pass_seq >= ctx->pc->pass_seq)
      return 0;
   b->pass_seq = ctx->pc->pass_seq;

   /* slight hack for undecidedness: set phi = entry if it's undefined */
   if (!b->phi)
      b->phi = b->entry;

   for (n = 0; n < 2; ++n) {
      if (!b->out[n] || b->out[n] == b)
         continue;
      ret = pass_build_live_sets(ctx, b->out[n]);
      if (ret)
         return ret;

      if (n == 0) {
         for (j = 0; j < (ctx->pc->num_values + 31) / 32; ++j)
            b->live_set[j] = b->out[n]->live_set[j];
      } else {
         for (j = 0; j < (ctx->pc->num_values + 31) / 32; ++j)
            b->live_set[j] |= b->out[n]->live_set[j];
      }
   }

   if (!b->entry)
      return 0;

   bb_live_set_print(ctx->pc, b);

   for (i = b->exit; i != b->entry->prev; i = i->prev) {
      for (j = 0; j < 4; j++) {
         if (!i->def[j])
            break;
         live_set_rem(b, i->def[j]);
      }
      for (j = 0; j < 4; j++) {
         if (!i->src[j])
            break;
         live_set_add(b, i->src[j]->value);
      }
      if (i->src[4])
         live_set_add(b, i->src[4]->value);
      if (i->flags_def)
         live_set_rem(b, i->flags_def);
      if (i->flags_src)
         live_set_add(b, i->flags_src->value);
   }
   for (i = b->phi; i && i->opcode == NV_OP_PHI; i = i->next)
      live_set_rem(b, i->def[0]);

   bb_live_set_print(ctx->pc, b);

   return 0;
}

static void collect_live_values(struct nv_basic_block *b, const int n)
{
   int i;

   if (b->out[0]) {
      if (b->out[1]) { /* what to do about back-edges ? */
         for (i = 0; i < n; ++i)
            b->live_set[i] = b->out[0]->live_set[i] | b->out[1]->live_set[i];
      } else {
         memcpy(b->live_set, b->out[0]->live_set, n * sizeof(uint32_t));
      }
   } else
   if (b->out[1]) {
      memcpy(b->live_set, b->out[1]->live_set, n * sizeof(uint32_t));
   } else {
      memset(b->live_set, 0, n * sizeof(uint32_t));
   }
}

/* NOTE: the live intervals of phi functions start at the first non-phi insn. */
static int
pass_build_intervals(struct nv_pc_pass *ctx, struct nv_basic_block *b)
{
   struct nv_instruction *i, *i_stop;
   int j, s;
   const int n = (ctx->pc->num_values + 31) / 32;

   /* verify that first block does not have live-in values */
   if (b->num_in == 0)
      for (j = 0; j < n; ++j)
         assert(b->live_set[j] == 0);

   collect_live_values(b, n);

   /* remove live-outs def'd in a parallel block, hopefully they're all phi'd */
   for (j = 0; j < 2; ++j) {
      if (!b->out[j] || !b->out[j]->phi)
         continue;
      for (i = b->out[j]->phi; i->opcode == NV_OP_PHI; i = i->next) {
         live_set_rem(b, i->def[0]);

         for (s = 0; s < 4; ++s) {
            if (!i->src[s])
               break;
            assert(i->src[s]->value->insn);
            if (nvbb_reachable_by(b, i->src[s]->value->insn->bb, b->out[j]))
               live_set_add(b, i->src[s]->value);
            else
               live_set_rem(b, i->src[s]->value);
         }
      }
   }

   /* remaining live-outs are live until the end */
   if (b->exit) {
      for (j = 0; j < ctx->pc->num_values; ++j) {
         if (!(b->live_set[j / 32] & (1 << (j % 32))))
            continue;
         add_range(&ctx->pc->values[j], b, b->exit->serial + 1);
#ifdef NV50_RA_DEBUG_LIVEI
         debug_printf("adding range for live value %i: ", j);
         livei_print(&ctx->pc->values[j]);
#endif

      }
   }

   i_stop = b->entry ? b->entry->prev : NULL;

   /* don't have to include phi functions here (will have 0 live range) */
   for (i = b->exit; i != i_stop; i = i->prev) {
      assert(i->serial >= b->phi->serial && i->serial <= b->exit->serial);
      for (j = 0; j < 4; ++j) {
         if (i->def[j])
            live_set_rem(b, i->def[j]);
      }
      if (i->flags_def)
         live_set_rem(b, i->flags_def);

      for (j = 0; j < 5; ++j) {
         if (i->src[j] && !live_set_test(b, i->src[j])) {
            live_set_add(b, i->src[j]->value);
            add_range(i->src[j]->value, b, i->serial);
#ifdef NV50_RA_DEBUG_LIVEI
            debug_printf("adding range for source %i (ends living): ",
                         i->src[j]->value->n);
            livei_print(i->src[j]->value);
#endif
         }
      }
      if (i->flags_src && !live_set_test(b, i->flags_src)) {
         live_set_add(b, i->flags_src->value);
         add_range(i->flags_src->value, b, i->serial);
#ifdef NV50_RA_DEBUG_LIVEI
         debug_printf("adding range for source %i (ends living): ",
                      i->flags_src->value->n);
         livei_print(i->flags_src->value);
#endif
      }
   }

   b->pass_seq = ctx->pc->pass_seq;

   if (b->out[0] && b->out[0]->pass_seq < ctx->pc->pass_seq)
      pass_build_intervals(ctx, b->out[0]);

   if (b->out[1] && b->out[1]->pass_seq < ctx->pc->pass_seq)
      pass_build_intervals(ctx, b->out[1]);

   return 0;
}

static INLINE void
nv50_ctor_register_set(struct nv_pc *pc, struct register_set *set)
{
   memset(set, 0, sizeof(*set));

   set->last[NV_FILE_GPR] = 255;
   set->last[NV_FILE_OUT] = 127;
   set->last[NV_FILE_FLAGS] = 4;
   set->last[NV_FILE_ADDR] = 4;

   set->pc = pc;
}

static void
insert_ordered_tail(struct nv_value *list, struct nv_value *nval)
{
   struct nv_value *elem;

   for (elem = list->prev;
	elem != list && elem->livei->bgn > nval->livei->bgn;
	elem = elem->prev);
   /* now elem begins before or at the same time as val */

   nval->prev = elem;
   nval->next = elem->next;
   elem->next->prev = nval;
   elem->next = nval;
}

static int
pass_linear_scan(struct nv_pc_pass *ctx, int iter)
{
   struct nv_instruction *i;
   struct register_set f, free;
   int k, n;
   struct nv_value *cur, *val, *tmp[2];
   struct nv_value active, inactive, handled, unhandled;

   make_empty_list(&active);
   make_empty_list(&inactive);
   make_empty_list(&handled);
   make_empty_list(&unhandled);

   nv50_ctor_register_set(ctx->pc, &free);

   /* joined values should have range = NULL and thus not be added;
    * also, fixed memory values won't be added because they're not
    * def'd, just used
    */
   for (n = 0; n < ctx->num_insns; ++n) {
      i = ctx->insns[n];

      for (k = 0; k < 4; ++k) {
         if (i->def[k] && i->def[k]->livei)
            insert_ordered_tail(&unhandled, i->def[k]);
         else
         if (0 && i->def[k])
            debug_printf("skipping def'd value %i: no livei\n", i->def[k]->n);
      }
      if (i->flags_def && i->flags_def->livei)
         insert_ordered_tail(&unhandled, i->flags_def);
   }

   for (val = unhandled.next; val != unhandled.prev; val = val->next) {
      assert(val->join == val);
      assert(val->livei->bgn <= val->next->livei->bgn);
   }

   foreach_s(cur, tmp[0], &unhandled) {
      remove_from_list(cur);

      foreach_s(val, tmp[1], &active) {
         if (livei_end(val) <= cur->livei->bgn) {
            reg_release(&free, val);
            move_to_head(&handled, val);
         } else
         if (!livei_contains(val, cur->livei->bgn)) {
            reg_release(&free, val);
            move_to_head(&inactive, val);
         }
      }

      foreach_s(val, tmp[1], &inactive) {
         if (livei_end(val) <= cur->livei->bgn)
            move_to_head(&handled, val);
         else
         if (livei_contains(val, cur->livei->bgn)) {
            reg_occupy(&free, val);
            move_to_head(&active, val);
         }
      }

      f = free;

      foreach(val, &inactive)
         if (livei_have_overlap(val, cur))
            reg_occupy(&f, val);

      foreach(val, &unhandled)
         if (val->reg.id >= 0 && livei_have_overlap(val, cur))
            reg_occupy(&f, val);

      if (cur->reg.id < 0) {
         boolean mem = FALSE;

         if (nv_is_vector_op(cur->insn->opcode))
            mem = !reg_assign(&f, &cur->insn->def[0], 4);
         else
         if (iter)
            mem = !reg_assign(&f, &cur, 1);

         if (mem) {
            NOUVEAU_ERR("out of registers\n");
            abort();
         }
      }
      insert_at_head(&active, cur);
      reg_occupy(&free, cur);
   }

   return 0;
}

static int
nv_pc_pass1(struct nv_pc *pc, struct nv_basic_block *root)
{
   struct nv_pc_pass *ctx;
   int i, ret;

   NV50_DBGMSG("REGISTER ALLOCATION - entering\n");

   ctx = CALLOC_STRUCT(nv_pc_pass);
   if (!ctx)
      return -1;
   ctx->pc = pc;

   ctx->insns = CALLOC(NV_PC_MAX_INSTRUCTIONS, sizeof(struct nv_instruction *));
   if (!ctx->insns) {
      FREE(ctx);
      return -1;
   }

   pc->pass_seq++;
   ret = pass_generate_phi_movs(ctx, root);
   assert(!ret);

   for (i = 0; i < pc->loop_nesting_bound; ++i) {
      pc->pass_seq++;
      ret = pass_build_live_sets(ctx, root);
      assert(!ret && "live sets");
      if (ret) {
         NOUVEAU_ERR("failed to build live sets (iteration %d)\n", i);
         goto out;
      }
   }

   pc->pass_seq++;
   nv_pc_pass_in_order(root, pass_order_instructions, ctx);

   pc->pass_seq++;
   ret = pass_build_intervals(ctx, root);
   assert(!ret && "build intervals");
   if (ret) {
      NOUVEAU_ERR("failed to build live intervals\n");
      goto out;
   }

#ifdef NV50_RA_DEBUG_LIVEI
   for (i = 0; i < pc->num_values; ++i)
      livei_print(&pc->values[i]);
#endif

   ret = pass_join_values(ctx, 0);
   if (ret)
      goto out;
   ret = pass_linear_scan(ctx, 0);
   if (ret)
      goto out;
   ret = pass_join_values(ctx, 1);
   if (ret)
      goto out;
   ret = pass_join_values(ctx, 2);
   if (ret)
      goto out;
   ret = pass_linear_scan(ctx, 1);
   if (ret)
      goto out;

   for (i = 0; i < pc->num_values; ++i)
      livei_release(&pc->values[i]);

   NV50_DBGMSG("REGISTER ALLOCATION - leaving\n");

out:
   FREE(ctx->insns);
   FREE(ctx);
   return ret;
}

int
nv_pc_exec_pass1(struct nv_pc *pc)
{
   int i, ret;

   for (i = 0; i < pc->num_subroutines + 1; ++i)
      if (pc->root[i] && (ret = nv_pc_pass1(pc, pc->root[i])))
         return ret;
   return 0;
}
