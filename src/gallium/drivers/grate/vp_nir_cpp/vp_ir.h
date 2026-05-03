#ifndef CPPVPIR
#define CPPVPIR

#include <cstdint>
#include <memory>
#include <iostream>
#include <array>
#include <unordered_map>
#include <vector>

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

static const std::unordered_map<enum vp_vector_op, std::string> v_ops = {
    {VP_VEC_OP_NOP, "vNOP"},
    {VP_VEC_OP_MOV, "vMOV"},
    {VP_VEC_OP_MUL, "vMUL"},
    {VP_VEC_OP_ADD, "vADD"},
    {VP_VEC_OP_MAD, "vMAD"},
    {VP_VEC_OP_DP3, "vDP3"},
    {VP_VEC_OP_DPH, "vDPH"},
    {VP_VEC_OP_DP4, "vDP4"},
    {VP_VEC_OP_DST, "vDST"},
    {VP_VEC_OP_MIN, "vMIN"},
    {VP_VEC_OP_MAX, "vMAX"},
    {VP_VEC_OP_SLT, "vSLT"},
    {VP_VEC_OP_SGE, "vSGW"},
    {VP_VEC_OP_ARL, "vARL"},
    {VP_VEC_OP_FRC, "vFRC"},
    {VP_VEC_OP_FLR, "vFLR"},
    {VP_VEC_OP_SEQ, "vSEQ"},
    {VP_VEC_OP_SFL, "vSFL"},
    {VP_VEC_OP_SGT, "vSGt"},
    {VP_VEC_OP_SLE, "vSLE"},
    {VP_VEC_OP_SNE, "vSNE"},
    {VP_VEC_OP_STR, "vSTR"},
    {VP_VEC_OP_SSG, "vSSG"},
    {VP_VEC_OP_ARR, "vARR"},
    {VP_VEC_OP_MVA, "vMVA"},
    {VP_VEC_OP_TXL, "vTXL"},
    {VP_VEC_OP_PUSHA, "vPUSHA"},
    {VP_VEC_OP_POPA, "vPOPA}"}
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

static const std::unordered_map<enum vp_scalar_op, std::string> s_ops = {
    {VP_SCALAR_OP_NOP, "sNOP"},
    {VP_SCALAR_OP_MOV, "sMOV"},
    {VP_SCALAR_OP_RCP, "sRCP"},
    {VP_SCALAR_OP_RCC, "sRCC"},
    {VP_SCALAR_OP_RSQ, "sRSQ"},
    {VP_SCALAR_OP_EXP, "sEXP"},
    {VP_SCALAR_OP_LOG, "sLOG"},
    {VP_SCALAR_OP_LIT, "sLIT"},
    {VP_SCALAR_OP_BRA, "sBRA"},
    {VP_SCALAR_OP_BRI, "sBRI"},
    {VP_SCALAR_OP_CLA, "sCLA"},
    {VP_SCALAR_OP_CLI, "sCLI"},
    {VP_SCALAR_OP_RET, "sRET"},
    {VP_SCALAR_OP_LG2, "sLG2"},
    {VP_SCALAR_OP_EX2, "sEX2"},
    {VP_SCALAR_OP_SIN, "sSIN"},
    {VP_SCALAR_OP_COS, "sCOS"},
    {VP_SCALAR_OP_BRB, "sBRB"},
    {VP_SCALAR_OP_CLB, "sCLB"},
    {VP_SCALAR_OP_PUSHA, "sPUSHA"},
    {VP_SCALAR_OP_POPA, "sPOPA"}
};

enum vp_swz {
   VP_SWZ_X = 0,
   VP_SWZ_Y = 1,
   VP_SWZ_Z = 2,
   VP_SWZ_W = 3
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

static const std::unordered_map<enum reg_type, uint8_t> reg_type_store_mask = {
    {REG_TYPE_VEC4, 0b1111},
    {REG_TYPE_VIRT_VEC3_XYZ, 0b0111},
    {REG_TYPE_VIRT_VEC3_XYW, 0b1011},
    {REG_TYPE_VIRT_VEC3_XZW, 0b1101},
    {REG_TYPE_VIRT_VEC3_YZW, 0b1110},
    {REG_TYPE_VIRT_VEC2_XY, 0b0011},
    {REG_TYPE_VIRT_VEC2_XZ, 0b0101},
    {REG_TYPE_VIRT_VEC2_XW, 0b1001},
    {REG_TYPE_VIRT_VEC2_YZ, 0b0110},
    {REG_TYPE_VIRT_VEC2_YW, 0b1010},
    {REG_TYPE_VIRT_VEC2_ZW, 0b1100},
    {REG_TYPE_VIRT_SCALAR_X, 0b0001},
    {REG_TYPE_VIRT_SCALAR_Y, 0b0010},
    {REG_TYPE_VIRT_SCALAR_Z, 0b0100},
    {REG_TYPE_VIRT_SCALAR_W, 0b1000},
};

/*
* Some Operations need to be done on the Vector pipe, some need to be done on
* the scalar pipe, s
*/
enum ops {
//
    OP_NOP, // should NOT appear in the wild. Its detail of scheduling and bundling
// Dataflow ops; Either pipe
    OP_MOV,
    OP_PUSHA,
    OP_POPA,

// Math ops
    // Vector pipe
    VOP_MUL,
    VOP_ADD,
    VOP_MAD,
    VOP_DP3,
    VOP_DPH,
    VOP_DP4,
    VOP_DST,
    VOP_MIN,
    VOP_MAX,
    VOP_FRC,
    VOP_FLR,
    VOP_SSG,
    // Scalar pipe
    SOP_RCP,
    SOP_RCC,
    SOP_RSQ,
    SOP_EXP,
    SOP_LOG,
    SOP_LIT,
    SOP_LG2,
    SOP_EX2,
    SOP_SIN,
    SOP_COS,

// Bool/Comparison ops
    // Vector pipe
    VOP_SFL,
    VOP_SLT,
    VOP_SLE,
    VOP_SEQ,
    VOP_SGE,
    VOP_SGT,
    VOP_STR,
    VOP_SNE,

// Address Register Write
    // Vector pipe
    VOP_ARR,
    VOP_ARL,
    VOP_MVA,

// Texture Access
    // Vector pipe
    VOP_TXL,

// Branch ops
    // Scalar Pipe
    SOP_BRA,
    SOP_CLA,
    SOP_BRI,
    SOP_CLI,
    SOP_BRB,
    SOP_CLB,
    SOP_RET,
};

class gir_instruction; // fwd dcl

class gir_ssa_def {
    int virt_id; // unique, assigend once
    gir_instruction *def; // assigning instruction
    unsigned num_uses;
    unsigned num_components;
};

class gir_reg {
public:
    int hw_id;
    reg_type storage_type; // Storage Type // Assigned by reg allocator
    gir_ssa_def *def;
};

enum src_type {
    src_reg,
    src_attr,
    src_constant
};
class gir_source: public gir_reg {
    enum src_type type;
    bool abs, neg;

    std::array<enum vp_swz, 4> swizzle; // logical/math swizzle

    unsigned addr; // register/attribute/constant address
};


enum dst_type{
    dst_reg,
    dst_out,
    dst_cc
};
class gir_dest: public gir_reg {
    enum dst_type type;
    bool sat;
    int write_mask; // logical/math mask
};

enum cc_func{
    FALSE = 0,
    LT = 1,
    EQ = 2,
    LE = 3,
    GT = 4,
    NE = 5,
    GE = 6,
    TRUE = 7
};
class gir_instruction {
public:
    std::string name;
    enum ops op;

    // CC
    std::shared_ptr<gir_dest> dst; // register
    std::shared_ptr<gir_dest> out; // export/varying
    std::shared_ptr<gir_dest> cc_dst;

    std::array<std::shared_ptr<gir_source>, 3> src; // 3 source operands
    std::shared_ptr<gir_source> cc_src;

    enum cc_func cc_func;

    gir_instruction(
        std::shared_ptr<gir_dest> dst,
        std::shared_ptr<gir_dest> out,
        std::shared_ptr<gir_dest> cc_dst,
        std::shared_ptr<gir_source> src0,
        std::shared_ptr<gir_source> src1,
        std::shared_ptr<gir_source> src2,
        std::shared_ptr<gir_source> cc_src,
        enum cc_func cc_func
    ):
        dst(std::move(dst)),
        out{out},
        cc_dst{cc_dst},
        src{src0, src1, src2, },
        cc_src{cc_src},
        cc_func{cc_func}
    {

    };

    virtual void print(std::ostream& os) const = 0;
};

std::ostream& operator<<(std::ostream& os, gir_instruction& instr) {
    instr.print(os);
    return os;
}

#if 0
// No one cares about this detail (yet)
class gir_alu_scalar_instr: public gir_instruction {
public:
    enum vp_scalar_op op;

    gir_alu_scalar_instr(
        enum vp_scalar_op op,
        std::shared_ptr<gir_dest> dst,
        std::shared_ptr<gir_dest> out,
        std::shared_ptr<gir_dest> cc_dst,
        std::shared_ptr<gir_source> src0,
        std::shared_ptr<gir_source> src1,
        std::shared_ptr<gir_source> src2,
        std::shared_ptr<gir_source> cc_src,
        enum cc_func cc_func
    ): gir_instruction{dst, out, cc_dst, src0, src1, src2, cc_src, cc_func}, op{op}
    {};

    void print(std::ostream& os) const override {
        os << s_ops.at(op);
    }
};

class gir_alu_vector_instr: public gir_instruction {
public:
    enum vp_vector_op op;

    gir_alu_vector_instr(
        enum vp_vector_op op,
        std::shared_ptr<gir_dest> dst,
        std::shared_ptr<gir_dest> out,
        std::shared_ptr<gir_dest> cc_dst,
        std::shared_ptr<gir_source> src0,
        std::shared_ptr<gir_source> src1,
        std::shared_ptr<gir_source> src2,
        std::shared_ptr<gir_source> cc_src,
        enum cc_func cc_func
    ): gir_instruction{dst, out, cc_dst, src0, src1, src2, cc_src, cc_func}, op{op}
    {};

    void print(std::ostream& os) const override {
        os << v_ops.at(op);
    }
};
#endif

class gir_bundle {
    std::unique_ptr<gir_alu_scalar_instr> scalar;
    std::unique_ptr<gir_alu_vector_instr> vector;
};



#endif