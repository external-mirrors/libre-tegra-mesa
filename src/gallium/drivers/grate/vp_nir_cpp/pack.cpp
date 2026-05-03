

enum swizzle {
        X,
        Y,
        Z,
        W
};



struct addr_reg {
        unsigned id;
        enum swizzle swz;
};

enum dst_type {
        OREG,
        AREG,
        OUT, // Export
};

struct dst {
        enum dst_type type;
        unsigned char wr_msk;
        bool sat;
        unsigned id;
        bool rel_addr; // for OBUF
        bool write_cc;
};

enum src_type {
        INVAL = 0,
        IREG = 1, // Register
        BUF = 2, // Attribute
        CTX = 3 // Constant/Uniform
};

struct src {
        enum src_type type;
        bool abs, neg;
        enum swizzle swz[4];

        unsigned id;
        bool rel_addr; // for IBUF and CTX
};

enum instr_type {
        VECTOR,
        SCALAR,
        INDEPENDENT // MOV, NOP, PUSH/POP; should allow optimization for scheduler. But might not fit in here? 🥴
};

struct instr {
        enum instr_type type;
        struct src src[3];
        struct dst dst;

        bool cc_write;
};

enum vp_vector_op {
   VP_VEC_OP_NOP = 0,
   VP_VEC_OP_MOV = 1,
   VP_VEC_OP_MUL = 2,
   VP_VEC_OP_ADD = 3,
   VP_VEC_OP_MAD = 4,
   VP_VEC_OP_DP3 = 5,
   VP_VEC_OP_DPH = 6,
   VP_VEC_OP_DP4 = 7,
   VP_VEC_OP_DST = 8,
   VP_VEC_OP_MIN = 9,
   VP_VEC_OP_MAX = 10,
   VP_VEC_OP_SLT = 11,
   VP_VEC_OP_SGE = 12,
   VP_VEC_OP_ARL = 13,
   VP_VEC_OP_FRC = 14,
   VP_VEC_OP_FLR = 15,
   VP_VEC_OP_SEQ = 16,
   VP_VEC_OP_SFL = 17,
   VP_VEC_OP_SGT = 18,
   VP_VEC_OP_SLE = 19,
   VP_VEC_OP_SNE = 20,
   VP_VEC_OP_STR = 21,
   VP_VEC_OP_SSG = 22,
   VP_VEC_OP_ARR = 23,
   VP_VEC_OP_MVA = 24,
   VP_VEC_OP_TXL = 25,
   VP_VEC_OP_PUSHA = 26,
   VP_VEC_OP_POPA = 27
};

struct vector_instr {
        struct instr instr;
        enum vp_vector_op op;

        // ARL/ARR op code decide that dest is AddrReg.
};

enum vp_scalar_op {
    VP_SCALAR_OP_NOP = 0,
    VP_SCALAR_OP_MOV = 1,
    VP_SCALAR_OP_RCP = 2,
    VP_SCALAR_OP_RCC = 3,
    VP_SCALAR_OP_RSQ = 4,
    VP_SCALAR_OP_EXP = 5,
    VP_SCALAR_OP_LOG = 6,
    VP_SCALAR_OP_LIT = 7,
    VP_SCALAR_OP_BRA = 8,
    VP_SCALAR_OP_BRI = 9,
    VP_SCALAR_OP_CLA = 10,
    VP_SCALAR_OP_CLI = 11,
    VP_SCALAR_OP_RET = 12,
    VP_SCALAR_OP_LG2 = 13,
    VP_SCALAR_OP_EX2 = 14,
    VP_SCALAR_OP_SIN = 15,
    VP_SCALAR_OP_COS = 16,
    VP_SCALAR_OP_BRB = 17,
    VP_SCALAR_OP_CLB = 18,
    VP_SCALAR_OP_PUSHA = 19,
    VP_SCALAR_OP_POPA = 20
};

struct scalar_instr {
        struct instr instr;
        enum vp_scalar_op op;
};

enum cc_func {
       FALSE = 0,
       LT = 1,
       EQ = 2,
       LE = 3,
       GT = 4,
       NE = 5,
       GE = 6,
       TRUE = 7,
};



enum vliw_resources {
    RES_VEC_SLOT = 1 << 0,
    RES_SCA_SLOT = 1 << 1,

    RES_A0_READ  = 1 << 2,
    RES_A0_WRITE = 1 << 3,

    RES_CC_READ  = 1 << 4,
    RES_CC_WRITE = 1 << 5,
};

struct vliw_bundle {
        struct vector_instr vinstr;
        struct scalar_instr sinstr;
        struct addr_reg addr_reg_src;
        struct addr_reg addr_reg_dst;

        // CC related stuff
        unsigned cc_id;
        enum swizzle cc_swz[4];
        bool cc_check;
        enum cc_func cc_func;

        // Addr Reg related stuff.
        unsigned addr_id;
        enum swizzle addr_swz;


        // for scheduler / bundler
        unsigned latency;
        unsigned resources; // enum resources
};


#include <stdint.h>

union vliw_instr{
        struct __attribute__((packed)) {
                unsigned LAST : 1;          //   0
                unsigned CTX_INDX : 1;      //   1
                unsigned OUT : 5;           //   2 .. 6
                unsigned SRT_ADDR : 6;      //   7 .. 12
                unsigned VWE : 4;           //  13 .. 16
                unsigned SWE : 4;           //  17 .. 20
                unsigned rC : 17;           //  21 .. 37
                unsigned rB : 17;           //  38 .. 54
                unsigned rA : 17;           //  55 .. 71
                unsigned IBUF_ADDR : 4;     //  72 .. 75
                unsigned CTX_ADDR : 10;     //  76 .. 85
                unsigned OPCODE_V : 5;      //  86 .. 90
                unsigned OPCODE_S : 5;      //  91 .. 95
                unsigned SCALAR_SEL : 2;    //  96 .. 97
                unsigned RCC_EXTR : 8;      //  98 .. 105
                unsigned RCC_COMPARE : 3;   // 106 .. 108
                unsigned MOD_WE : 1;        // 109
                unsigned RCC_WEN : 1;       // 110
                unsigned RT_ADDR : 6;       // 111 .. 116
                unsigned RA_ABS : 1;        // 117
                unsigned RB_ABS : 1;        // 118
                unsigned RC_ABS : 1;        // 119
                unsigned OFFREG_RA : 1;     // 120
                unsigned CC_SEL : 1;        // 121
                unsigned SATURATE : 1;      // 122
                unsigned IBUF_INDX : 1;     // 123
                unsigned OBUF_INDX : 1;     // 124
                unsigned CC_WR_SEL : 1;     // 125
                unsigned OUT_SEL : 1;       // 126
                unsigned RSVD : 1;        // 127
        };

        uint32_t words[4];
};





// Packaging logic.
struct gir_vector {};
struct gir_scalar {};
struct git_cc {};
struct gir_addr {};

bool can_bundle(struct gir_vector v, struct gir_scalar s) {
        // gg ez
        return false;


        // not so ez.
        // Check if:
        // - CC and AddrReg match.
        // - dst does not overlap between Vector and Scalar
        // - src of MAD/ADD overlaps with scalar
        // - Saturate
        // - RAW: Read-After-Write Hazards
}



union vliw_instr bundle_instr(struct gir_vector *v, struct gir_scalar *s, struct gir_cc *cc, struct gir_addr *addr) {
        union vliw_instr vliw_instr;


        return vliw_instr;
}
// CC check is VLIW bundle global.
// CC writing depends on instruction.

void pack_bundle(struct vliw_bundle *bundle, bool last) {
        // emit machine code
}