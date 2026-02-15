#include "vp_ir.h"

#include "util/macros.h"

static const char *v_ops[] = {
	[VP_VEC_OP_NOP] = "vNOP",
	[VP_VEC_OP_MOV] = "vMOV",
	[VP_VEC_OP_MUL] = "vMUL",
	[VP_VEC_OP_ADD] = "vADD",
	[VP_VEC_OP_MAD] = "vMAD",
	[VP_VEC_OP_DP3] = "vDP3",
	[VP_VEC_OP_DPH] = "vDPH",
	[VP_VEC_OP_DP4] = "vDP4",
	[VP_VEC_OP_DST] = "vDST",
	[VP_VEC_OP_MIN] = "vMIN",
	[VP_VEC_OP_MAX] = "vMAX",
	[VP_VEC_OP_SLT] = "vSLT",
	[VP_VEC_OP_SGE] = "vSGW",
	[VP_VEC_OP_ARL] = "vARL",
	[VP_VEC_OP_FRC] = "vFRC",
	[VP_VEC_OP_FLR] = "vFLR",
	[VP_VEC_OP_SEQ] = "vSEQ",
	[VP_VEC_OP_SFL] = "vSFL",
	[VP_VEC_OP_SGT] = "vSGt",
	[VP_VEC_OP_SLE] = "vSLE",
	[VP_VEC_OP_SNE] = "vSNE",
	[VP_VEC_OP_STR] = "vSTR",
	[VP_VEC_OP_SSG] = "vSSG",
	[VP_VEC_OP_ARR] = "vARR",
	[VP_VEC_OP_ARA] = "vARA",
	[VP_VEC_OP_TXL] = "vTXL",
	[VP_VEC_OP_PUSHA] = "vPUSHA",
	[VP_VEC_OP_POPA] = "vPOPA"
};

static const char *s_ops[] = {
   [VP_SCALAR_OP_NOP] = "sNOP",
   [VP_SCALAR_OP_MOV] = "sMOV",
   [VP_SCALAR_OP_RCP] = "sRCP",
   [VP_SCALAR_OP_RCC] = "sRCC",
   [VP_SCALAR_OP_RSQ] = "sRSQ",
   [VP_SCALAR_OP_EXP] = "sEXP",
   [VP_SCALAR_OP_LOG] = "sLOG",
   [VP_SCALAR_OP_LIT] = "sLIT",
   [VP_SCALAR_OP_BRA] = "sBRA",
   [VP_SCALAR_OP_CAL] = "sCAL",
   [VP_SCALAR_OP_RET] = "sRET",
   [VP_SCALAR_OP_LG2] = "sLG2",
   [VP_SCALAR_OP_EX2] = "sEX2",
   [VP_SCALAR_OP_SIN] = "sSIN",
   [VP_SCALAR_OP_COS] = "sCOS",
   [VP_SCALAR_OP_PUSHA] = "sPUSHA",
   [VP_SCALAR_OP_POPA] = "sPOPA"
};

static const char vp_swz[] = {
   [VP_SWZ_X] = 'x',
   [VP_SWZ_Y] = 'y',
   [VP_SWZ_Z] = 'z',
   [VP_SWZ_W] = 'w'
};

enum print_mode {
	VIRT,
	HW
};

static char *print_dest(char* buf, struct vp_dst_operand *dst, enum print_mode mode) {
	int n = sprintf(buf, "%s%d.%s",

		(dst->file == VP_DST_FILE_OUTPUT) ? "o" : (dst->file == VP_DST_FILE_TEMP) ? ((mode == HW) ? "r" : "%") : "?",

		(mode == HW) ? dst->hw_id : dst->virt_id,
		(dst->reg_class == REG_CLASS_VIRT_SCALAR) ? "vec1" :
		(dst->reg_class == REG_CLASS_VIRT_VEC2) ? "vec2" :
		(dst->reg_class == REG_CLASS_VIRT_VEC3) ? "vec3" :
		(dst->reg_class == REG_CLASS_VEC4) ? "vec4" : "?"
	);

	return buf + n;
}

