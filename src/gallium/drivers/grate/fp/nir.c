/*
 * NIR to Tegra fragment program.
 *
 * The fragment ALU is scalar with four slots to a packet, so NIR is scalarised
 * and every SSA definition gets one scalar register. That is a better fit than
 * the vec4 granularity the TGSI path had to use, because the register file is
 * only 19 scalars deep.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/nir/nir.h"
#include "compiler/glsl_types.h"
#include "util/u_memory.h"
#include "util/u_math.h"

#include "grate_common.h"
#include "grate_compiler.h"
#include "fpir.h"

/* r16..r23 are the global file; r0..r3 carry varyings, TEX results and the
 * colour output and r4 holds 1/w, so r5..r15 are what is left to spill into. */
#define FP_NUM_GLOBALS 8
#define FP_SPILL_FIRST 5
#define FP_SPILL_COUNT 11

struct fp_nir_ctx {
   struct grate_fp_shader *fp;
   int *ssa_slot;
   unsigned num_slots;
   bool overflow;

   /* per pipeline instruction */
   struct fp_mfu_instr *mfu;
   uint32_t constants[3];
   int num_constants;
};

static unsigned
fp_reg_for_slot(struct fp_nir_ctx *ctx, unsigned slot)
{
   if (slot < FP_NUM_GLOBALS)
      return 16 + slot;

   slot -= FP_NUM_GLOBALS;
   if (slot < FP_SPILL_COUNT)
      return FP_SPILL_FIRST + slot;

   if (!ctx->overflow)
      fprintf(stderr, "GRATE FRAG: out of temporary registers\n");
   ctx->overflow = true;
   return 16;
}

static unsigned
fp_slot_for_def(struct fp_nir_ctx *ctx, const nir_def *def)
{
   if (ctx->ssa_slot[def->index] < 0)
      ctx->ssa_slot[def->index] = ctx->num_slots++;
   return ctx->ssa_slot[def->index];
}

static struct fp_alu_src_operand
fp_src_zero(void)
{
   struct fp_alu_src_operand s = { .index = 31, .datatype = FP_DATATYPE_FIXED10 };
   return s;
}

static struct fp_alu_src_operand
fp_src_one(void)
{
   struct fp_alu_src_operand s = { .index = 31, .datatype = FP_DATATYPE_FIXED10,
                                   .sub_reg_select_high = 1 };
   return s;
}

/* TEX writes RGBA into R2-R3 as four fx10s */
static struct fp_alu_src_operand
fp_src_tex(unsigned comp)
{
   int o = comp < 3 ? (2 - comp) : 3;
   struct fp_alu_src_operand s = {
      .index = 2 + o / 2,
      .datatype = FP_DATATYPE_FIXED10,
      .sub_reg_select_high = (o % 2) != 0,
   };
   return s;
}

/* up to three fp20 constants ride in a packet's fourth slot as registers 28..30 */
static int
fp_constant(struct fp_nir_ctx *ctx, float v)
{
   uint32_t enc = grate_fp20_from_float(v);

   for (int i = 0; i < ctx->num_constants; ++i)
      if (ctx->constants[i] == enc)
         return 28 + i;

   if (ctx->num_constants == 3)
      return -1;

   ctx->constants[ctx->num_constants] = enc;
   return 28 + ctx->num_constants++;
}

