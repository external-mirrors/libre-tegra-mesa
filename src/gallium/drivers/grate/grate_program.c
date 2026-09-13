#include <stdio.h>
#include <string.h>

#include "util/u_dynarray.h"
#include "util/u_bitcast.h"
#include "util/u_memory.h"

#include "nir/nir_to_tgsi.h"
#include "compiler/nir/nir.h"
#include "grate_nir.h"

#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_transform.h"

#include "grate_device.h"
#include "host1x01_hardware.h"
#include "grate_common.h"
#include "grate_context.h"
#include "grate_screen.h"
#include "grate_program.h"
#include "grate_compiler.h"
#include "fp/fpir.h"
#include "vp/vpir.h"

struct grate_vs_tgsi_transform_context {
   struct tgsi_transform_context base;
   unsigned next_temp;
};

static void
grate_vs_tgsi_transform_instruction(struct tgsi_transform_context *ctx,
                                    struct tgsi_full_instruction *inst)
{
   struct grate_vs_tgsi_transform_context *gctx =
      (struct grate_vs_tgsi_transform_context *)ctx;

   int const_index = -1;
   for (unsigned i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      if (inst->Src[i].Register.File != TGSI_FILE_CONSTANT)
         continue;

      if (const_index < 0)
         const_index = inst->Src[i].Register.Index;
      else if (inst->Src[i].Register.Index != const_index) {
         /* We already have a constant-reads in this instruction,
          * add extra instructions in order to read more
          */
         unsigned temp = gctx->next_temp++;
         tgsi_transform_temp_decl(ctx, temp);
         /* insert MOV to temp */
         tgsi_transform_op1_inst(ctx, TGSI_OPCODE_MOV,
                                 /* dst reg */
                                 TGSI_FILE_TEMPORARY, temp,
                                 TGSI_WRITEMASK_XYZW,
                                 /* src reg */
                                 TGSI_FILE_CONSTANT,
                                 inst->Src[i].Register.Index);

         /* rewrite instruction */
         tgsi_transform_src_reg_xyzw(&inst->Src[i], TGSI_FILE_TEMPORARY, temp);
         inst->Src[i].Register.File = TGSI_FILE_TEMPORARY;
         inst->Src[i].Register.Index = temp;
      }
   }

   ctx->emit_instruction(ctx, inst);
}

static struct tgsi_token *
grate_vs_tgsi_transform(const struct tgsi_token *tokens_in)
{
   grate_trace();
   const unsigned new_len = tgsi_num_tokens(tokens_in) + 100;

   struct tgsi_shader_info info;
   tgsi_scan_shader(tokens_in, &info);

   struct grate_vs_tgsi_transform_context ctx = {};
   ctx.base.transform_instruction = grate_vs_tgsi_transform_instruction;
   ctx.next_temp = info.file_max[TGSI_FILE_TEMPORARY] + 1;

   return tgsi_transform_shader(tokens_in, new_len, &ctx.base);
}


