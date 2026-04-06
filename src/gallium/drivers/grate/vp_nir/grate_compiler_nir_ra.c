#include "grate_compiler_nir.h"
#include "vp_ir.h"

#include "util/ralloc.h"


#define NUM_VEC4_REGS 32
#define LANES_PER_REG 4
#define NUM_LANES (NUM_VEC4_REGS * LANES_PER_REG)

/*
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
*/

#if 0
struct ra_regs *
grate_ra_setup(void *mem_ctx)
{
   struct ra_regs *regs = ra_alloc_reg_set(mem_ctx, NUM_VEC4_REGS *
                  NUM_REG_TYPES, false);

   /* classes always be created from index 0, so equal to the class enum
    * which represents a register with (c+1) components
    */
   struct ra_class *classes[NUM_REG_CLASSES];
   for (int c = 0; c < NUM_REG_CLASSES; c++)
      classes[c] = ra_alloc_reg_class(regs);
   /* add each register of each class */
   for (int r = 0; r < NUM_REG_TYPES * NUM_VEC4_REGS; r++)
      ra_class_add_reg(classes[reg_get_class(r)], r);
   /* set conflicts */
   for (int r = 0; r < NUM_VEC4_REGS; r++) {
      for (int i = 0; i < NUM_REG_TYPES; i++) {
         for (int j = 0; j < i; j++) {
            if (reg_writemask[i] & reg_writemask[j]) {
               ra_add_reg_conflict(regs, NUM_REG_TYPES * r + i,
                                         NUM_REG_TYPES * r + j);
            }
         }
      }
   }
   ra_set_finalize(regs, NULL);
   printf("For what ever reason register allocator does not fail :)\n");

   return regs;
}
#endif