static struct fp_alu_src_operand
fp_nir_src(struct fp_nir_ctx *ctx, nir_src src, unsigned comp)
{
   nir_instr *parent = nir_def_instr(src.ssa);
   struct fp_alu_src_operand op = { 0 };

   switch (parent->type) {
   case nir_instr_type_load_const: {
      nir_load_const_instr *lc = nir_instr_as_load_const(parent);
      float v = comp < lc->def.num_components ? lc->value[comp].f32 : 0.0f;

      if (v == 0.0f)
         return fp_src_zero();
      if (v == 1.0f)
         return fp_src_one();

      int reg = fp_constant(ctx, v);
      if (reg < 0) {
         fprintf(stderr, "GRATE FRAG: out of embedded constants for %f\n", v);
         return fp_src_zero();
      }
      op.index = reg;
      op.datatype = FP_DATATYPE_FP20;
      return op;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(parent);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_input:
      case nir_intrinsic_load_interpolated_input: {
         /* a varying is interpolated into the row register of its own
          * component, so the slot is keyed on the source swizzle */
         unsigned row = nir_intrinsic_base(intr);

         assert(ctx->mfu != NULL);
         ctx->mfu->var[comp].op = FP_VAR_OP_FP20;
         ctx->mfu->var[comp].tram_row = row;
         ctx->fp->info.max_tram_row = MAX2(ctx->fp->info.max_tram_row, row);

         op.index = comp;
         return op;
      }

      case nir_intrinsic_load_uniform: {
         unsigned base = nir_intrinsic_base(intr);
         unsigned off = nir_src_is_const(intr->src[0])
                           ? nir_src_as_uint(intr->src[0]) : 0;
         unsigned slot = (base + off) * 4 + comp;

         if (slot >= GRATE_FP_NUM_UNIFORMS) {
            fprintf(stderr, "GRATE FRAG: uniform slot %u past the %u the "
                            "hardware has\n", slot, GRATE_FP_NUM_UNIFORMS);
            return fp_src_zero();
         }

         op.index = GRATE_FP_UNIFORM_BASE + slot;
         op.datatype = FP_DATATYPE_FP20;
         return op;
      }

      default:
         break;
      }
      break;
   }

   case nir_instr_type_tex:
      return fp_src_tex(comp);

   case nir_instr_type_alu: {
      nir_alu_instr *a = nir_instr_as_alu(parent);

      /* vecN only gathers: component c is whatever source c names */
      if ((a->op == nir_op_vec2 || a->op == nir_op_vec3 ||
           a->op == nir_op_vec4) &&
          comp < nir_op_infos[a->op].num_inputs)
         return fp_nir_src(ctx, a->src[comp].src, a->src[comp].swizzle[0]);
      break;
   }

   default:
      break;
   }

   op.index = fp_reg_for_slot(ctx, fp_slot_for_def(ctx, src.ssa));
   op.datatype = FP_DATATYPE_FP20;
   return op;
}

static struct fp_alu_src_operand
fp_nir_alu_src(struct fp_nir_ctx *ctx, nir_alu_instr *alu, unsigned i)
{
   struct fp_alu_src_operand op =
      fp_nir_src(ctx, alu->src[i].src, alu->src[i].swizzle[0]);
   return op;
}

/*
 * The ALU computes rA*rB + rC, so every operation below is written in that
 * shape. MIN and MAX take the two products instead.
 */
enum fp_form {
   FP_FORM_MOV,       /* s0 * 1 + 0     */
   FP_FORM_MUL,       /* s0 * s1 + 0    */
   FP_FORM_ADD,       /* s0 * 1 + s1    */
   FP_FORM_MAD,       /* s0 * s1 + s2   */
   FP_FORM_SUB_REV,   /* s1 * 1 + (-s0) */
   FP_FORM_ONE_MINUS, /* (-s0) * 1 + 1  */
};

static struct fp_alu_instr_packet *
fp_new_packet(struct grate_fp_shader *fp)
{
   struct fp_alu_instr_packet *pkt = CALLOC_STRUCT(fp_alu_instr_packet);
   list_inithead(&pkt->link);
   return pkt;
}

static struct fp_instr *
fp_new_instr(void)
{
   struct fp_instr *inst = CALLOC_STRUCT(fp_instr);
   list_inithead(&inst->link);
   return inst;
}

static void
fp_finish(struct fp_nir_ctx *ctx, struct fp_instr *inst,
          struct fp_alu_instr_packet *pkt)
{
   struct grate_fp_shader *fp = ctx->fp;

