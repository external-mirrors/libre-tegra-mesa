#include "test_ir.h"
#include "util/ralloc.h"
#include "util/list.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------
 * Function creation
 * --------------------------------------------------- */
struct vnir_function *
vnir_function_create(void *mem_ctx, unsigned num_vec4_regs, unsigned num_pred_regs)
{
    struct vnir_function *func = rzalloc(mem_ctx, struct vnir_function);
    func->mem_ctx = mem_ctx;
    list_inithead(&func->blocks);
    func->num_blocks = 0;
    func->next_ssa_index = 0;
    func->num_vec4_regs = num_vec4_regs;
    func->num_pred_regs = num_pred_regs;
    return func;
}

/* ---------------------------------------------------
 * Block creation
 * --------------------------------------------------- */
struct vnir_block *
vnir_block_create(struct vnir_function *func)
{
    struct vnir_block *block = rzalloc(func, struct vnir_block);
    list_inithead(&block->instr_list);
    block->parent_function = func;
    block->index = func->num_blocks++;
    list_addtail(&func->blocks, &block->node);
    return block;
}

/* ---------------------------------------------------
 * SSA definition creation
 * --------------------------------------------------- */
struct vnir_def *
vnir_def_create(struct vnir_function *func, enum vnir_type type, uint8_t num_components)
{
    struct vnir_def *def = rzalloc(func, struct vnir_def);
    def->type = type;
    def->num_components = num_components;
    def->ssa_index = func->next_ssa_index++;
    def->hw_reg = -1;
    def->reg_mask = 0xF;
    list_inithead(&def->uses);
    return def;
}

/* ---------------------------------------------------
 * ALU instruction creation
 * --------------------------------------------------- */
struct vnir_alu_instr *
vnir_alu_instr_create(struct vnir_block *block, int op,
                      enum vnir_alu_slot slot, unsigned num_srcs)
{
    size_t size = sizeof(struct vnir_alu_instr) +
                  num_srcs * sizeof(struct vnir_src);
    struct vnir_alu_instr *alu = rzalloc_size(block->parent_function, size);
    alu->instr.type = VNIR_INSTR_ALU;
    alu->instr.parent_block = block;
    alu->op.op = op;
    alu->slot = slot;
    alu->num_srcs = num_srcs;
    list_addtail(&block->instr_list, &alu->instr.node);
    return alu;
}

/* ---------------------------------------------------
 * Phi instruction creation
 * --------------------------------------------------- */
struct vnir_phi_instr *
vnir_phi_instr_create(struct vnir_block *block, unsigned num_srcs)
{
    size_t size = sizeof(struct vnir_phi_instr) +
                  num_srcs * sizeof(struct vnir_phi_src);
    struct vnir_phi_instr *phi = rzalloc_size(block->parent_function, size);
    phi->instr.type = VNIR_INSTR_PHI;
    phi->instr.parent_block = block;
    phi->num_srcs = num_srcs;
    list_addtail(&block->instr_list, &phi->instr.node);
    return phi;
}

/* ---------------------------------------------------
 * Jump instruction creation
 * --------------------------------------------------- */
struct vnir_jump_instr *
vnir_jump_instr_create(struct vnir_block *block,
                       enum vnir_jump_type type,
                       struct vnir_block *target)
{
    struct vnir_jump_instr *jump = rzalloc(block->parent_function, struct vnir_jump_instr);
    jump->instr.type = VNIR_INSTR_JUMP;
    jump->instr.parent_block = block;
    jump->type = type;
    jump->target = target;
    list_addtail(&block->instr_list, &jump->instr.node);
    return jump;
}

/* ---------------------------------------------------
 * Add a source to an ALU instruction
 * --------------------------------------------------- */
void
vnir_alu_src_init(struct vnir_alu_instr *alu, unsigned idx,
                  struct vnir_def *def,
                  uint8_t swizzle[4], bool abs, bool neg)
{
    if (idx >= alu->num_srcs)
        return;
    alu->src[idx].def = def; /* NULL allowed for load/store pseudo-source */
    memcpy(alu->src[idx].swizzle, swizzle, 4);
    alu->src[idx].abs = abs;
    alu->src[idx].neg = neg;

    if (def)
        list_add(&alu->src[idx].use_link, &def->uses);
}

/* ---------------------------------------------------
 * Destination initialization
 * --------------------------------------------------- */
void
vnir_dst_init(struct vnir_dst *dst, struct vnir_def *def,
              uint8_t writemask, bool saturate)
{
    dst->def = *def;
    dst->writemask = writemask;
    dst->saturate = saturate;
}