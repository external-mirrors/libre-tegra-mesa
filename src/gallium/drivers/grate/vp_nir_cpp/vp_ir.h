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
   VP_VEC_OP_ARA = 24,
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
    {VP_VEC_OP_ARA, "vARA"},
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
    VP_SCALAR_OP_BRA = 9,
    VP_SCALAR_OP_CAL = 11,
    VP_SCALAR_OP_RET = 12,
    VP_SCALAR_OP_LG2 = 13,
    VP_SCALAR_OP_EX2 = 14,
    VP_SCALAR_OP_SIN = 15,
    VP_SCALAR_OP_COS = 16,
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
    {VP_SCALAR_OP_RET, "sRET"},
    {VP_SCALAR_OP_LG2, "sLG2"},
    {VP_SCALAR_OP_EX2, "sEX2"},
    {VP_SCALAR_OP_SIN, "sSIN"},
    {VP_SCALAR_OP_COS, "sCOS"},
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

class gir_instruction;

class gir_ssa_def {
    int virt_id;
    gir_instruction *def; // assigning instruction
    unsigned num_uses;
};

class gir_reg {
public:
    int hw_id;
    reg_type storage_type; // Storage Type // Assigned by reg allocator
};

class gir_source: public gir_reg {
    bool abs, neg;
    
    std::array<enum vp_swz, 4> swizzle; // logical/math swizzle
};

class gir_source_tmp: public gir_reg {
    gir_ssa_def *def;
};

class gir_source_attr: public gir_reg {
    
};

class gir_source_const: public gir_reg {
    
};

class gir_dest: public gir_reg {
    gir_ssa_def *def;

    bool sat;
    int write_mask; // logical/math mask

};

class gir_instruction {
public:
    int a;
    std::string name;
    std::vector<std::shared_ptr<gir_source>> src;
    // CC
    std::shared_ptr<gir_dest> dst;


    virtual void print(std::ostream& os) const  = 0;

};

std::ostream& operator<<(std::ostream& os, gir_instruction& instr) {
    instr.print(os);
    return os;
}

class gir_alu_scalar_instr: public gir_instruction {
public:
    enum vp_scalar_op op;

    gir_alu_scalar_instr(enum vp_scalar_op op):
    op{op} {};

    void print(std::ostream& os) const override {
        os << s_ops.at(op);
    }
};

class gir_alu_vector_instr: public gir_instruction {
public:
    enum vp_vector_op op;

    gir_alu_vector_instr(enum vp_vector_op op):
    op{op} {};

    void print(std::ostream& os) const override {
        os << v_ops.at(op);
    }
};

class gir_alu_vector_bin_instr: public gir_instruction {
    enum vp_vector_op op;
    void print(std::ostream& os) const override {
        os << v_ops.at(op);
    }
};

class gir_bundle {
    // Convert to smart pointers
    std::unique_ptr<gir_alu_scalar_instr> scalar;
    std::unique_ptr<gir_alu_vector_instr> vector;
};



#endif