   if (ctx->num_constants > 0) {
      pkt->has_constants = true;
      memcpy(pkt->constants, ctx->constants, sizeof(pkt->constants));
   }

   inst->alu_sched.address = list_length(&fp->alu_instructions);
   inst->alu_sched.num_instructions = 1;
   list_addtail(&pkt->link, &fp->alu_instructions);

   if (ctx->mfu) {
      inst->mfu_sched.address = list_length(&fp->mfu_instructions);
      inst->mfu_sched.num_instructions = 1;
      list_addtail(&ctx->mfu->link, &fp->mfu_instructions);
   }

   list_addtail(&inst->link, &fp->fp_instructions);

   ctx->mfu = NULL;
   ctx->num_constants = 0;
}

static bool
fp_src_is_varying(nir_src src)
{
   nir_instr *p = nir_def_instr(src.ssa);

   if (p->type == nir_instr_type_intrinsic) {
      nir_intrinsic_op op = nir_instr_as_intrinsic(p)->intrinsic;
      return op == nir_intrinsic_load_input ||
             op == nir_intrinsic_load_interpolated_input;
   }

   /* vecN only gathers, so look through it */
   if (p->type == nir_instr_type_alu) {
      nir_alu_instr *a = nir_instr_as_alu(p);
      if (a->op == nir_op_vec2 || a->op == nir_op_vec3 ||
          a->op == nir_op_vec4) {
         for (unsigned i = 0; i < nir_op_infos[a->op].num_inputs; ++i)
            if (fp_src_is_varying(a->src[i].src))
               return true;
      }
   }

   return false;
}

/* a varying source needs an MFU instruction to interpolate it into a row register */
static void
fp_need_mfu_for(struct fp_nir_ctx *ctx, nir_src src)
{
   if (!fp_src_is_varying(src) || ctx->mfu)
      return;

   ctx->mfu = CALLOC_STRUCT(fp_mfu_instr);
   list_inithead(&ctx->mfu->link);
}

static void
fp_need_mfu(struct fp_nir_ctx *ctx, nir_alu_instr *alu)
{
   unsigned n = nir_op_infos[alu->op].num_inputs;

   for (unsigned i = 0; i < n; ++i)
      fp_need_mfu_for(ctx, alu->src[i].src);
}

static struct fp_alu_dst_operand
fp_dst_for_def(struct fp_nir_ctx *ctx, const nir_def *def, bool saturate)
{
   struct fp_alu_dst_operand d = { 0 };
   d.index = fp_reg_for_slot(ctx, fp_slot_for_def(ctx, def));
   d.write_low_sub_reg = true;
   d.write_high_sub_reg = true;
   d.saturate = saturate;
   return d;
}

/* the colour output lives in r2-r3 as four fx10s, swizzled RGBA -> BGRA */
static struct fp_alu_dst_operand
fp_dst_output(unsigned comp, bool saturate)
{
   struct fp_alu_dst_operand d = { 0 };
   int o = comp < 3 ? (2 - comp) : 3;

   d.index = 2 + o / 2;
   d.write_low_sub_reg = (o % 2) == 0;
   d.write_high_sub_reg = (o % 2) != 0;
   d.saturate = saturate;
   return d;
}

static void
fp_emit_form(struct fp_nir_ctx *ctx, nir_alu_instr *alu, enum fp_alu_op op,
             enum fp_form form, enum fp_condition cond)
{
   struct fp_instr *inst = fp_new_instr();
   struct fp_alu_instr_packet *pkt = fp_new_packet(ctx->fp);
   unsigned n = nir_op_infos[alu->op].num_inputs;
   struct fp_alu_src_operand s[3];

   fp_need_mfu(ctx, alu);

   for (unsigned i = 0; i < n && i < 3; ++i)
      s[i] = fp_nir_alu_src(ctx, alu, i);

