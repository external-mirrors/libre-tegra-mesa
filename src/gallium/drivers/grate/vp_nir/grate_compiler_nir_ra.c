#include "grate_compiler_nir.h"

#include "util/ralloc.h"





#define NUM_VEC4_REGS 32
#define LANES_PER_REG 4
#define NUM_LANES (NUM_VEC4_REGS * LANES_PER_REG)





/* Swizzles and write masks can be used to layer virtual non-interfering
 * registers on top of the real VEC4 registers. For example, the virtual
 * VEC3_XYZ register and the virtual SCALAR_W register that use the same
 * physical VEC4 base register do not interfere.
 */
enum reg_class {
   REG_CLASS_VIRT_SCALAR,
   REG_CLASS_VIRT_VEC2,
   REG_CLASS_VIRT_VEC3,
   REG_CLASS_VEC4,
   NUM_REG_CLASSES,
};

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

/* writemask when used as dest */
static const uint8_t
reg_writemask[NUM_REG_TYPES] = {
   [REG_TYPE_VEC4] = 0xf,
   [REG_TYPE_VIRT_SCALAR_X] = 0x1,
   [REG_TYPE_VIRT_SCALAR_Y] = 0x2,
   [REG_TYPE_VIRT_VEC2_XY] = 0x3,
   [REG_TYPE_VIRT_SCALAR_Z] = 0x4,
   [REG_TYPE_VIRT_VEC2_XZ] = 0x5,
   [REG_TYPE_VIRT_VEC2_YZ] = 0x6,
   [REG_TYPE_VIRT_VEC3_XYZ] = 0x7,
   [REG_TYPE_VIRT_SCALAR_W] = 0x8,
   [REG_TYPE_VIRT_VEC2_XW] = 0x9,
   [REG_TYPE_VIRT_VEC2_YW] = 0xa,
   [REG_TYPE_VIRT_VEC3_XYW] = 0xb,
   [REG_TYPE_VIRT_VEC2_ZW] = 0xc,
   [REG_TYPE_VIRT_VEC3_XZW] = 0xd,
   [REG_TYPE_VIRT_VEC3_YZW] = 0xe,
};

static inline int reg_get_type(int virt_reg)
{
   return virt_reg % NUM_REG_TYPES;
}


static inline int reg_get_class(int virt_reg)
{
   switch (reg_get_type(virt_reg)) {
   case REG_TYPE_VEC4:
      return REG_CLASS_VEC4;
   case REG_TYPE_VIRT_VEC3_XYZ:
   case REG_TYPE_VIRT_VEC3_XYW:
   case REG_TYPE_VIRT_VEC3_XZW:
   case REG_TYPE_VIRT_VEC3_YZW:
      return REG_CLASS_VIRT_VEC3;
   case REG_TYPE_VIRT_VEC2_XY:
   case REG_TYPE_VIRT_VEC2_XZ:
   case REG_TYPE_VIRT_VEC2_XW:
   case REG_TYPE_VIRT_VEC2_YZ:
   case REG_TYPE_VIRT_VEC2_YW:
   case REG_TYPE_VIRT_VEC2_ZW:
      return REG_CLASS_VIRT_VEC2;
   case REG_TYPE_VIRT_SCALAR_X:
   case REG_TYPE_VIRT_SCALAR_Y:
   case REG_TYPE_VIRT_SCALAR_Z:
   case REG_TYPE_VIRT_SCALAR_W:
      return REG_CLASS_VIRT_SCALAR;
   }

   assert(false);
   return 0;
}


/* precomputed by register_allocate  */
/* optimization for later because people told me stop premature optimization :) */
static unsigned int *q_values[] = {
   (unsigned int[]) {1, 2, 3, 4, 2, 2, 3, },
   (unsigned int[]) {3, 5, 6, 6, 5, 5, 6, },
   (unsigned int[]) {3, 4, 4, 4, 4, 4, 4, },
   (unsigned int[]) {1, 1, 1, 1, 1, 1, 1, },
   (unsigned int[]) {1, 2, 2, 2, 1, 2, 2, },
   (unsigned int[]) {2, 3, 3, 3, 2, 3, 3, },
   (unsigned int[]) {2, 2, 2, 2, 2, 2, 2, },
};

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