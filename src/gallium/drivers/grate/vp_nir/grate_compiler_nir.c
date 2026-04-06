#include "grate_compiler_nir.h"

#include "vp_ir.h"




#if 0

static void
emit_load_input(struct tegra_fp_shader *fp, nir_intrinsic_instr *intr)
{
   /* we can't dynamically index varyings */
   nir_const_value *const_offset = nir_src_as_const_value(intr->src[0]);
   assert(const_offset != NULL);

   assert(1 == intr->num_components); // should be scalar by now

   uint32_t offset = nir_intrinsic_base(intr) + const_offset->u32[0];

   struct fpir_node *node = create_fpir_node(fp, FPIR_VAR_NODE);
   node->var.index = offset * 4 + nir_intrinsic_component(intr);

   printf("load comp: %d\n", node->var.index);

   list_addtail(&node->link, &fp->nodes);

   assert(intr->dest.is_ssa);
   init_ssa_def(fp, &intr->dest.ssa, node);
}

static void
emit_load_const(struct tegra_fp_shader *fp, nir_load_const_instr *instr)
{
   printf("emit_load_const: not implemented\n");
   assert(instr->def.num_components == 1);
}
#endif

static struct vp_instr *
emit_store_output(struct grate_vp_shader *vp, nir_intrinsic_instr *intr)
{
   assert(nir_intrinsic_infos[intr->intrinsic].num_srcs == 2);

   const nir_intrinsic_info *info = &nir_intrinsic_infos[intr->intrinsic];

   int idx = nir_intrinsic_base(intr);
   int write_mask = nir_intrinsic_write_mask(intr);

   //int comp = nir_intrinsic_component(intr); // later my friend

   //assert(!intr->src[0].is_ssa);

   // no support for dynamic indexing of outputs
   nir_const_value *const_offset = nir_src_as_const_value(intr->src[1]);
   assert(const_offset != NULL);
   idx += const_offset->u32;

/*
   src = get_src(ctx, &intr->src[0]);
   for (int i = 0; i < intr->num_components; i++) {
      unsigned n = idx * 4 + i + comp;
      ctx->ir->outputs[n] = src[i];
   }
*/


// Check if src is powered by alu_instr
   if (intr->src[0].ssa->parent_instr->type == nir_instr_type_alu) {
      struct nir_alu_instr *parent_fsat = nir_instr_as_alu(intr->src[0].ssa->parent_instr);
      // Check if src is fsat
      if (parent_fsat->op == nir_op_fsat) {
         // if src of fsat is ALU instr
         if (parent_fsat->src->src.ssa->parent_instr->type == nir_instr_type_alu) {
            struct nir_alu_instr *parent_fsat_generator = nir_instr_as_alu(parent_fsat->src->src.ssa->parent_instr);
            bool use_as_src = nir_def_all_uses_are_fsat(&parent_fsat_generator->def);
            // If src of fsat is only used for fsat
            if (use_as_src) {
               int index = parent_fsat_generator->def.index;
               printf("Should use %d\n", index);
            }
         }
      }
   }


//@store_output (%14, %16 (0x0)) (base=0, range=1, wrmask=xyzw, component=0, src_type=float32, io location=VARYING_SLOT_POS slots=1, xfb(), xfb2())  // VARYING_SLOT_POS
//@store_output (%22, %24 (0x0)) (base=1, range=1, wrmask=w, component=0, src_type=float32, io location=VARYING_SLOT_COL0 slots=1, xfb(), xfb2())  // VARYING_SLOT_COL0
//@store_output (%38, %39 (0x0)) (base=1, range=1, wrmask=xyz, component=0, src_type=float32, io location=VARYING_SLOT_COL0 slots=1, xfb(), xfb2())  // VARYING_SLOT_COL0

   // Gather Output Data
   int dst_index = intr->def.index;
   enum reg_class dst_reg_class = intr->def.num_components - 1;

   // Gather Input Data
   int src_index[3];
   enum vp_swz src_swizzle[3][4];
   for (int i = 0; i < info->num_srcs; i++) {
      src_index[i] = intr->src[i].ssa->index;
/*
      src_swizzle[i][0] = intr->src[i].swizzle[0];
      src_swizzle[i][1] = intr->src[i].swizzle[1];
      src_swizzle[i][2] = intr->src[i].swizzle[2];
      src_swizzle[i][3] = intr->src[i].swizzle[3];
*/
   }

   enum vp_swz swizzle[4] = {
      0,0,0,0
      /*src->SwizzleX,
      src->SwizzleY,
      src->SwizzleZ,
      src->SwizzleW */
   };


   return emit_packed(emit_vMOV(emit_output(vp, idx, write_mask, false),
                        src_temp(idx, swizzle)),
                         emit_sNOP());
}