   if (alu->op == nir_op_fneg)
      s[0].negate = !s[0].negate;
   if (alu->op == nir_op_fabs)
      s[0].absolute_value = true;

   struct fp_alu_src_operand rA, rB, rC;
   switch (form) {
   case FP_FORM_MOV: rA = s[0]; rB = fp_src_one();  rC = fp_src_zero(); break;
   case FP_FORM_MUL: rA = s[0]; rB = s[1];          rC = fp_src_zero(); break;
   case FP_FORM_ADD: rA = s[0]; rB = fp_src_one();  rC = s[1];          break;
   case FP_FORM_MAD: rA = s[0]; rB = s[1];          rC = s[2];          break;
   case FP_FORM_SUB_REV: rA = s[1]; rB = fp_src_one(); rC = s[0];
                     rC.negate = !rC.negate;                            break;
   case FP_FORM_ONE_MINUS: rA = s[0]; rA.negate = !rA.negate;
                     rB = fp_src_one(); rC = fp_src_one();              break;
   default: UNREACHABLE("bad fragment source form");
   }

   struct fp_alu_instr a = {
      .op = op,
      .condition = cond,
      .dst = fp_dst_for_def(ctx, &alu->def, alu->op == nir_op_fsat),
      /* src[3] mirrors src[2] so the rD selector stays off */
      .src = { rA, rB, rC, rC },
   };
   pkt->slots[0] = a;

   fp_finish(ctx, inst, pkt);
}

/*
 * There is no NOT-EQUAL condition code - the hardware offers only EQUAL,
 * GEQUAL and GREATER - so compute the equality into a scratch register and
 * turn it inside out.
 */
static void
fp_emit_sne(struct fp_nir_ctx *ctx, nir_alu_instr *alu)
{
   unsigned scratch = ctx->num_slots++;

   {
      struct fp_instr *inst = fp_new_instr();
      struct fp_alu_instr_packet *pkt = fp_new_packet(ctx->fp);

      fp_need_mfu(ctx, alu);

      struct fp_alu_src_operand a = fp_nir_alu_src(ctx, alu, 0);
      struct fp_alu_src_operand b = fp_nir_alu_src(ctx, alu, 1);
      struct fp_alu_dst_operand d = { 0 };

      d.index = fp_reg_for_slot(ctx, scratch);
      d.write_low_sub_reg = true;
      d.write_high_sub_reg = true;

      a.negate = !a.negate;   /* rC is negated below, so b - a */
      pkt->slots[0] = (struct fp_alu_instr){
         .op = FP_ALU_OP_MAD,
         .condition = FP_CONDITION_EQUAL,
         .dst = d,
         .src = { b, fp_src_one(), a, a },
      };

      fp_finish(ctx, inst, pkt);
   }

   {
      struct fp_instr *inst = fp_new_instr();
      struct fp_alu_instr_packet *pkt = fp_new_packet(ctx->fp);
      struct fp_alu_src_operand s = { 0 };

      s.index = fp_reg_for_slot(ctx, scratch);
      s.datatype = FP_DATATYPE_FP20;
      s.negate = true;

      pkt->slots[0] = (struct fp_alu_instr){
         .op = FP_ALU_OP_MAD,
         .condition = FP_CONDITION_ALWAYS,
         .dst = fp_dst_for_def(ctx, &alu->def, false),
         .src = { s, fp_src_one(), fp_src_one(), fp_src_one() },
      };

      fp_finish(ctx, inst, pkt);
   }
}