static void *
grate_create_vs_state(struct pipe_context *pcontext,
                      const struct pipe_shader_state *template)
{
   grate_trace();
   struct grate_vertex_shader_state *so =
      CALLOC_STRUCT(grate_vertex_shader_state);

   if (!so)
      return NULL;

   so->base = *template;

   struct grate_vp_shader vp;
   bool use_nir = template->type == PIPE_SHADER_IR_NIR &&
                  !getenv("GRATE_VS_TGSI");

   if (use_nir) {
      /* straight from NIR: no nir_to_tgsi round trip */
      nir_shader *s = template->ir.nir;

      grate_nir_lower_vs(s);
      grate_nir_to_vp(&vp, s);
   } else {
      if (template->type == PIPE_SHADER_IR_NIR) {
         so->base.tokens = nir_to_tgsi(template->ir.nir,
                                       pcontext->screen);
         so->base.type = PIPE_SHADER_IR_TGSI;
      }

      if (grate_debug & GRATE_DEBUG_TGSI) {
         fprintf(stderr, "DEBUG: TGSI:\n");
         tgsi_dump(so->base.tokens, 0);
         fprintf(stderr, "\n");
      }

      struct tgsi_token *new_tokens = grate_vs_tgsi_transform(so->base.tokens);
      if (!new_tokens)
         return NULL;

      struct tgsi_parse_context parser;
      unsigned ok = tgsi_parse_init(&parser, new_tokens);
      assert(ok == TGSI_PARSE_OK);

      grate_tgsi_to_vp(&vp, &parser);
   }

   if (getenv("GRATE_VS_TRACE")) {
      int idx = 0;
      fprintf(stderr, "GRATE VP PROGRAM (%s):\n", use_nir ? "nir" : "tgsi");
      list_for_each_entry(struct vp_instr, vi, &vp.instructions, link) {
         fprintf(stderr, "  [%2d] vec op=%d dst(f=%d i=%d m=0x%x)"
                 " src0(f=%d i=%d) src1(f=%d i=%d) src2(f=%d i=%d)"
                 " | scl op=%d dst(f=%d i=%d m=0x%x) src(f=%d i=%d)\n", idx++,
                 vi->vec.op, vi->vec.dst.file, vi->vec.dst.index,
                 vi->vec.dst.write_mask,
                 vi->vec.src[0].file, vi->vec.src[0].index,
                 vi->vec.src[1].file, vi->vec.src[1].index,
                 vi->vec.src[2].file, vi->vec.src[2].index,
                 vi->scalar.op, vi->scalar.dst.file, vi->scalar.dst.index,
                 vi->scalar.dst.write_mask,
                 vi->scalar.src.file, vi->scalar.src.index);
      }
      fprintf(stderr, "  output_mask=0x%x immediates=%u\n",
              vp.output_mask, vp.num_immediates);
   }

   int num_instructions = list_length(&vp.instructions);
   assert(num_instructions < 256);
   /* each immediate needs an offset write plus a 4-word vec4 upload */
   int num_imm_commands = vp.num_immediates * 6;
   int num_commands = 2 + num_instructions * 4 + num_imm_commands;
   uint32_t *commands = MALLOC(num_commands * sizeof(uint32_t));
   if (!commands) {
      FREE(so);
      return NULL;
   }

   commands[0] = host1x_opcode_imm(REG_TGR3D_VPE_INST_OFFSET, 0);
   commands[1] = host1x_opcode_nonincr(REG_TGR3D_VPE_INST_DATA,
                                       num_instructions * 4);

   struct vp_instr *last = list_last_entry(&vp.instructions, struct vp_instr, link);
   int offset = 2;
   list_for_each_entry(struct vp_instr, instr, &vp.instructions, link) {
      bool end_of_program = instr == last;
      grate_vp_pack(commands + offset, instr, end_of_program);
      offset += 4;
   }

   /*
    * Upload the immediates into the top of the vertex constant file, where
    * GRATE_VP_IMMEDIATE_SLOT() told the compiler to read them from.
    * REG_TGR3D_VPE_CONST_OFFSET counts words, so a vec4 slot is 4 words in.
    */
   for (unsigned i = 0; i < vp.num_immediates; ++i) {
      commands[offset++] =
         host1x_opcode_imm(REG_TGR3D_VPE_CONST_OFFSET,
                           GRATE_VP_IMMEDIATE_SLOT(i) * 4);
      commands[offset++] = host1x_opcode_nonincr(REG_TGR3D_VPE_CONST_DATA, 4);
      for (int c = 0; c < 4; ++c)
         commands[offset++] = u_bitcast_f2u(vp.immediates[i][c]);
   }

   assert(offset == num_commands);

   so->blob.commands = commands;
   so->blob.num_commands = num_commands;
   so->output_mask = vp.output_mask;

   return so;
}

static void
grate_bind_vs_state(struct pipe_context *pcontext, void *so)
{
   grate_trace();
   grate_context(pcontext)->vshader = so;
}

static void
grate_delete_vs_state(struct pipe_context *pcontext, void *so)
{
   grate_trace();
   FREE(so);
}

