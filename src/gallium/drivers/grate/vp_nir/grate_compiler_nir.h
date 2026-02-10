#ifndef MYNIR_H
#define MYNIR_H

//#include "nir.h"
#include <pipe/p_state.h>
#include "nir.h"
#include "grate_compiler.h"

#include "util/register_allocate.h"


struct grate_compiler {
        struct ra_regs *regs; // allocated by ralloc
};


struct ra_regs *grate_ra_setup(void *mem_ctx);

void nir_main(struct pipe_context *pcontext, const struct pipe_shader_state *template, struct grate_vp_shader *vp);

#endif