static void
fp_emit_alu(struct fp_nir_ctx *ctx, nir_alu_instr *alu)
{
   switch (alu->op) {
   case nir_op_mov:  fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MOV,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_fmul: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MUL,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_fadd: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_ADD,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_ffma: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MAD,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_fmin: fp_emit_form(ctx, alu, FP_ALU_OP_MIN, FP_FORM_ADD,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_fmax: fp_emit_form(ctx, alu, FP_ALU_OP_MAX, FP_FORM_ADD,
                                  FP_CONDITION_ALWAYS); break;
   /* (a < b) is (b - a > 0); the condition code turns it into 0 or 1 */
   case nir_op_slt:  fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_SUB_REV,
                                  FP_CONDITION_GREATER); break;
   case nir_op_sge:  fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_SUB_REV,
                                  FP_CONDITION_GEQUAL); break;
   case nir_op_seq:  fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_SUB_REV,
                                  FP_CONDITION_EQUAL); break;
   case nir_op_sne:  fp_emit_sne(ctx, alu); break;
   case nir_op_fsat: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MOV,
                                  FP_CONDITION_ALWAYS); break;
   /* negation and absolute value are operand modifiers, not instructions */
   case nir_op_fneg: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MOV,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_fabs: fp_emit_form(ctx, alu, FP_ALU_OP_MAD, FP_FORM_MOV,
                                  FP_CONDITION_ALWAYS); break;
   case nir_op_vec2:
   case nir_op_vec3:
   case nir_op_vec4:
      /* nothing to do: the components are read straight from their sources */
      break;
   default:
      fprintf(stderr, "GRATE FRAG NIR UNIMPLEMENTED: %s\n",
              nir_op_infos[alu->op].name);
      ctx->fp->unsupported = true;
      break;
   }
}

static void
fp_emit_store_output(struct fp_nir_ctx *ctx, nir_intrinsic_instr *intr)
{
   struct grate_fp_shader *fp = ctx->fp;
   unsigned mask = nir_intrinsic_write_mask(intr);
   struct fp_alu_instr slots[4];
   unsigned n = 0;

   fp_need_mfu_for(ctx, intr->src[0]);

   /* resolve every component first: doing so is what decides whether the
    * packet's fourth slot is spent on embedded constants */
   for (unsigned c = 0; c < 4; ++c) {
      if (!(mask & (1u << c)))
         continue;

      struct fp_alu_src_operand src = fp_nir_src(ctx, intr->src[0], c);
      slots[n++] = (struct fp_alu_instr){
         .op = FP_ALU_OP_MAD,
         .condition = FP_CONDITION_ALWAYS,
         .dst = fp_dst_output(c, false),
         .src = { src, fp_src_one(), fp_src_zero(), fp_src_zero() },
      };
   }

   if (n == 0)
      return;

   /* constants ride in the fourth slot, so only three writes fit beside them */
   unsigned per_packet = ctx->num_constants > 0 ? 3 : 4;
   unsigned first = list_length(&fp->alu_instructions);
   unsigned packets = 0;

   for (unsigned i = 0; i < n; i += per_packet) {
      struct fp_alu_instr_packet *pkt = fp_new_packet(fp);
      unsigned count = MIN2(per_packet, n - i);

      for (unsigned k = 0; k < count; ++k)
         pkt->slots[k] = slots[i + k];

      if (ctx->num_constants > 0) {
         pkt->has_constants = true;
         memcpy(pkt->constants, ctx->constants, sizeof(pkt->constants));
      }

      list_addtail(&pkt->link, &fp->alu_instructions);
      packets++;
   }

   if (packets > 3)
      fprintf(stderr, "GRATE FRAG: %u ALU packets exceeds the 3 the scheduler "
                      "can issue\n", packets);

   /*
    * The colour written by the ALU accumulates in R2-R3 across instructions,
    * so the store belongs on the last one rather than on every write. A
    * shader that writes its components in separate statements otherwise
    * stores each partial result, and only the components of the final store
    * are right.
    */
   struct fp_instr *inst = fp_new_instr();
   inst->alu_sched.address = first;
   inst->alu_sched.num_instructions = packets;

   if (ctx->mfu) {
      inst->mfu_sched.address = list_length(&fp->mfu_instructions);
      inst->mfu_sched.num_instructions = 1;
      list_addtail(&ctx->mfu->link, &fp->mfu_instructions);
   }

   list_addtail(&inst->link, &fp->fp_instructions);

   ctx->mfu = NULL;
   ctx->num_constants = 0;
}

