#ifndef MYNIRCPP_H
#define MYNIRCPP_H

//#include "nir.h"
#include <pipe/p_state.h>
#include "nir.h"
#include "grate_compiler.h"

#include "util/register_allocate.h"

#ifdef __cplusplus
extern "C" {
#endif

struct grate_compiler {
        struct ra_regs *regs; // allocated by ralloc
};


struct ra_regs *grate_ra_setup(void *mem_ctx);

void grate_ra_assign(struct grate_compiler *c, struct grate_vp_shader *vp);

void nir_main_cpp(struct pipe_context *pcontext, const struct pipe_shader_state *template1, struct grate_vp_shader *vp);

#ifdef __cplusplus
}
#endif

#endif
