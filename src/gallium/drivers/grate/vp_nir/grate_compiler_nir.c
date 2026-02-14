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

   int idx = nir_intrinsic_base(intr);
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

   enum vp_swz swizzle[4] = {
      0,0,0,0
      /*src->SwizzleX,
      src->SwizzleY,
      src->SwizzleZ,
      src->SwizzleW */
   };

   printf("emit_store_output: idx: %d\n", idx);

   return emit_packed(emit_vMOV(emit_output(vp, idx, 0, false),
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
   printf("%s\n", __FUNCTION__);
   struct vp_instr *vp_instr = NULL;

   const nir_op_info *info = &nir_op_infos[alu->op];

   struct nir_instr *instr = (struct nir_instr *)alu;
   nir_print_instr(instr, stdout);
   printf("\n");

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

   switch (alu->op) {
   case nir_op_mov:
      break;

   case nir_op_fadd:
      vp_instr = emit_packed(emit_vADD(dst_temp(dst_index, dst_reg_class),
                                      src_temp(src_index[0], src_swizzle[0]),
                                      src_temp(src_index[1], src_swizzle[1])),
                              emit_sNOP());
      break;

   case nir_op_fmul:
      //nir_print_instr(alu, stderr);
      break;

   case nir_op_iadd:
      break;

   default:
      printf("emit_alu: not implemented (%s)\n", info->name);
      break;
   }

   if (vp_instr != NULL)
      list_addtail(&vp_instr->link, &vp->instructions);

   printf("-------------------------------------------\n");
}



static void
emit_block(struct grate_vp_shader *vp, struct nir_block *block)
{
   printf("%s\n", __FUNCTION__);
   nir_foreach_instr(instr, block) {
      switch (instr->type) {
      case nir_instr_type_alu:
         emit_alu(vp, nir_instr_as_alu(instr));
         break;
      #if 0
      case nir_instr_type_deref:
         UNREACHABLE("nir_instr_type_ssa_undef not supported");
         break;
      case nir_instr_type_call:
         UNREACHABLE("nir_instr_type_call not supported");
         break;
      case nir_instr_type_tex:
         UNREACHABLE("nir_instr_type_tex not supported");
         break;
      #endif
      case nir_instr_type_intrinsic:
         emit_intrinsic(vp, nir_instr_as_intrinsic(instr));
         break;
      #if 0
      case nir_instr_type_load_const:
         //emit_load_const(vp, nir_instr_as_load_const(instr));
         break;
      case nir_instr_type_jump:
         UNREACHABLE("nir_instr_type_jump not supported");
         break;
      case nir_instr_type_undef:
         UNREACHABLE("nir_instr_type_ssa_undef not supported");
         break;
      case nir_instr_type_phi:
         UNREACHABLE("nir_instr_type_phi not supported");
         break;
      case nir_instr_type_parallel_copy:
         UNREACHABLE("nir_instr_type_parallel_copy not supported");
         break;
      }
         #endif
      default:
         break;
      }
   }
}



static void
emit_cf_list(struct grate_vp_shader *vp, struct exec_list *list)
{
   printf("%s\n", __FUNCTION__);
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
   printf("%s\n", __FUNCTION__);

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


   grate_dump_ir(vp);

   c->regs = grate_ra_setup(c);
   if (!c->regs) {
      ralloc_free((void *)c);
      c = NULL;
   }

}