static void
fp_emit_tex(struct fp_nir_ctx *ctx, nir_tex_instr *tex)
{
   struct grate_fp_shader *fp = ctx->fp;
   struct fp_instr *inst = fp_new_instr();
   int coord_idx = nir_tex_instr_src_index(tex, nir_tex_src_coord);

   if (coord_idx < 0) {
      fprintf(stderr, "GRATE FRAG NIR: texture with no coordinate\n");
      fp->unsupported = true;
      FREE(inst);
      return;
   }

   nir_src coord = tex->src[coord_idx].src;
   nir_instr *cp = nir_def_instr(coord.ssa);
   bool coord_is_varying = false;
   unsigned row = 0;

   if (cp->type == nir_instr_type_intrinsic) {
      nir_intrinsic_op op = nir_instr_as_intrinsic(cp)->intrinsic;
      if (op == nir_intrinsic_load_input ||
          op == nir_intrinsic_load_interpolated_input) {
         coord_is_varying = true;
         row = nir_intrinsic_base(nir_instr_as_intrinsic(cp));
      }
   }

   /*
    * TEX always reads S and T from row registers 0 and 1. A varying lands
    * there through the MFU; anything else has to be moved there by an ALU
    * packet in a preceding instruction.
    */
   if (coord_is_varying) {
      struct fp_mfu_instr *mfu = CALLOC_STRUCT(fp_mfu_instr);
      list_inithead(&mfu->link);

      for (unsigned c = 0; c < 2; ++c) {
         mfu->var[c].op = FP_VAR_OP_FP20;
         mfu->var[c].tram_row = row;
      }
      fp->info.max_tram_row = MAX2(fp->info.max_tram_row, row);

      inst->mfu_sched.address = list_length(&fp->mfu_instructions);
      inst->mfu_sched.num_instructions = 1;
      list_addtail(&mfu->link, &fp->mfu_instructions);
   } else {
      struct fp_instr *setup = fp_new_instr();
      struct fp_alu_instr_packet *pkt = fp_new_packet(fp);

      for (unsigned c = 0; c < 2; ++c) {
         struct fp_alu_src_operand src = fp_nir_src(ctx, coord, c);
         struct fp_alu_dst_operand d = {
            .index = c,                  /* row register 0 / 1 */
            .write_low_sub_reg = true,
            .write_high_sub_reg = true,
         };
         pkt->slots[c] = (struct fp_alu_instr){
            .op = FP_ALU_OP_MAD,
            .dst = d,
            .src = { src, fp_src_one(), fp_src_zero(), fp_src_zero() },
         };
      }

      if (ctx->num_constants > 0) {
         pkt->has_constants = true;
         memcpy(pkt->constants, ctx->constants, sizeof(pkt->constants));
         ctx->num_constants = 0;
      }

      setup->alu_sched.address = list_length(&fp->alu_instructions);
      setup->alu_sched.num_instructions = 1;
      list_addtail(&pkt->link, &fp->alu_instructions);
      list_addtail(&setup->link, &fp->fp_instructions);
   }

   inst->tex.enable = true;
   inst->tex.sampler = tex->sampler_index;
   inst->tex.dst_r2_r3 = true;
   inst->tex.src_r2_r3 = false;

   list_addtail(&inst->link, &fp->fp_instructions);
}

static void
fp_emit_intrinsic(struct fp_nir_ctx *ctx, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_load_input:
   case nir_intrinsic_load_interpolated_input:
   case nir_intrinsic_load_uniform:
   case nir_intrinsic_load_barycentric_pixel:
      /* resolved where they are used */
      return;

   case nir_intrinsic_store_output:
      fp_emit_store_output(ctx, intr);
      return;

   default:
      fprintf(stderr, "GRATE FRAG NIR UNIMPLEMENTED intrinsic: %s\n",
              nir_intrinsic_infos[intr->intrinsic].name);
      ctx->fp->unsupported = true;
      return;
   }
}

