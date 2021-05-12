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
};

enum class NextPCType : uint16_t
{
    AdvancedPC   = 0,
    JumpResult   = 1,
    BranchResult = 2,
};

class NextPCController : public Controller
{
    CONTROLLER_DECLARE_FUNCTIONS();

  private:
    // Registers to read
    uint32_t IF_ID_Instr;
    uint32_t EX_MEM_Instr;
    uint32_t EX_MEM_ALUResult;

    // Signals
    uint32_t nextPCType;
};

class StallController : public Controller
{
    CONTROLLER_DECLARE_FUNCTIONS();

  private:
    // Registers to read
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
    uint32_t ID_EX_Reg2;
    uint32_t ID_EX_Reg3;
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
    uint32_t ID_EX_Reg2;
    uint32_t ID_EX_Reg3;

    // Registers to forward
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_Instr;

    uint32_t EX_MEM_RegWrite;
    uint32_t EX_MEM_MemWrite;
    uint32_t EX_MEM_MemRead;
    uint32_t EX_MEM_Reg2Value;

    // Registers to write
    // uint32_t EX_MEM_BranchResult;
    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_DestReg;
};

class MemoryAccess : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_Instr;

    uint32_t EX_MEM_RegWrite;
    uint32_t EX_MEM_MemWrite;
    uint32_t EX_MEM_MemRead;
    uint32_t EX_MEM_Reg2Value;

    // uint32_t EX_MEM_BranchResult;
    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_DestReg;

    // Registers to forward
    uint32_t MEM_WB_PC;
    uint32_t MEM_WB_Instr;

    uint32_t MEM_WB_RegWrite;
    uint32_t MEM_WB_MemRead;

    uint32_t MEM_WB_ALUResult;
    uint32_t MEM_WB_DestReg;

    // Registers to write
    uint32_t MEM_WB_ReadData;
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

    // Registers to forward
    uint32_t WB_PC;
};

#endif