static void *
grate_create_fs_state(struct pipe_context *pcontext,
                      const struct pipe_shader_state *template)
{
   grate_trace();
   struct grate_context *context = grate_context(pcontext);
   struct grate_fragment_shader_state *so = CALLOC_STRUCT(grate_fragment_shader_state);
   enum drm_tegra_soc_id soc_id = context->drm->soc_id;

   if (!so)
      return NULL;

   so->base = *template;

   if (template->type == PIPE_SHADER_IR_NIR) {
      so->base.tokens = nir_to_tgsi(template->ir.nir,
                                    pcontext->screen);
      so->base.type = PIPE_SHADER_IR_TGSI;
   }


   if (grate_debug & GRATE_DEBUG_TGSI) {
      fprintf(stderr, "DEBUG: TGSI:\n");
      tgsi_dump(so->base.tokens, 0);
      fprintf(stderr, "\n");
   }

   struct tgsi_parse_context parser;
   unsigned ok = tgsi_parse_init(&parser, so->base.tokens);
   assert(ok == TGSI_PARSE_OK);

   struct grate_fp_shader fp;
   grate_tgsi_to_fp(&fp, &parser);

   struct util_dynarray buf;
   util_dynarray_init(&buf, NULL);

#define PUSH(x) util_dynarray_append_typed(&buf, uint32_t, (x))
   /*
    * This is libgrate's ALU_BUFFER_SIZE register: NUM_ROWS holds
    * alu_buffer_size - 1, and MAX_QID the number of quads that fit in the
    * sequencer's output window.
    */
   PUSH(host1x_opcode_incr(REG_TGR3D_GLOBAL_PIX_ATTR, 1));
   PUSH(TGR3D_GLOBAL_PIX_ATTR_NUM_ROWS(GRATE_ALU_BUFFER_SIZE - 1) |
        TGR3D_GLOBAL_PIX_ATTR_MAX_QID((GRATE_PSEQ_MAX_OUT - 1) /
                                      (GRATE_ALU_BUFFER_SIZE * 4)));

   PUSH(host1x_opcode_imm(REG_TGR3D_PSEQ_QUAD_ID, 0));
   PUSH(host1x_opcode_imm(REG_TGR3D_GLOBAL_INST_OFFSET, 0));
   PUSH(host1x_opcode_imm(REG_TGR3D_AT_INST_OFFSET, 0));
   PUSH(host1x_opcode_imm(REG_TGR3D_ALU_INST_OFFSET, 0));

   /*
    * These were asserts, which the packaged build compiles out with
    * b_ndebug, so an oversized program quietly wrote schedule fields that do
    * not fit their 6 bits and handed the GPU a malformed stream. Say so
    * loudly instead: a wrong picture is recoverable, a hung gr3d on a Tegra 3
    * takes the machine with it.
    */
   int num_fp_instrs = list_length(&fp.fp_instructions);
   if (num_fp_instrs >= 64) {
      fprintf(stderr, "GRATE FRAG: %d instructions exceeds the 63 the pixel "
                      "sequencer can address; this shader will render wrong\n",
              num_fp_instrs);
      num_fp_instrs = 63;
   }

   /*
    * pseq_to_dw_exec_nb is the number of DW executions, not the instruction
    * count: grate's reference shaders use 1 for both a one-EXEC and a two-EXEC
    * program, and each has exactly one "DW: store".
    */
   int num_dw_instrs = 0;
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link)
      if (instr->dw.enable)
         num_dw_instrs++;

   PUSH(host1x_opcode_incr(REG_TGR3D_PSEQ_COMMAND_EVEN(0), 1));
   // TODO: document/convert the remaining bits
   PUSH(0x20006000 | num_fp_instrs);

   PUSH(host1x_opcode_incr(REG_TGR3D_PSEQ_DWR_IF_STATE, 1));
   /* START=0, COUNT=1. Feeding num_dw_instrs in here instead was measured to
    * change nothing, so the hardware evidently does not want the store count. */
   PUSH(TGR3D_PSEQ_DWR_IF_STATE_START(0) | TGR3D_PSEQ_DWR_IF_STATE_COUNT(1));

   if (soc_id == DRM_TEGRA_SOC_T114) {
      // TODO: document/convert these values
      /* XXX: maybe not needed */
      PUSH(host1x_opcode_incr(0x547, 0x0002));
      PUSH(0xc0000000);
      PUSH(0x00000000);
   }

   PUSH(host1x_opcode_imm(REG_TGR3D_PSEQ_FLUSH, 0));

   PUSH(host1x_opcode_nonincr(REG_TGR3D_PSEQ_INST_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link)
      PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(REG_TGR3D_AT_REMAP_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link)
      PUSH(grate_fp_pack_sched(&instr->mfu_sched));

   int num_mfu_instrs = list_length(&fp.mfu_instructions);
   if (num_mfu_instrs >= 64) {
      fprintf(stderr, "GRATE FRAG: %d MFU instructions exceeds the 63 that fit "
                      "in a schedule address\n", num_mfu_instrs);
      num_mfu_instrs = 63;
   }

   PUSH(host1x_opcode_nonincr(REG_TGR3D_AT_INST_DATA_LO, num_mfu_instrs * 2));
   list_for_each_entry(struct fp_mfu_instr, instr, &fp.mfu_instructions, link) {
      uint32_t words[2];
      grate_fp_pack_mfu(words, instr);
      PUSH(words[0]);
      PUSH(words[1]);
   }

   PUSH(host1x_opcode_nonincr(REG_TGR3D_TEX_INST_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link)
      PUSH(grate_fp_pack_tex(&instr->tex));

   PUSH(host1x_opcode_nonincr(REG_TGR3D_ALU_REMAP_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link) {
      if (soc_id == DRM_TEGRA_SOC_T114)
         PUSH(grate_fp_pack_alu_sched_t114(&instr->alu_sched));
      else
         PUSH(grate_fp_pack_sched(&instr->alu_sched));
   }

   int num_alu_instrs = list_length(&fp.alu_instructions);
   PUSH(host1x_opcode_nonincr(REG_TGR3D_ALU_INST_DATA, num_alu_instrs * 4 * 2));
   list_for_each_entry(struct fp_alu_instr_packet, instr, &fp.alu_instructions, link) {
      for (int i = 0; i < 4; ++i) {
         uint32_t words[2];
         /* the fourth slot carries embedded constants when the packet needs them */
         if (i == 3 && instr->has_constants)
            grate_fp_pack_alu_constants(words, instr->constants);
         else
            grate_fp_pack_alu(words, instr->slots + i);
         PUSH(words[0]);
         PUSH(words[1]);
      }
   }

   PUSH(host1x_opcode_nonincr(REG_TGR3D_ALU_P2CX_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link)
      PUSH(0x00000000);

   PUSH(host1x_opcode_nonincr(REG_TGR3D_DW_INST_DATA, num_fp_instrs));
   list_for_each_entry(struct fp_instr, instr, &fp.fp_instructions, link) {
      /* the shader names colour target n; the surface it lives in is offset by
       * however many surfaces come before the colour targets */
      struct fp_dw_instr dw = instr->dw;
      dw.index += context->framebuffer.rt_base;
      PUSH(grate_fp_pack_dw(&dw));
   }

   uint32_t tram_setup = 0;
   tram_setup |= TGR3D_GLOBAL_TRI_ATTR_NUM_TRIS(64 / fp.info.max_tram_row);
   tram_setup |= TGR3D_GLOBAL_TRI_ATTR_TRI_ROWS(fp.info.max_tram_row);

   PUSH(host1x_opcode_incr(REG_TGR3D_GLOBAL_TRI_ATTR, 1));
   PUSH(tram_setup);

#undef PUSH
   util_dynarray_trim(&buf);

   so->blob.num_commands = buf.size / sizeof(uint32_t);
   so->blob.commands = buf.data;
   so->info = fp.info;
   return so;
}

static void
grate_bind_fs_state(struct pipe_context *pcontext, void *so)
{
   grate_trace();
   grate_context(pcontext)->fshader = so;
}

static void
grate_delete_fs_state(struct pipe_context *pcontext, void *so)
{
   grate_trace();
   FREE(so);
}

void
grate_context_program_init(struct pipe_context *pcontext)
{
   grate_trace();
   pcontext->create_vs_state = grate_create_vs_state;
   pcontext->bind_vs_state = grate_bind_vs_state;
   pcontext->delete_vs_state = grate_delete_vs_state;

   pcontext->create_fs_state = grate_create_fs_state;
   pcontext->bind_fs_state = grate_bind_fs_state;
   pcontext->delete_fs_state = grate_delete_fs_state;
}
