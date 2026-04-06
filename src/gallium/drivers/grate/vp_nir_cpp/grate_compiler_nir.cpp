#include "grate_compiler_nir.h"

#include <list>
#include <iostream>
#include <memory>
#include "vp_ir.h"

void nir_main_cpp(struct pipe_context *pcontext, const struct pipe_shader_state *template1, struct grate_vp_shader *vp) {


    std::list<std::unique_ptr<gir_instruction>> instr;

    instr.push_front(std::make_unique<gir_alu_vector_instr>(VP_VEC_OP_ADD));
    instr.push_front(std::make_unique<gir_alu_scalar_instr>(VP_SCALAR_OP_RCP));

    for (const auto& i: instr) {
        std::cout << *i << std::endl;
    }

    printf("Hello World!\n");
}