static void
emit_intrinsic(struct grate_vp_shader *vp, nir_intrinsic_instr *intr)
{
   struct vp_instr * vp_instr = NULL;
   switch (intr->intrinsic) {
   /*
      case nir_intrinsic_load_input:
      emit_load_input(fp, intr);
      break;
   */
   case nir_intrinsic_store_output:
      vp_instr = emit_store_output(vp, intr);
      break;

   default:
      printf("emit_intrinsic: not implemented (%s)\n",
             nir_intrinsic_infos[intr->intrinsic].name);
   }

   if (vp_instr != NULL)
      list_addtail(&vp_instr->link, &vp->instructions);

}





static void
emit_alu(struct grate_vp_shader *vp, nir_alu_instr *alu)
{
   //printf("%s\n", __FUNCTION__);
   struct vp_instr *vp_instr = NULL;

   const nir_op_info *info = &nir_op_infos[alu->op];

   struct nir_instr *instr = (struct nir_instr *)alu;

   // Gather Output Data
   int dst_index = alu->def.index;
   enum reg_class dst_reg_class = alu->def.num_components - 1;

   // Gather Input Data
   int src_index[3];
   enum vp_swz src_swizzle[3][4];
   for (int i = 0; i < info->num_inputs; i++) {
      src_index[i] = alu->src[i].src.ssa->index;

      src_swizzle[i][0] = alu->src[i].swizzle[0];
      src_swizzle[i][1] = alu->src[i].swizzle[1];
      src_swizzle[i][2] = alu->src[i].swizzle[2];
      src_swizzle[i][3] = alu->src[i].swizzle[3];
   }

   /*
	* It works that way. Trust me. Or proof me wrong
	NIR mov > fsat > mov

	becomes
	MOV_Sat > fsat > mov
	^ saturate if result is always used by fsat (simple block1)

	MOV_Sat > fsat > mov
			^ ignore

	MOV_Sat > fsat > mov
					^ check if src is fsat, check if src of fsat is only used by fsat, if yes optimize (crazy block2)

	MOV_Sat > mov
   */

   // Saturate output if possible
   bool sat = nir_def_all_uses_are_fsat(&alu->def);

   // Check if src is powered by alu_instr
   if (alu->src[0].src.ssa->parent_instr->type == nir_instr_type_alu) {
      struct nir_alu_instr *parent_fsat = nir_instr_as_alu(alu->src[0].src.ssa->parent_instr);
      // Check if src is fsat
      if (parent_fsat->op == nir_op_fsat) {
         // if src of fsat is ALU instr
         if (parent_fsat->src->src.ssa->parent_instr->type == nir_instr_type_alu) {
            struct nir_alu_instr *parent_fsat_generator = nir_instr_as_alu(parent_fsat->src->src.ssa->parent_instr);
            bool use_as_src = nir_def_all_uses_are_fsat(&parent_fsat_generator->def);
            // If src of fsat is only used for fsat
            if (use_as_src) {
               int index = parent_fsat_generator->def.index;
               printf("Should use %d\n", index);
            }
         }
      }
   }


#if 0
   // Handle ABS and NEG modifiers
   i: Loop over all source:
      bool abs/neg = false
      parent  = nir_instr.parent
      while parent.op == fabs|fneg
         if parent.op == fabs:
            abs = !abs
            parent = parent.parent
            continue;
         elif parent.op == fneg:
            neg = !neg
            parent = parent.parent
            continue;
         else
            break
      grate_instr.src[i] = parent
      grate_instr.abs = abs
      grate_instr.neg = neg
#endif

   switch (alu->op) {
   case nir_op_mov:
      break;

   case nir_op_fadd:
      vp_instr = emit_packed(emit_vADD(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;

   case nir_op_fmul:
      vp_instr = emit_packed(emit_vMUL(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;
   case nir_op_fdot3:
      vp_instr = emit_packed(emit_vDP3(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;

   case nir_op_fmax:
      vp_instr = emit_packed(emit_vMAX(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;

   case nir_op_slt:
      vp_instr = emit_packed(emit_vSLT(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;

   case nir_op_frsq:
      vp_instr = emit_packed(emit_vNOP(),
                              emit_sRSQ(dst_temp(dst_index, dst_reg_class, sat),
                                      src_temp(src_index[0], src_swizzle[0])));
      break;

   case nir_op_fsat:
      if (!sat) {
         vp_instr = emit_packed(emit_vMOV(dst_temp(dst_index, dst_reg_class, true),
                        src_temp(src_index[0], src_swizzle[0])),
                         emit_sNOP());
      }
      break;

   default:
      printf("emit_alu: not implemented (%s): ", info->name);
      nir_print_instr(instr, stdout);
      printf("\n");
      break;
   }

   if (vp_instr != NULL)
      list_addtail(&vp_instr->link, &vp->instructions);
}



static void
emit_block(struct grate_vp_shader *vp, struct nir_block *block)
{
   //printf("%s\n", __FUNCTION__);
   nir_foreach_instr(instr, block) {
      switch (instr->type) {
      case nir_instr_type_alu:
         emit_alu(vp, nir_instr_as_alu(instr));
         break;

      case nir_instr_type_deref:
         printf("emit_block: (deref) not implemented \n");
         //UNREACHABLE("nir_instr_type_ssa_undef not supported");
         break;
      case nir_instr_type_call:
         printf("emit_block: (call) not implemented \n");
         //UNREACHABLE("nir_instr_type_call not supported");
         break;
      case nir_instr_type_tex:
         printf("emit_block: (tex) not implemented \n");
         //UNREACHABLE("nir_instr_type_tex not supported");
         break;
      case nir_instr_type_intrinsic:
         emit_intrinsic(vp, nir_instr_as_intrinsic(instr));
         break;

      case nir_instr_type_load_const:
         printf("emit_block: (load_const) not implemented \n");
         //emit_load_const(vp, nir_instr_as_load_const(instr));
         break;
      case nir_instr_type_jump:
         printf("emit_block: (jump) not implemented \n");
         //UNREACHABLE("nir_instr_type_jump not supported");
         break;
      case nir_instr_type_undef:
         printf("emit_block: (undef) not implemented \n");
         //UNREACHABLE("nir_instr_type_ssa_undef not supported");
         break;
      case nir_instr_type_phi:
         printf("emit_block: (phi) not implemented \n");
         //UNREACHABLE("nir_instr_type_phi not supported");
         break;
      case nir_instr_type_parallel_copy:
         printf("emit_block: (parallel_copy) not implemented \n");
         //UNREACHABLE("nir_instr_type_parallel_copy not supported");
         break;
      default:
         printf("emit_block: not implemented \n");
         break;
      }
   }
}



static void
emit_cf_list(struct grate_vp_shader *vp, struct exec_list *list)
{
   //printf("%s\n", __FUNCTION__);
   foreach_list_typed(nir_cf_node, node, node, list) {
      switch (node->type) {
      case nir_cf_node_block:
         emit_block(vp, nir_cf_node_as_block(node));
         break;
      case nir_cf_node_if:
         UNREACHABLE("nir_cf_node_if not supported");
         break;
      case nir_cf_node_loop:
         UNREACHABLE("nir_cf_node_loop not supported");
         break;
      case nir_cf_node_function:
         UNREACHABLE("nir_cf_node_function not supported");
         break;
      }
   }
}


static void
emit_function(struct grate_vp_shader *vp , nir_function_impl *impl) {
   //printf("%s\n", __FUNCTION__);

   nir_print_function_body(impl, stderr);

   emit_cf_list(vp, &impl->body);
}



static int
type_size(const struct glsl_type *type, bool bindless)
{
   return glsl_count_attribute_slots(type, false);
}

void nir_main(struct pipe_context *pcontext, const struct pipe_shader_state *template, struct grate_vp_shader *vp) {
   //struct grate_compiler *c = CALLOC_STRUCT(grate_compiler);
   struct grate_compiler *c = rzalloc(NULL, struct grate_compiler);

   nir_shader *nir = nir_shader_clone(NULL, template->ir.nir);

   nir_print_shader(nir, stderr);

   nir_lower_io(nir, nir_var_shader_in | nir_var_shader_out,
             type_size, 0);


   //NIR_PASS(_, nir, nir_lower_io_vars_to_temporaries, nir_shader_get_entrypoint(nir), true, false);

   nir_lower_ubo_vec4(nir);
   //nir_lower_vec3_to_vec4(); // maybe, maybe not.

   printf("--------\n");
   nir_print_shader(nir, stdout);

   nir_convert_from_ssa(nir, true, false);
   nir_trivialize_registers(nir);
   printf("++++++++++\n");
   nir_print_shader(nir, stdout);

   nir_function_impl *entry = nir_shader_get_entrypoint(nir);



   list_inithead(&vp->instructions);


   emit_function(vp, entry);


   grate_dump_ir_virt(vp);

   grate_dump_ir_hw(vp);

   c->regs = grate_ra_setup(c);
   if (!c->regs) {
      ralloc_free((void *)c);
      c = NULL;
   }


   grate_ra_assign(c, vp);
}
