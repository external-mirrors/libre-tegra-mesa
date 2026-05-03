#include "vp_ir.h"

#if 0
std::unique_ptr<gir_alu_vector_instr>
emit_vec_unop(enum vp_vector_op op, struct vp_dst_operand dst,
              struct vp_src_operand src)
{
	std::unique_ptr<gir_alu_vector_instr> ptr = std::make_unique<gir_alu_vector_instr>(42);
	return std::move(ptr); // explicit move
}

struct vp_vec_instr
emit_vec_binop(enum vp_vector_op op, struct vp_dst_operand dst,
              struct vp_src_operand src0, struct vp_src_operand src1)
{
   struct vp_vec_instr ret = {
      .op = op,
      .dst = dst,
      .src = { src0, src1, src_undef() }
   };
   return ret;
}
#endif


std::unique_ptr<gir_alu_vector_instr>
emit_vec_nullop(enum vp_vector_op op)
{
	std::unique_ptr<gir_alu_vector_instr> ptr = std::make_unique<gir_alu_vector_instr>(op, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, TRUE);
	return std::move(ptr); // explicit move
}

#define GEN_V_NULLOP(OP) \
struct vp_vec_instr \
emit_v ## OP (struct vp_dst_operand dst, struct vp_src_operand src0, \
          struct vp_src_operand src1) \
{ \
   return emit_vec_binop(VP_VEC_OP_ ## OP, dst, src0, src1); \
}

GEN_V_NULLOP(NOP)
GEN_V_NULLOP(SFL)
GEN_V_NULLOP(SFL)