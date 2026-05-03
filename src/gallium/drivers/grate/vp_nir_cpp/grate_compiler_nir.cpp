#include "grate_compiler_nir.h"

#include <list>
#include <iostream>
#include <memory>
#include "vp_ir.h"


static bool
grate_alu_to_scalar_filter_cb(const nir_instr *instr, const void *data)
{


   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);
   switch (alu->op) {
   case nir_op_frcp: // SOP_RCP
   case nir_op_frsq: // SOP_RSQ
   case nir_op_flog2: // SOP_LOG2
   case nir_op_fexp2: // SOP_EX2
   case nir_op_fcos: // SOP_SIN
   case nir_op_fsin: // SOP_COS

   /* What about us?
   SOP_RCC,
   SOP_EXP,
   SOP_LOG,
   SOP_LIT,
   */
      return true;
   /* TODO: can do better than alu_to_scalar for vector compares */
   case nir_op_b32all_fequal2:
   case nir_op_b32all_fequal3:
   case nir_op_b32all_fequal4:
   case nir_op_b32any_fnequal2:
   case nir_op_b32any_fnequal3:
   case nir_op_b32any_fnequal4:
   case nir_op_b32all_iequal2:
   case nir_op_b32all_iequal3:
   case nir_op_b32all_iequal4:
   case nir_op_b32any_inequal2:
   case nir_op_b32any_inequal3:
   case nir_op_b32any_inequal4:
      return true;

   default:
      break;
   }

   return false;
}

static int
type_size(const struct glsl_type *type, bool bindless)
{
   return glsl_count_attribute_slots(type, false);
}

void nir_main_cpp(struct pipe_context *pcontext, const struct pipe_shader_state *template1, struct grate_vp_shader *vp) {
    printf("Hello World from NIR C++ Vertex Shader Compiler!\n");

    std::list<std::unique_ptr<gir_instruction>> instr;


   //struct grate_compiler *c = CALLOC_STRUCT(grate_compiler);
   struct grate_compiler *c = rzalloc(NULL, struct grate_compiler);

   nir_shader *s = nir_shader_clone(NULL, template1->ir.nir);

   nir_print_shader(s, stderr);

   nir_lower_io(s, nir_var_shader_in | nir_var_shader_out,
             type_size, static_cast<nir_lower_io_options>(0));

   NIR_PASS(_, s, nir_opt_undef);
   NIR_PASS(_, s, nir_copy_prop);
   NIR_PASS(_, s, nir_opt_dce);



      int a;

   //NIR_PASS(_, s, nir_lower_io_vars_to_temporaries, nir_shader_get_entrypoint(nir), true, false);

   NIR_PASS(_, s, nir_opt_shrink_vectors, false);


   NIR_PASS(_, s, nir_lower_alu_to_scalar, grate_alu_to_scalar_filter_cb, 0);

   nir_opt_peephole_select_options peephole_select_options = {
      .limit = 16,
      .indirect_load_ok = true,
      .expensive_alu_ok = true,
   };
   NIR_PASS(_, s, nir_opt_peephole_select, &peephole_select_options);
NIR_PASS(_, s, nir_lower_bool_to_float, true);

   unsigned  move_all =
       nir_move_const_undef | nir_move_load_ubo | nir_move_load_input | nir_move_load_frag_coord |
       nir_move_comparisons | nir_move_copies | nir_move_load_ssbo | nir_move_alu;

   NIR_PASS(_, s, nir_opt_move, (nir_move_options)move_all);

nir_lower_ubo_vec4(s);
NIR_PASS(_, s, nir_move_vec_src_uses_to_dest, false);
   //nir_lower_vec3_to_vec4(); // maybe, maybe not.

   printf("--------\n");
   nir_print_shader(s, stdout);

   nir_convert_from_ssa(s, true, false);
   nir_trivialize_registers(s);
   printf("++++++++++\n");
   nir_print_shader(s, stdout);

   nir_function_impl *entry = nir_shader_get_entrypoint(s);



   list_inithead(&vp->instructions);

#if 0
   emit_function(vp, entry);


   grate_dump_ir_virt(vp);

   grate_dump_ir_hw(vp);

   c->regs = grate_ra_setup(c);
   if (!c->regs) {
      ralloc_free((void *)c);
      c = NULL;
   }


   grate_ra_assign(c, vp);

    instr.push_front(std::make_unique<gir_alu_vector_instr>(VP_VEC_OP_ADD));
    instr.push_front(std::make_unique<gir_alu_scalar_instr>(VP_SCALAR_OP_RCP));

    for (const auto& i: instr) {
        std::cout << *i << std::endl;
    }


#endif
    list_inithead(&vp->instructions);
}