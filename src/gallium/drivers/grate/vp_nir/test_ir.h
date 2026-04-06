/*
 * vnir.h — Mesa-style Vec4-native SSA IR for VLIW ES2.0 backend
 *
 * Updated: load/store intrinsics are now encoded as ALU sources
 */

#ifndef TESTIR_H
#define TESTIR_H

#include "util/list.h"
#include "util/bitset.h"
#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------
 * 1. Base types
 * --------------------------------------------------- */
enum vnir_type {
    VNIR_TYPE_VEC4,
    VNIR_TYPE_VEC3,
    VNIR_TYPE_VEC2,
    VNIR_TYPE_SCALAR,
    VNIR_TYPE_PREDICATE,
};

/* ---------------------------------------------------
 * 2. SSA Definition
 * --------------------------------------------------- */
struct vnir_def {
    struct list_head node;      /* use in def lists */
    enum vnir_type type;
    uint32_t ssa_index;             /* SSA index */
    uint8_t num_components;     /* 1 or 4 */
    uint32_t live_start;
    uint32_t live_end;
    int hw_reg;                    /* physical register, -1 if unallocated */
    uint8_t reg_mask;           /* for vec4 writes */
    struct list_head uses;      /* list of vnir_src */
};

/* ---------------------------------------------------
 * 3. Source Operand
 * --------------------------------------------------- */
struct vnir_src {
    struct list_head use_link;  /* link into def->uses */
    struct vnir_def *def;       /* if NULL, treated as constant/memory pseudo-source */
    uint8_t swizzle[4];         /* xyzw as 0..3 */
    bool abs;
    bool neg;
};

/* ---------------------------------------------------
 * 4. Destination
 * --------------------------------------------------- */
struct vnir_dst {
    struct vnir_def def;
    uint8_t writemask;          /* 4-bit mask */
    bool saturate;
};

/* ---------------------------------------------------
 * 5. Instructions
 * --------------------------------------------------- */
enum vnir_instr_type {
    VNIR_INSTR_ALU,
    VNIR_INSTR_PHI,
    VNIR_INSTR_JUMP,
};

struct vnir_instr {
    struct list_head node;
    enum vnir_instr_type type;
    struct vnir_block *parent_block;
    struct vnir_def *predicate;   /* optional predicate def */
};

/* ---------------------------------------------------
 * 5a. ALU Instructions
 * --------------------------------------------------- */
enum vnir_alu_slot {
    VNIR_SLOT_VECTOR,
    VNIR_SLOT_SCALAR,
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
   VP_VEC_OP_MVA = 24,
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
   VP_SCALAR_OP_BRA = 8,
   VP_SCALAR_OP_BRI = 9,
   VP_SCALAR_OP_CLA = 10,
   VP_SCALAR_OP_CLI = 11,
   VP_SCALAR_OP_RET = 12,
   VP_SCALAR_OP_LG2 = 13,
   VP_SCALAR_OP_EX2 = 14,
   VP_SCALAR_OP_SIN = 15,
   VP_SCALAR_OP_COS = 16,
   VP_SCALAR_OP_BRB = 17,
   VP_SCALAR_OP_CLB = 18,
   VP_SCALAR_OP_PUSHA = 19,
   VP_SCALAR_OP_POPA = 20
};

struct vnir_alu_instr {
    struct vnir_instr instr;
    enum vnir_alu_slot slot;
    union op{
        enum vp_vec_op vec_op;
        enum vp_scalar_op scalar_op;
        int op;
    } op;
    struct vnir_dst dst;
    unsigned num_srcs;
    struct vnir_src src[3];
};

/* ---------------------------------------------------
 * 5b. Phi Instruction
 * --------------------------------------------------- */
struct vnir_phi_src {
    struct vnir_block *pred;
    struct vnir_src src;
};

struct vnir_phi_instr {
    struct vnir_instr instr;
    struct vnir_dst dst;
    unsigned num_srcs;
    struct vnir_phi_src src[];
};

/* ---------------------------------------------------
 * 5c. Jump Instructions
 * --------------------------------------------------- */
enum vnir_jump_type {
    VNIR_JUMP_BRANCH,
    VNIR_JUMP_BREAK,
    VNIR_JUMP_CONTINUE,
    VNIR_JUMP_RETURN,
};

struct vnir_jump_instr {
    struct vnir_instr instr;
    enum vnir_jump_type type;
    struct vnir_block *target; /* for branch */
};

/* ---------------------------------------------------
 * 6. Basic Block
 * --------------------------------------------------- */
struct vnir_block {
    struct list_head node;
    struct list_head instr_list;  /* vnir_instr */
    struct vnir_function *parent_function;

    BITSET_WORD *live_in;
    BITSET_WORD *live_out;

    unsigned index;

    struct vnir_block **predecessors;
    unsigned num_preds;
    struct vnir_block **successors;
    unsigned num_succs;
};

/* ---------------------------------------------------
 * 7. Function
 * --------------------------------------------------- */
struct vnir_function {
    struct list_head node;
    struct list_head blocks;      /* vnir_block */
    unsigned num_blocks;
    unsigned next_ssa_index;

    unsigned num_vec4_regs;       /* ~32 vec4 */
    unsigned num_pred_regs;

    void *mem_ctx;
};

/* ---------------------------------------------------
 * 8. VLIW Bundle (post-scheduling)
 * --------------------------------------------------- */
struct vnir_bundle {
    struct vnir_alu_instr *vector;
    struct vnir_alu_instr *scalar;
};











struct vnir_function *
vnir_function_create(void *mem_ctx, unsigned num_vec4_regs, unsigned num_pred_regs);

struct vnir_block *
vnir_block_create(struct vnir_function *func);

struct vnir_def *
vnir_def_create(struct vnir_function *func, enum vnir_type type, uint8_t num_components);

struct vnir_alu_instr *
vnir_alu_instr_create(struct vnir_block *block, int op,
                      enum vnir_alu_slot slot, unsigned num_srcs);

struct vnir_phi_instr *
vnir_phi_instr_create(struct vnir_block *block, unsigned num_srcs);

struct vnir_jump_instr *
vnir_jump_instr_create(struct vnir_block *block,
                       enum vnir_jump_type type,
                       struct vnir_block *target);

void
vnir_alu_src_init(struct vnir_alu_instr *alu, unsigned idx,
                  struct vnir_def *def,
                  uint8_t swizzle[4], bool abs, bool neg);

void
vnir_dst_init(struct vnir_dst *dst, struct vnir_def *def,
              uint8_t writemask, bool saturate);


#endif /* VNIR_H */