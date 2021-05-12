// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

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
    uint32_t Branch;
};

class InstructionDecode : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t IF_ID_PC;
    uint32_t IF_ID_NextPC;
    uint32_t IF_ID_Instr;

    // Registers to write
    uint32_t ID_EX_PC;
    uint32_t ID_EX_NextPC;
    uint32_t ID_EX_Reg1Value;
    uint32_t ID_EX_Reg2Value;
    uint32_t ID_EX_Imm;
    uint32_t ID_EX_Instr;
};

class Execution : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t ID_EX_PC;
    uint32_t ID_EX_NextPC;
    uint32_t ID_EX_Reg1Value;
    uint32_t ID_EX_Reg2Value;
    uint32_t ID_EX_Imm;
    uint32_t ID_EX_Instr;

    // Registers to write
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_BranchResult;
    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_Reg2Value;
    uint32_t EX_MEM_Instr;
};

class MemoryAccess : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t EX_MEM_PC;
    uint32_t EX_MEM_BranchResult;
    uint32_t EX_MEM_ALUResult;
    uint32_t EX_MEM_Reg2Value;
    uint32_t EX_MEM_Instr;

    // Registers to write
    uint32_t PC;
    uint32_t MEM_WB_PC;
    uint32_t MEM_WB_ReadData;
    uint32_t MEM_WB_ALUResult;
    uint32_t MEM_WB_Instr;
};

class WriteBack : public Datapath
{
    DATAPATH_DECLARE_FUNCTIONS()

  private:
    // Registers to read
    uint32_t MEM_WB_PC;
    uint32_t MEM_WB_ReadData;
    uint32_t MEM_WB_ALUResult;
    uint32_t MEM_WB_Instr;

    // Registers to write
    uint32_t WB_PC;
};