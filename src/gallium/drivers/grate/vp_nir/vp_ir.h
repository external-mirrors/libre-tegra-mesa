#ifndef VP_IR_H
#define VP_IR_H


#include "grate_compiler.h"
#include "util/list.h"

#include "stdbool.h"
#include "stdint.h"


// Stupid copy; share via header
enum reg_class {
   REG_CLASS_VIRT_SCALAR,
   REG_CLASS_VIRT_VEC2,
   REG_CLASS_VIRT_VEC3,
   REG_CLASS_VEC4,
   NUM_REG_CLASSES,
};

enum reg_type {
   REG_TYPE_VEC4,
   REG_TYPE_VIRT_VEC3_XYZ,
   REG_TYPE_VIRT_VEC3_XYW,
   REG_TYPE_VIRT_VEC3_XZW,
   REG_TYPE_VIRT_VEC3_YZW,
   REG_TYPE_VIRT_VEC2_XY,
   REG_TYPE_VIRT_VEC2_XZ,
   REG_TYPE_VIRT_VEC2_XW,
   REG_TYPE_VIRT_VEC2_YZ,
   REG_TYPE_VIRT_VEC2_YW,
   REG_TYPE_VIRT_VEC2_ZW,
   REG_TYPE_VIRT_SCALAR_X,
   REG_TYPE_VIRT_SCALAR_Y,
   REG_TYPE_VIRT_SCALAR_Z,
   REG_TYPE_VIRT_SCALAR_W,
   NUM_REG_TYPES,
};



enum vp_src_file {
   VP_SRC_FILE_UNDEF = 0,
   VP_SRC_FILE_TEMP = 1,
   VP_SRC_FILE_ATTRIB = 2,
   VP_SRC_FILE_UNIFORM = 3,
};

enum vp_dst_file {
   VP_DST_FILE_TEMP,
   VP_DST_FILE_OUTPUT,
   VP_DST_FILE_UNDEF
};

enum vp_swz {
   VP_SWZ_X = 0,
   VP_SWZ_Y = 1,
   VP_SWZ_Z = 2,
   VP_SWZ_W = 3
};

enum vp_vec_op {
   VP_VEC_OP_NOP = 0,
   VP_VEC_OP_MOV = 1,
   VP_VEC_OP_MUL = 2,
   VP_VEC_OP_ADD = 3,
   VP_VEC_OP_MAD = 4,
   VP_VEC_OP_DP3 = 5,
   VP_VEC_OP_DPH = 6,
   VP_VEC_OP_DP4 = 7,
   VP_VEC_OP_DST = 8,
   VP_VEC_OP_MIN = 9,
   VP_VEC_OP_MAX = 10,
   VP_VEC_OP_SLT = 11,
   VP_VEC_OP_SGE = 12,
   VP_VEC_OP_ARL = 13,
   VP_VEC_OP_FRC = 14,
   VP_VEC_OP_FLR = 15,
   VP_VEC_OP_SEQ = 16,
   VP_VEC_OP_SFL = 17,
   VP_VEC_OP_SGT = 18,
   VP_VEC_OP_SLE = 19,
   VP_VEC_OP_SNE = 20,
   VP_VEC_OP_STR = 21,
   VP_VEC_OP_SSG = 22,
   VP_VEC_OP_ARR = 23,
   VP_VEC_OP_ARA = 24,
   VP_VEC_OP_TXL = 25,
   VP_VEC_OP_PUSHA = 26,
   VP_VEC_OP_POPA = 27
};

enum vp_scalar_op {
   VP_SCALAR_OP_NOP = 0,
   VP_SCALAR_OP_MOV = 1,
   VP_SCALAR_OP_RCP = 2,
   VP_SCALAR_OP_RCC = 3,
   VP_SCALAR_OP_RSQ = 4,
   VP_SCALAR_OP_EXP = 5,
   VP_SCALAR_OP_LOG = 6,
   VP_SCALAR_OP_LIT = 7,
   VP_SCALAR_OP_BRA = 9,
   VP_SCALAR_OP_CAL = 11,
   VP_SCALAR_OP_RET = 12,
   VP_SCALAR_OP_LG2 = 13,
   VP_SCALAR_OP_EX2 = 14,
   VP_SCALAR_OP_SIN = 15,
   VP_SCALAR_OP_COS = 16,
   VP_SCALAR_OP_PUSHA = 19,
   VP_SCALAR_OP_POPA = 20
};

struct vp_dst_operand {
   enum vp_dst_file file;
   int index;
   int virt_id;
   int hw_id;
   unsigned int write_mask;
   enum reg_class reg_class;
   enum reg_type reg_type;
   bool saturate;
};

struct vp_src_operand {
   enum vp_src_file file;
   int index;
   int virt_id;
   int hw_id;
   enum vp_swz swizzle[4];
   enum reg_class reg_class;
   enum reg_type reg_type;
   bool negate, absolute;

};

struct vp_vec_instr {
   enum vp_vec_op op;
   struct vp_dst_operand dst;
   struct vp_src_operand src[3];
};

struct vp_scalar_instr {
   enum vp_scalar_op op;
   struct vp_dst_operand dst;
   struct vp_src_operand src;
};

struct vp_instr {
   struct list_head link;
   struct vp_vec_instr vec;
   struct vp_scalar_instr scalar;
};

struct vp_vec_instr
emit_vNOP(void);

void
grate_dump_ir(struct grate_vp_shader *vp);

void
grate_vp_pack(uint32_t *dst, struct vp_instr *instr, bool end_of_program);

struct vp_instr *
emit_packed(struct vp_vec_instr vec, struct vp_scalar_instr scalar);

struct vp_vec_instr
emit_vADD(struct vp_dst_operand dst, struct vp_src_operand src0,
          struct vp_src_operand src2);


struct vp_vec_instr
emit_vec_unop(enum vp_vec_op op, struct vp_dst_operand dst,
              struct vp_src_operand src);

struct vp_vec_instr
emit_vec_binop(enum vp_vec_op op, struct vp_dst_operand dst,
              struct vp_src_operand src0, struct vp_src_operand src1);



struct vp_vec_instr
emit_vMOV(struct vp_dst_operand dst, struct vp_src_operand src);


struct vp_vec_instr
emit_vMAD(struct vp_dst_operand dst, struct vp_src_operand src0,
          struct vp_src_operand src1, struct vp_src_operand src2);


struct vp_scalar_instr
emit_sNOP(void);

struct vp_dst_operand
emit_output(struct grate_vp_shader *vp, int index,
            unsigned int write_mask, bool saturate);

struct vp_src_operand
src_temp(int index, const enum vp_swz swizzle[4]);

struct vp_dst_operand
dst_temp(int index, enum reg_class reg_class);
#endif
