#include "grate_compiler.h"
#include "unistd.h"
#include "vp_ir.h"

#include "util/u_memory.h"

static struct vp_src_operand
src_undef()
{
   struct vp_src_operand ret = {
      .file = VP_SRC_FILE_UNDEF,
      .index = 0,
      .swizzle = { VP_SWZ_X, VP_SWZ_Y, VP_SWZ_Z, VP_SWZ_W }
   };
   return ret;
}

static struct vp_src_operand
attrib(int index, const enum vp_swz swizzle[4], bool negate, bool absolute)
{
   struct vp_src_operand ret = {
      .file = VP_SRC_FILE_ATTRIB,
      .index = index,
      .negate = negate,
      .absolute = absolute
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vp_src_operand
uniform(int index, const enum vp_swz swizzle[4], bool negate, bool absolute)
{
   struct vp_src_operand ret = {
      .file = VP_SRC_FILE_UNIFORM,
      .index = index,
      .negate = negate,
      .absolute = absolute
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

struct vp_src_operand
src_temp(int index, const enum vp_swz swizzle[4])
{
   struct vp_src_operand ret = {
      .file = VP_SRC_FILE_TEMP,
      .virt_id = index,
      .negate = false,
      .absolute = false
   };
   memcpy(ret.swizzle, swizzle, sizeof(ret.swizzle));
   return ret;
}

static struct vp_dst_operand
dst_undef()
{
   struct vp_dst_operand ret = {
      .file = VP_DST_FILE_UNDEF,
      .index = 0,
      .write_mask = 0,
      .saturate = 0
   };
   return ret;
}

struct vp_dst_operand
emit_output(struct grate_vp_shader *vp, int index,
            unsigned int write_mask, bool saturate)
{
   vp->output_mask |= 1 << index;
   struct vp_dst_operand ret = {
      .file = VP_DST_FILE_OUTPUT,
      .index = index,
      .write_mask = write_mask,
      .saturate = saturate
   };
   return ret;
}

struct vp_dst_operand
dst_temp(int index, enum reg_class reg_class)
{
   struct vp_dst_operand ret = {
      .file = VP_DST_FILE_TEMP,
      .virt_id = index,
      .write_mask = 0,
      .saturate = false,
      .reg_class = reg_class
   };
   return ret;
}

struct vp_vec_instr
emit_vec_unop(enum vp_vec_op op, struct vp_dst_operand dst,
              struct vp_src_operand src)
{
   struct vp_vec_instr ret = {
      .op = op,
      .dst = dst,
      .src = { src, src_undef(), src_undef() }
   };
   return ret;
}

struct vp_vec_instr
emit_vec_binop(enum vp_vec_op op, struct vp_dst_operand dst,
              struct vp_src_operand src0, struct vp_src_operand src1)
{
   struct vp_vec_instr ret = {
      .op = op,
      .dst = dst,
      .src = { src0, src1, src_undef() }
   };
   return ret;
}

struct vp_vec_instr
emit_vNOP(void)
{
   struct vp_vec_instr ret = {
      .op = VP_VEC_OP_NOP,
      .dst = dst_undef(),
      .src = { src_undef(), src_undef(), src_undef() }
   };
   return ret;
}

struct vp_vec_instr
emit_vMOV(struct vp_dst_operand dst, struct vp_src_operand src)
{
   return emit_vec_unop(VP_VEC_OP_MOV, dst, src);
}

struct vp_vec_instr
emit_vADD(struct vp_dst_operand dst, struct vp_src_operand src0,
          struct vp_src_operand src2)
{
   struct vp_vec_instr ret = {
      .op = VP_VEC_OP_ADD,
      .dst = dst,
      .src = { src0, src_undef(), src2 } // add is "strange" in that it takes src0 and src2
   };
   return ret;
}

#define GEN_V_BINOP(OP) \
static struct vp_vec_instr \
emit_v ## OP (struct vp_dst_operand dst, struct vp_src_operand src0, \
          struct vp_src_operand src1) \
{ \
   return emit_vec_binop(VP_VEC_OP_ ## OP, dst, src0, src1); \
}

GEN_V_BINOP(MUL)
GEN_V_BINOP(DP3)
GEN_V_BINOP(DP4)
GEN_V_BINOP(SLT)
GEN_V_BINOP(MAX)

struct vp_vec_instr
emit_vMAD(struct vp_dst_operand dst, struct vp_src_operand src0,
          struct vp_src_operand src1, struct vp_src_operand src2)
{
   struct vp_vec_instr ret = {
      .op = VP_VEC_OP_MAD,
      .dst = dst,
      .src = { src0, src1, src2 }
   };
   return ret;
}

struct vp_scalar_instr
emit_sNOP(void)
{
   struct vp_scalar_instr ret = {
      .op = VP_SCALAR_OP_NOP,
      .dst = dst_undef(),
      .src = src_undef()
   };
   return ret;
}

#define GEN_S_UNOP(OP) \
static struct vp_scalar_instr \
emit_s ## OP (struct vp_dst_operand dst, struct vp_src_operand src) \
{ \
   struct vp_scalar_instr ret = { \
      .op = VP_SCALAR_OP_ ## OP, \
      .dst = dst, \
      .src = src \
   }; \
   return ret; \
}

GEN_S_UNOP(RSQ)

struct vp_instr *
emit_packed(struct vp_vec_instr vec, struct vp_scalar_instr scalar)
{
   struct vp_instr *ret = CALLOC_STRUCT(vp_instr);
   list_inithead(&ret->link);
   ret->vec = vec;
   ret->scalar = scalar;
   return ret;
}