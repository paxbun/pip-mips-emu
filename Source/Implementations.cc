// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Implementations.hh>

namespace
{

template <typename T, typename... Ts>
inline bool IsOneOf(T value, Ts... ts) noexcept
{
    return ((static_cast<T>(ts) == value) || ...);
}

constexpr uint32_t SignExtend(uint32_t value, uint32_t numBits) noexcept
{
    // 0  if value >= 0
    // -1 otherwise
    uint32_t const mask = ~(value >> (numBits - 1)) + 1;
    return value | (mask << numBits);
}

}

// ------------------------------------- DefaultHandler  --------------------------------------- //

#pragma region DefaultHandler

HANDLER_INIT(DefaultHandler)
{
    REGISTER_READ(IF_ID_PC);
    REGISTER_READ(ID_EX_PC);
    REGISTER_READ(EX_MEM_PC);
    REGISTER_READ(MEM_WB_PC);
    REGISTER_READ(WB_PC);
}

HANDLER_IS_TERMINATED(DefaultHandler)
{
    return memory.GetRegister(WB_PC)
           >= static_cast<uint32_t>(Address::MakeText(memory.GetTextSize()));
}

HANDLER_DUMP_PCS(DefaultHandler)
{
    std::ios_base::fmtflags flags = stream.flags();

    uint32_t registers[] = { IF_ID_PC, ID_EX_PC, EX_MEM_PC, MEM_WB_PC, WB_PC };

    stream << "Current pipeline PC state:\n";

    bool first = true;
    for (uint32_t reg : registers)
    {
        if (first)
            stream << '{';
        else
            stream << '|';

        first = false;
        stream << std::hex << memory.GetRegister(reg);
    }
    stream << "}\n";

    stream.flags(flags);
}

HANDLER_DUMP_REGISTERS(DefaultHandler)
{
    std::ios_base::fmtflags flags = stream.flags();

    stream << "Current register values:\n";
    stream << "------------------------------------\n";
    stream << "PC: 0x" << std::hex << memory.GetRegister(Memory::PC) << '\n';
    stream << "Registers:\n";

    for (uint32_t idx = 0; idx < Memory::PC; ++idx)
    {
        stream << "R" << std::dec << idx << ": 0x" << std::hex << memory.GetRegister(idx) << '\n';
    }

    stream.flags(flags);
}

HANDLER_DUMP_MEMORY(DefaultHandler)
{
    if (static_cast<uint32_t>(range.begin) > static_cast<uint32_t>(range.end))
        throw std::invalid_argument { "invalid memory range" };

    std::ios_base::fmtflags flags = stream.flags();

    stream << std::hex;
    stream << "Memory content [" << range.begin << ".." << range.end << "]:\n";
    stream << "------------------------------------\n";

    for (uint32_t current = range.begin; current <= range.end; current += 4)
    {
        Address address = Address::MakeFromWord(current);
        stream << address << ": 0x" << memory.GetWord(address) << '\n';
    }

    stream.flags(flags);
}

#pragma endregion

// ------------------------------------ InstructionFetch  -------------------------------------- //

#pragma region InstructionFetch

DATAPATH_INIT(InstructionFetch)
{
    REGISTER_READ(PC);

    REGISTER_WRITE(IF_ID_PC);
    REGISTER_WRITE(IF_ID_NextPC);
    REGISTER_WRITE(IF_ID_Instr);

    SIGNAL(Branch);
}

DATAPATH_EXEC(InstructionFetch)
{
    DEFINE_DELTAS();

    uint32_t const pcValue     = memory.GetRegister(PC);
    uint32_t const instruction = memory.GetWord(Address::MakeFromWord(pcValue));
    uint32_t const newPcValue  = pcValue + 4;

    ADD_DELTA(Delta::Conditioned(PC, newPcValue, Branch, 0));
    ADD_DELTA(Delta::Register(IF_ID_PC, pcValue));
    ADD_DELTA(Delta::Register(IF_ID_NextPC, newPcValue));
    ADD_DELTA(Delta::Register(IF_ID_Instr, instruction));

    RETURN_DELTAS();
}

#pragma endregion InstructionFetch

// ------------------------------------ InstructionDecode -------------------------------------- //

#pragma region InstructionDecode

DATAPATH_INIT(InstructionDecode)
{
    TOCK();

    REGISTER_READ(IF_ID_PC);
    REGISTER_READ(IF_ID_NextPC);
    REGISTER_READ(IF_ID_Instr);

    REGISTER_WRITE(ID_EX_PC);
    REGISTER_WRITE(ID_EX_NextPC);
    REGISTER_WRITE(ID_EX_Reg1Value);
    REGISTER_WRITE(ID_EX_Reg2Value);
    REGISTER_WRITE(ID_EX_Imm);
}