/*
 * Shared tail: never hand the GPU a program we could not translate, and set up
 * the barycentric weights the interpolators need.
 */
void
grate_fp_finish(struct grate_fp_shader *fp)
{
   /*
    * A shader we could not fully translate must not reach the GPU. A half
    * translated program is not merely wrong: gr3d hangs on one, and the
    * kernel then resets it in a loop until the machine goes down. Throw the
    * program away and store zeroes instead, so the surface is wrong but the
    * GPU survives and the log says why.
    */
   if (fp->unsupported) {
      fprintf(stderr, "GRATE FRAG: shader not translatable, substituting a "
                      "stub that writes nothing\n");

      list_for_each_entry_safe(struct fp_instr, i, &fp->fp_instructions, link)
         FREE(i);
      list_for_each_entry_safe(struct fp_alu_instr_packet, p,
                               &fp->alu_instructions, link)
         FREE(p);
      list_for_each_entry_safe(struct fp_mfu_instr, m, &fp->mfu_instructions,
                               link)
         FREE(m);
      list_inithead(&fp->fp_instructions);
      list_inithead(&fp->alu_instructions);
      list_inithead(&fp->mfu_instructions);

      struct fp_instr *inst = CALLOC_STRUCT(fp_instr);
      list_inithead(&inst->link);
      struct fp_alu_instr_packet *pkt = CALLOC_STRUCT(fp_alu_instr_packet);
      list_inithead(&pkt->link);
      for (int i = 0; i < 4; ++i) {
         pkt->slots[i] = (struct fp_alu_instr){
            .op = FP_ALU_OP_MAD,
            .condition = FP_CONDITION_ALWAYS,
            .dst = fp_dst_output(i, false),
            .src = { fp_src_zero(), fp_src_one(),
                     fp_src_zero(), fp_src_zero() },
         };
      }

      inst->alu_sched.address = 0;
      inst->alu_sched.num_instructions = 1;
      inst->dw.enable = 1;
      inst->dw.index = 0;
      inst->dw.src_regs = FP_DW_REGS_R2_R3;

      list_addtail(&pkt->link, &fp->alu_instructions);
      list_addtail(&inst->link, &fp->fp_instructions);
   }

   /*
    * Perspective interpolation needs the barycentric weights computed from
    * 1/w, and grate's reference shaders fold that into the same MFU
    * instruction that issues the interpolation:
    *
    *    MFU: sfu: rcp r4
    *         mul0: bar, sfu, bar0
    *         mul1: bar, sfu, bar1
    *         ipl: t0.fp20, t0.fp20, NOP, NOP
    *
    * A shader with no varyings has no MFU instruction at all, so give it one.
    */
   if (list_is_empty(&fp->mfu_instructions)) {
      struct fp_mfu_instr *mfu = CALLOC_STRUCT(fp_mfu_instr);
      list_inithead(&mfu->link);
      list_addtail(&mfu->link, &fp->mfu_instructions);

      list_for_each_entry(struct fp_instr, inst, &fp->fp_instructions, link) {
         inst->mfu_sched.num_instructions = 1;
         inst->mfu_sched.address = 0;
      }
   }

   /*
    * The weights are consumed by the interpolators in the very instruction
    * that computes them, so every MFU instruction that interpolates needs its
    * own copy of the setup, not just the first one. Giving it only to the
    * first left every later interpolation reading stale weights: a small error
    * in a smooth varying, and a completely wrong value in anything that feeds
    * a texture coordinate or a special function.
    *
    * An MFU instruction that interpolates nothing is left alone - that is
    * where the SFU ops do their own work, and they have no weights to compute.
    */
   list_for_each_entry(struct fp_mfu_instr, mfu, &fp->mfu_instructions, link) {
      bool interpolates = false;
      for (int i = 0; i < 4; ++i)
         if (mfu->var[i].op != FP_VAR_OP_NOP)
            interpolates = true;

      if (!interpolates && mfu->sfu.op != FP_SFU_OP_NOP)
         continue;

      mfu->sfu.op = FP_SFU_OP_RCP;
      mfu->sfu.reg = 4;
      mfu->mul[0].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
      mfu->mul[0].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
      mfu->mul[0].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_0;
      mfu->mul[1].dst = FP_MFU_MUL_DST_BARYCENTRIC_WEIGHT;
      mfu->mul[1].src[0] = FP_MFU_MUL_SRC_SFU_RESULT;
      mfu->mul[1].src[1] = FP_MFU_MUL_SRC_BARYCENTRIC_COEF_1;
   }
}