static char *print_src(char* buf, struct vp_src_operand *src, enum print_mode mode) {
	int n = sprintf(buf, "%s%s%s%d.%c%c%c%c%s",
		src->absolute ? "|" : "",
		src->negate ? "-" : "",
		(mode == HW) ? "r" : "%",
		(mode == HW) ? src->hw_id : src->virt_id,
		vp_swz[src->swizzle[0]],
		vp_swz[src->swizzle[1]],
		vp_swz[src->swizzle[2]],
		vp_swz[src->swizzle[3]],
		src->absolute ? "|" : ""
	);

	return buf + n;
}

// let it use VArgs
static char *print(char* buf, const char *str) {
	int n = sprintf(buf, "%s", str);

	return buf + n;
}

static char *print_v_op(char* tmp, struct vp_vec_instr *vec, enum print_mode mode) {

	tmp = print_dest(tmp, &vec->dst, mode);
	tmp = print(tmp, " = ");

	int n = sprintf(tmp, "%s%s ",
		v_ops[vec->op],
		vec->dst.saturate ? "_SAT" : ""
	);
	tmp += n;

	unsigned int x;
	switch(vec->op) {
		case VP_VEC_OP_NOP:
		case VP_VEC_OP_STR:
			x= 0;
			break;
		case VP_VEC_OP_MOV:
		case VP_VEC_OP_ARL:
		case VP_VEC_OP_ARR:
		case VP_VEC_OP_ARA:
		case VP_VEC_OP_FRC:
		case VP_VEC_OP_FLR:
		case VP_VEC_OP_SSG:
			x = 1;
			break;
		case VP_VEC_OP_MUL:
		case VP_VEC_OP_DP3:
		case VP_VEC_OP_DPH:
		case VP_VEC_OP_DP4:
		case VP_VEC_OP_DST:
		case VP_VEC_OP_MIN:
		case VP_VEC_OP_MAX:
		case VP_VEC_OP_SLT:
		case VP_VEC_OP_SGE:
		case VP_VEC_OP_SEQ:
		case VP_VEC_OP_SFL:
		case VP_VEC_OP_SGT:
		case VP_VEC_OP_SLE:
		case VP_VEC_OP_SNE:
		case VP_VEC_OP_TXL:
			x = 2;
			break;
		case VP_VEC_OP_ADD: // exception because of how add instruction is put to ALU
		case VP_VEC_OP_MAD:
		// No idea about stack stuff
		case VP_VEC_OP_PUSHA:
		case VP_VEC_OP_POPA:
			x = 3;
			break;
	}

	for (int i = 0; i < x; i++) {
		if (vec->op == VP_VEC_OP_ADD && i == 1)
			continue;

		tmp = print_src(tmp, &vec->src[i], mode);
		tmp = print(tmp, ", ");
	}


	return tmp;
}



static void
grate_dump_ir(struct grate_vp_shader *vp, bool virt, bool hw) {
	int num_instructions = list_length(&vp->instructions);
	printf("Dumping Grate-IR (%d) instructions:\n", num_instructions);

	char str[256];
	char *tmp = str;

	assert(vp != NULL);
	assert(vp->instructions.next != NULL);

	list_for_each_entry(struct vp_instr, instr, &vp->instructions, link) {
		// Loop init
		memset(str, 0, 256);
		tmp = str;

		// Generate Vec Op
		if (hw) {
			tmp = print_v_op(tmp, &instr->vec, HW);
			tmp = print(tmp, "\n");
			printf("┏ %s", str);
		}
		if (virt) {
			tmp = print_v_op(tmp, &instr->vec, VIRT);
			tmp = print(tmp, "\n");
			printf("┏ %s", str);
		}

		// Generate Scalar Op
		printf("┗ %s\n", s_ops[instr->scalar.op]);
	}
}

void
grate_dump_ir_virt(struct grate_vp_shader *vp) {
	grate_dump_ir(vp, true, false);
}

void
grate_dump_ir_hw(struct grate_vp_shader *vp) {
	grate_dump_ir(vp, false, true);
}

void
grate_dump_ir_all(struct grate_vp_shader *vp) {
	grate_dump_ir(vp, true, true);
}