DATAPATH_EXEC(InstructionDecode)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(IF_ID_PC, ID_EX_PC);
    FORWARD_REGISTER(IF_ID_NextPC, ID_EX_NextPC);
    FORWARD_REGISTER(IF_ID_Instr, ID_EX_Instr);

    uint32_t const instruction = memory.GetRegister(IF_ID_Instr);
    uint32_t const register1   = (instruction >> 21) & 0b11111;
    uint32_t const register2   = (instruction >> 16) & 0b11111;
    uint32_t const immediate   = (instruction >> 0) & 0xFFFF;

    uint32_t const register1Value = memory.GetRegister(register1);
    uint32_t const register2Value = memory.GetRegister(register2);

    ADD_DELTA(Delta::Register(ID_EX_Reg1Value, register1Value));
    ADD_DELTA(Delta::Register(ID_EX_Reg2Value, register2Value));
    ADD_DELTA(Delta::Register(ID_EX_Imm, SignExtend(immediate, 16)));

    RETURN_DELTAS();
}

#pragma endregion InstructionDecode

// ---------------------------------------- Execution ------------------------------------------ //

#pragma region Execution

DATAPATH_INIT(Execution)
{
    REGISTER_READ(ID_EX_PC);
    REGISTER_READ(ID_EX_NextPC);
    REGISTER_READ(ID_EX_Reg1Value);
    REGISTER_READ(ID_EX_Reg2Value);
    REGISTER_READ(ID_EX_Imm);
    REGISTER_READ(ID_EX_Instr);

    REGISTER_WRITE(EX_MEM_PC);
    REGISTER_WRITE(EX_MEM_BranchResult);
    REGISTER_WRITE(EX_MEM_ALUResult);
    REGISTER_WRITE(EX_MEM_Reg2Value);
    REGISTER_WRITE(EX_MEM_Instr);
}

DATAPATH_EXEC(Execution)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(ID_EX_PC, EX_MEM_PC);
    FORWARD_REGISTER(ID_EX_Reg2Value, EX_MEM_Reg2Value);
    FORWARD_REGISTER(ID_EX_Instr, EX_MEM_Instr);

    uint32_t const branchOffset = memory.GetRegister(ID_EX_Imm) << 2;
    uint32_t const branchResult = memory.GetRegister(ID_EX_NextPC) + branchOffset;
    ADD_DELTA(Delta::Register(EX_MEM_BranchResult, branchResult));

    // TODO

    RETURN_DELTAS();
}

#pragma endregion Execution

// -------------------------------------- MemoryAccess  ---------------------------------------- //

#pragma region MemoryAccess

DATAPATH_INIT(MemoryAccess)
{
    REGISTER_READ(EX_MEM_PC);
    REGISTER_READ(EX_MEM_BranchResult);
    REGISTER_READ(EX_MEM_ALUResult);
    REGISTER_READ(EX_MEM_Reg2Value);
    REGISTER_READ(EX_MEM_Instr);

    REGISTER_WRITE(MEM_WB_ReadData);
    REGISTER_WRITE(MEM_WB_ALUResult);
    REGISTER_WRITE(MEM_WB_Instr);
}

DATAPATH_EXEC(MemoryAccess)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(EX_MEM_PC, MEM_WB_PC);
    FORWARD_REGISTER(EX_MEM_ALUResult, MEM_WB_ALUResult);
    FORWARD_REGISTER(EX_MEM_Instr, MEM_WB_Instr);

    uint32_t const address = memory.GetRegister(EX_MEM_ALUResult);
    // TODO: LB and LW
    uint32_t const content = memory.GetWord(Address::MakeFromWord(address));
    ADD_DELTA(Delta::Register(MEM_WB_ReadData, content));

    // TODO: SB and SW
    uint32_t const writeData = memory.GetRegister(EX_MEM_Reg2Value);
    ADD_DELTA(Delta::MemoryWord(Address::MakeFromWord(address), writeData));

    // TODO: Conditioned
    uint32_t const branchResult = memory.GetRegister(EX_MEM_BranchResult);
    ADD_DELTA(Delta::Register(PC, branchResult));

    RETURN_DELTAS();
}

#pragma endregion MemoryAccess

// ---------------------------------------- WriteBack ------------------------------------------ //

#pragma region WriteBack

DATAPATH_INIT(WriteBack)
{
    REGISTER_READ(MEM_WB_ReadData);
    REGISTER_READ(MEM_WB_ALUResult);
    REGISTER_READ(MEM_WB_Instr);
}

DATAPATH_EXEC(WriteBack)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(MEM_WB_PC, WB_PC);

    // TODO
    ADD_DELTA(Delta::Register(0, MEM_WB_ReadData));
    ADD_DELTA(Delta::Register(0, MEM_WB_ALUResult));

    RETURN_DELTAS();
}

#pragma endregion WriteBack