void
grate_nir_to_fp(struct grate_fp_shader *fp, nir_shader *s)
{
   list_inithead(&fp->fp_instructions);
   list_inithead(&fp->alu_instructions);
   list_inithead(&fp->mfu_instructions);

   fp->num_immediates = 0;
   fp->num_temps = 0;
   fp->unsupported = false;
   fp->tex_temp = -1;
   fp->info.num_inputs = 0;
   fp->info.color_input = -1;
   fp->info.max_tram_row = 1;

   /*
    * One linker entry per varying the shader reads. Only the components the
    * varying actually has are routed: grate's reference linker leaves the
    * rest NOP, and enabling all four shifts what the TRAM delivers.
    */
   nir_foreach_shader_in_variable(var, s) {
      unsigned row = var->data.driver_location;
      unsigned n = glsl_get_vector_elements(glsl_without_array(var->type));
      uint32_t dst = 0;

      for (unsigned i = 0; i < n && i < 4; ++i)
         dst |= LINK_DST(i, i, LINK_DST_FP20);

      fp->info.inputs[fp->info.num_inputs].src = LINK_SRC(1);
      fp->info.inputs[fp->info.num_inputs].dst = dst;
      fp->info.num_inputs++;

      fp->info.max_tram_row = MAX2(fp->info.max_tram_row, row);

      if (var->data.location == VARYING_SLOT_COL0)
         fp->info.color_input = row;
   }

   nir_function_impl *impl = nir_shader_get_entrypoint(s);

   struct fp_nir_ctx ctx = { 0 };
   ctx.fp = fp;
   ctx.ssa_slot = MALLOC(impl->ssa_alloc * sizeof(int));
   for (unsigned i = 0; i < impl->ssa_alloc; ++i)
      ctx.ssa_slot[i] = -1;

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         switch (instr->type) {
         case nir_instr_type_alu:
            fp_emit_alu(&ctx, nir_instr_as_alu(instr));
            break;
         case nir_instr_type_intrinsic:
            fp_emit_intrinsic(&ctx, nir_instr_as_intrinsic(instr));
            break;
         case nir_instr_type_tex:
            fp_emit_tex(&ctx, nir_instr_as_tex(instr));
            break;
         case nir_instr_type_load_const:
         case nir_instr_type_undef:
         case nir_instr_type_jump:
            break;
         default:
            fprintf(stderr, "GRATE FRAG NIR: unhandled instruction type %d\n",
                    instr->type);
            fp->unsupported = true;
            break;
         }
      }
   }

   FREE(ctx.ssa_slot);

   /* store what the ALU built up in R2-R3, once, at the end */
   if (!list_is_empty(&fp->fp_instructions)) {
      struct fp_instr *last =
         list_last_entry(&fp->fp_instructions, struct fp_instr, link);

      last->dw.enable = 1;
      last->dw.index = 0;
      last->dw.stencil_write = 0;
      last->dw.src_regs = FP_DW_REGS_R2_R3;
   }

   if (ctx.overflow)
      fp->unsupported = true;

   grate_fp_finish(fp);
}
