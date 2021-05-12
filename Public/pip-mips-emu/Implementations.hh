// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_IMPLEMENTATIONS_HH
#define PIP_MIPS_EMU_IMPLEMENTATIONS_HH

#include <pip-mips-emu/Components.hh>

class DefaultHandler : public Handler
{
    HANDLER_DECLARE_FUNCTIONS()

  private:
    uint32_t IF_ID_PC;
    uint32_t ID_EX_PC;
    uint32_t EX_MEM_PC;
    uint32_t MEM_WB_PC;
    uint32_t WB_PC;

    uint32_t IF_ID_Instr;
    uint32_t ID_EX_Instr;
    uint32_t EX_MEM_Instr;
    uint32_t MEM_WB_Instr;
    uint32_t WB_Instr;
};

enum class NextPCType : uint16_t
{
    AdvancedPC             = 0,
    JumpResult             = 1,
    BranchResultID         = 2,
    BranchResultMemJump    = 3,
    BranchResultMemRestore = 4,
    NotMutated             = 5,
};

enum class PipelineState : uint16_t
{
    Normal   = 0,
    Stalled  = 1,
    Flushed  = 2,
    Flushed3 = 3,
};

class ATPPipelineStateController : public Controller
{
    CONTROLLER_DECLARE_FUNCTIONS();

  private:
    // Registers to read
    uint32_t IF_ID_Instr;

    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_Instr;

    uint32_t ID_EX_MemRead;
    uint32_t ID_EX_Reg2;

    // Signals
    uint32_t nextPCType;
    uint32_t pipelineState;
};

class ANTPPipelineStateController : public Controller
{
    CONTROLLER_DECLARE_FUNCTIONS();

  private:
    // Registers to read
    uint32_t IF_ID_Instr;

    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_Instr;

    uint32_t ID_EX_MemRead;
    uint32_t ID_EX_Reg2;

    // Signals
    uint32_t nextPCType;
    uint32_t pipelineState;
};

class InstructionFetch : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t PC;

    // Registers to write
    uint32_t IF_ID_PC;
    uint32_t IF_ID_NextPC;
    uint32_t IF_ID_Instr;

    // Signals
    uint32_t nextPCType;
    uint32_t pipelineState;
};

class InstructionDecode : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t IF_ID_PC;
    uint32_t IF_ID_NextPC;
    uint32_t IF_ID_Instr;

    // Registers to forward
    uint32_t ID_EX_PC;
    uint32_t ID_EX_NextPC;
    uint32_t ID_EX_Instr;

    // Registers to write
    uint32_t ID_EX_RegWrite;
    uint32_t ID_EX_MemWrite;
    uint32_t ID_EX_MemRead;

    uint32_t ID_EX_Reg1Value;
    uint32_t ID_EX_Reg2Value;

    uint32_t ID_EX_Imm;
    uint32_t ID_EX_Reg1;
    uint32_t ID_EX_Reg2;
    uint32_t ID_EX_Reg3;

    uint32_t ID_EX_RAWrite;
    uint32_t ID_EX_RAValue;

    uint32_t PC;

    // Signals
    uint32_t nextPCType;
    uint32_t pipelineState;
};

class Execution : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t ID_EX_PC;
    uint32_t ID_EX_NextPC;
    uint32_t ID_EX_Instr;

    uint32_t ID_EX_RegWrite;
    uint32_t ID_EX_MemWrite;
    uint32_t ID_EX_MemRead;

    uint32_t ID_EX_Reg1Value;
    uint32_t ID_EX_Reg2Value;

    uint32_t ID_EX_Imm;
    uint32_t ID_EX_Reg1;
    uint32_t ID_EX_Reg2;
    uint32_t ID_EX_Reg3;

    uint32_t ID_EX_RAWrite;
    uint32_t ID_EX_RAValue;

    // Registers to forward
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_NextPC;
    uint32_t EX_MEM_Instr;

    uint32_t EX_MEM_RegWrite;
    uint32_t EX_MEM_MemWrite;
    uint32_t EX_MEM_MemRead;
    uint32_t EX_MEM_Reg2Value;

    uint32_t EX_MEM_Reg2;

    uint32_t EX_MEM_RAWrite;
    uint32_t EX_MEM_RAValue;

    // Registers to write
    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_DestReg;

    // Registers for forwarding
    uint32_t MEM_WB_RegWrite;
    uint32_t MEM_WB_MemRead;
    uint32_t MEM_WB_DestReg;
    uint32_t MEM_WB_ReadData;

    // Signals
    uint32_t pipelineState;
};

class MemoryAccess : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_NextPC;
    uint32_t EX_MEM_Instr;

    uint32_t EX_MEM_RegWrite;
    uint32_t EX_MEM_MemWrite;
    uint32_t EX_MEM_MemRead;
    uint32_t EX_MEM_Reg2Value;

    uint32_t EX_MEM_Reg2;

    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_DestReg;

    uint32_t EX_MEM_RAWrite;
    uint32_t EX_MEM_RAValue;

    // Registers to forward
    uint32_t MEM_WB_PC;
    uint32_t MEM_WB_Instr;

    uint32_t MEM_WB_RegWrite;
    uint32_t MEM_WB_MemRead;

    uint32_t MEM_WB_ALUResult;
    uint32_t MEM_WB_DestReg;

    uint32_t MEM_WB_RAWrite;
    uint32_t MEM_WB_RAValue;

    // Registers to write
    uint32_t MEM_WB_ReadData;

    uint32_t PC;

    // Signals
    uint32_t nextPCType;
};

class WriteBack : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t MEM_WB_PC;
    uint32_t MEM_WB_Instr;

    uint32_t MEM_WB_RegWrite;
    uint32_t MEM_WB_MemRead;

    uint32_t MEM_WB_ALUResult;
    uint32_t MEM_WB_DestReg;

    uint32_t MEM_WB_ReadData;

    uint32_t MEM_WB_RAWrite;
    uint32_t MEM_WB_RAValue;

    // Registers to forward
    uint32_t WB_PC;
    uint32_t WB_Instr;

    // Registers to write
    uint32_t RA;
};

#endif