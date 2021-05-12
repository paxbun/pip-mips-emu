// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Formats.hh>
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
    return memory.GetRegister(WB_PC) + 4
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

        uint32_t const content = memory.GetRegister(reg);
        if (content)
            stream << std::hex << content;
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

// ------------------------------------ NextPCController  -------------------------------------- //

#pragma region NextPCController

CONTROLLER_INIT(NextPCController)
{
    REGISTER_READ(IF_ID_Instr);
    REGISTER_READ(EX_MEM_Instr);
    REGISTER_READ(EX_MEM_ALUResult);

    MAKE_SIGNAL(nextPCType);
}

CONTROLLER_EXEC(NextPCController)
{
    DEFINE_CONTROLS();

    uint32_t if_id_instr     = memory.GetRegister(IF_ID_Instr);
    uint32_t if_id_operation = (if_id_instr >> 26) & 0b111111;
    uint32_t if_id_function  = (if_id_instr >> 0) & 0b111111;
    if (if_id_function == 0 && IsOneOf(if_id_function, JRFormatFn::JR))
    {
        ADD_CONTROL((Control::New(nextPCType, NextPCType::JumpResult)));
        RETURN_CONTROLS();
    }

    uint32_t ex_mem_instr     = memory.GetRegister(EX_MEM_Instr);
    uint32_t ex_mem_aluResult = memory.GetRegister(EX_MEM_ALUResult);
    uint32_t ex_mem_operation = (ex_mem_instr >> 26) & 0b111111;
    if (ex_mem_aluResult && IsOneOf(ex_mem_operation, BIFormatOp::BEQ, BIFormatOp::BNE))
    {
        ADD_CONTROL((Control::New(nextPCType, NextPCType::BranchResult)));
        RETURN_CONTROLS();
    }

    ADD_CONTROL((Control::New(nextPCType, NextPCType::AdvancedPC)));
    RETURN_CONTROLS();
}

#pragma endregion NextPCController

// ------------------------------------ InstructionFetch  -------------------------------------- //

#pragma region InstructionFetch

DATAPATH_INIT(InstructionFetch)
{
    REGISTER_READ(PC);

    REGISTER_WRITE(IF_ID_PC);
    REGISTER_WRITE(IF_ID_NextPC);
    REGISTER_WRITE(IF_ID_Instr);

    SIGNAL(nextPCType);
}

DATAPATH_EXEC(InstructionFetch)
{
    DEFINE_DELTAS();

    uint32_t const pcValue = memory.GetRegister(PC);
    if (pcValue >= memory.GetTextSize())
    {
        ADD_DELTA((Delta::Register(IF_ID_NextPC, 0)));
        ADD_DELTA((Delta::Register(IF_ID_Instr, 0)));
    }
    else
    {
        uint32_t const instruction = memory.GetWord(Address::MakeFromWord(pcValue));
        uint32_t const newPCValue  = pcValue + 4;

        ADD_DELTA((Delta::Conditioned(PC, newPCValue, nextPCType, NextPCType::AdvancedPC)));
        ADD_DELTA((Delta::Register(IF_ID_PC, pcValue)));
        ADD_DELTA((Delta::Register(IF_ID_NextPC, newPCValue)));
        ADD_DELTA((Delta::Register(IF_ID_Instr, instruction)));
    }

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
    REGISTER_WRITE(ID_EX_Instr);

    REGISTER_WRITE(ID_EX_RegWrite);
    REGISTER_WRITE(ID_EX_MemWrite);
    REGISTER_WRITE(ID_EX_MemRead);

    REGISTER_WRITE(ID_EX_Reg1Value);
    REGISTER_WRITE(ID_EX_Reg2Value);

    REGISTER_WRITE(ID_EX_Imm);
    REGISTER_WRITE(ID_EX_Reg2);
    REGISTER_WRITE(ID_EX_Reg3);
}

DATAPATH_EXEC(InstructionDecode)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(IF_ID_PC, ID_EX_PC);
    FORWARD_REGISTER(IF_ID_NextPC, ID_EX_NextPC);
    FORWARD_REGISTER(IF_ID_Instr, ID_EX_Instr);

    uint32_t const instruction = memory.GetRegister(IF_ID_Instr);

    uint32_t regWrite = 0, memWrite = 0, memRead = 0;

    uint32_t const operation = (instruction >> 26) & 0b111111;
    if (operation == 0)
    {
        uint32_t const function = (instruction >> 0) & 0b111111;
        if (!IsOneOf(function, JRFormatFn::JR))
            regWrite = 1;
    }
    else if (IsOneOf(operation,
                     IFormatOp::ADDIU,
                     IFormatOp::ANDI,
                     IFormatOp::ORI,
                     IFormatOp::SLTIU,
                     IIFormatOp::LUI,
                     JFormatOp::JAL))
    {
        regWrite = 1;
    }
    else if (IsOneOf(operation, OIFormatOp::LB, OIFormatOp::LW))
    {
        regWrite = 1;
        memRead  = 1;
    }
    else if (IsOneOf(operation, OIFormatOp::SB, OIFormatOp::SW))
    {
        memWrite = 1;
    }

    // branch calculation

    uint32_t const register1 = (instruction >> 21) & 0b11111;
    uint32_t const register2 = (instruction >> 16) & 0b11111;
    uint32_t const register3 = (instruction >> 11) & 0b11111;
    uint32_t const immediate = (instruction >> 0) & 0xFFFF;

    uint32_t const register1Value = memory.GetRegister(register1);
    uint32_t const register2Value = memory.GetRegister(register2);

    ADD_DELTA((Delta::Register(ID_EX_RegWrite, regWrite)));
    ADD_DELTA((Delta::Register(ID_EX_MemWrite, memWrite)));
    ADD_DELTA((Delta::Register(ID_EX_MemRead, memRead)));

    ADD_DELTA((Delta::Register(ID_EX_Reg1Value, register1Value)));
    ADD_DELTA((Delta::Register(ID_EX_Reg2Value, register2Value)));

    ADD_DELTA((Delta::Register(ID_EX_Imm, immediate)));
    ADD_DELTA((Delta::Register(ID_EX_Reg1, register1)));
    ADD_DELTA((Delta::Register(ID_EX_Reg2, register2)));
    ADD_DELTA((Delta::Register(ID_EX_Reg3, register3)));

    RETURN_DELTAS();
}

#pragma endregion InstructionDecode

// ---------------------------------------- Execution ------------------------------------------ //

#pragma region Execution

DATAPATH_INIT(Execution)
{
    REGISTER_READ(ID_EX_PC);
    REGISTER_READ(ID_EX_NextPC);
    REGISTER_READ(ID_EX_Instr);

    REGISTER_READ(ID_EX_RegWrite);
    REGISTER_READ(ID_EX_MemWrite);
    REGISTER_READ(ID_EX_MemRead);

    REGISTER_READ(ID_EX_Reg1Value);
    REGISTER_READ(ID_EX_Reg2Value);

    REGISTER_READ(ID_EX_Imm);
    REGISTER_READ(ID_EX_Reg1);
    REGISTER_READ(ID_EX_Reg2);
    REGISTER_READ(ID_EX_Reg3);

    REGISTER_WRITE(EX_MEM_PC);
    REGISTER_WRITE(EX_MEM_Instr);

    REGISTER_READ_WRITE(EX_MEM_RegWrite);
    REGISTER_WRITE(EX_MEM_MemWrite);
    REGISTER_WRITE(EX_MEM_MemRead);
    REGISTER_WRITE(EX_MEM_Reg2Value);

    REGISTER_READ_WRITE(EX_MEM_ALUResult);
    REGISTER_READ_WRITE(EX_MEM_DestReg);

    REGISTER_READ(MEM_WB_RegWrite);
    REGISTER_READ(MEM_WB_MemRead);
    REGISTER_READ(MEM_WB_DestReg);
    REGISTER_READ(MEM_WB_ReadData);
}

DATAPATH_EXEC(Execution)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(ID_EX_PC, EX_MEM_PC);
    FORWARD_REGISTER(ID_EX_Instr, EX_MEM_Instr);

    FORWARD_REGISTER(ID_EX_RegWrite, EX_MEM_RegWrite);
    FORWARD_REGISTER(ID_EX_MemWrite, EX_MEM_MemWrite);
    FORWARD_REGISTER(ID_EX_MemRead, EX_MEM_MemRead);

    uint32_t const instruction = memory.GetRegister(ID_EX_Instr);
    uint32_t const operation   = (instruction >> 26) & 0b111111;
    uint32_t const register1   = memory.GetRegister(ID_EX_Reg1);
    uint32_t const register2   = memory.GetRegister(ID_EX_Reg2);
    uint32_t const register3   = memory.GetRegister(ID_EX_Reg3);

    uint32_t const immediate = memory.GetRegister(ID_EX_Imm);

    uint32_t source1Value = memory.GetRegister(ID_EX_Reg1Value);
    uint32_t source2Value = memory.GetRegister(ID_EX_Reg2Value);

    uint32_t const ex_mem_regWrite = memory.GetRegister(EX_MEM_RegWrite);
    uint32_t const ex_mem_destReg  = memory.GetRegister(EX_MEM_DestReg);

    uint32_t const mem_wb_regWrite = memory.GetRegister(MEM_WB_RegWrite);
    uint32_t const mem_wb_memRead  = memory.GetRegister(MEM_WB_MemRead);
    uint32_t const mem_wb_destReg  = memory.GetRegister(MEM_WB_DestReg);

    // EX/MEM to EX forwarding
    if (ex_mem_regWrite && ex_mem_destReg)
    {
        uint32_t const ex_mem_aluResult = memory.GetRegister(EX_MEM_ALUResult);
        if (ex_mem_destReg == register1)
            source1Value = ex_mem_aluResult;
        if (ex_mem_destReg == register2)
            source2Value = ex_mem_aluResult;
    }
    // MEM/WB to EX forwarding
    else if (mem_wb_regWrite && mem_wb_destReg && mem_wb_memRead)
    {
        uint32_t const mem_wb_readData = memory.GetRegister(MEM_WB_ReadData);
        if (mem_wb_destReg == register1)
            source1Value = mem_wb_readData;
        if (mem_wb_destReg == register2)
            source2Value = mem_wb_readData;
    }

    ADD_DELTA(Delta::Register(EX_MEM_Reg2Value, source2Value));

    uint32_t destinationValue = 0;
    uint32_t destination      = register2;
    if (operation == 0)
    {
        // R format
        uint32_t const function    = (instruction >> 0) & 0b111111;
        uint32_t const shiftAmount = (instruction >> 6) & 0b11111;

        if (IsOneOf(function, SRFormatFn::SLL, SRFormatFn::SRL))
        {
            switch (static_cast<SRFormatFn>(function))
            {
            case SRFormatFn::SLL: destinationValue = source2Value << shiftAmount; break;
            case SRFormatFn::SRL: destinationValue = source2Value >> shiftAmount; break;
            }
        }
        else
        {
            switch (static_cast<RFormatFn>(function))
            {
            case RFormatFn::ADDU: destinationValue = source1Value + source2Value; break;
            case RFormatFn::SUBU: destinationValue = source1Value - source2Value; break;
            case RFormatFn::AND: destinationValue = source1Value & source2Value; break;
            case RFormatFn::NOR: destinationValue = ~(source1Value | source2Value); break;
            case RFormatFn::OR: destinationValue = source1Value | source2Value; break;
            case RFormatFn::SLTU: destinationValue = source1Value < source2Value; break;
            }
        }

        destination = register3;
    }
    else if (IsOneOf(
                 operation, IFormatOp::ADDIU, IFormatOp::ANDI, IFormatOp::ORI, IFormatOp::SLTIU))
    {
        switch (static_cast<IFormatOp>(operation))
        {
        case IFormatOp::ADDIU:
        {
            destinationValue = source1Value + SignExtend(immediate, 16);
            break;
        }
        case IFormatOp::ANDI:
        {
            destinationValue = source1Value & immediate;
            break;
        }
        case IFormatOp::ORI:
        {
            destinationValue = source1Value | immediate;
            break;
        }
        case IFormatOp::SLTIU:
        {
            destinationValue
                = static_cast<uint32_t>(static_cast<int32_t>(source1Value)
                                        < static_cast<int32_t>(SignExtend(immediate, 16)));
            break;
        }
        }
    }
    else if (IsOneOf(operation, IIFormatOp::LUI))
    {
        destinationValue = (immediate << 16);
    }
    else if (IsOneOf(operation, OIFormatOp::LB, OIFormatOp::LW, OIFormatOp::SB, OIFormatOp::SW))
    {
        destinationValue = source1Value + SignExtend(immediate, 16);
    }
    ADD_DELTA((Delta::Register(EX_MEM_ALUResult, destinationValue)));
    ADD_DELTA((Delta::Register(EX_MEM_DestReg, destination)));

    RETURN_DELTAS();
}

#pragma endregion Execution

// -------------------------------------- MemoryAccess  ---------------------------------------- //

#pragma region MemoryAccess

DATAPATH_INIT(MemoryAccess)
{
    TICK();

    // Registers to read
    REGISTER_READ(EX_MEM_PC);
    REGISTER_READ(EX_MEM_Instr);

    REGISTER_READ(EX_MEM_RegWrite);
    REGISTER_READ(EX_MEM_MemWrite);
    REGISTER_READ(EX_MEM_MemRead);
    REGISTER_READ(EX_MEM_Reg2Value);

    // REGISTER_READ(EX_MEM_BranchResult);
    REGISTER_READ(EX_MEM_ALUResult);
    REGISTER_READ(EX_MEM_DestReg);

    // Registers to forward
    REGISTER_WRITE(MEM_WB_PC);
    REGISTER_WRITE(MEM_WB_Instr);

    REGISTER_WRITE(MEM_WB_RegWrite);

    REGISTER_WRITE(MEM_WB_ALUResult);
    REGISTER_WRITE(MEM_WB_DestReg);

    // Registers to write
    REGISTER_WRITE(MEM_WB_ReadData);
}

DATAPATH_EXEC(MemoryAccess)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(EX_MEM_PC, MEM_WB_PC);
    FORWARD_REGISTER(EX_MEM_Instr, MEM_WB_Instr);

    FORWARD_REGISTER(EX_MEM_RegWrite, MEM_WB_RegWrite);
    FORWARD_REGISTER(EX_MEM_MemRead, MEM_WB_MemRead);

    FORWARD_REGISTER(EX_MEM_ALUResult, MEM_WB_ALUResult);
    FORWARD_REGISTER(EX_MEM_DestReg, MEM_WB_DestReg);

    uint32_t       readData    = 0;
    uint32_t const writeData   = memory.GetRegister(EX_MEM_Reg2Value);
    uint32_t const instruction = memory.GetRegister(EX_MEM_Instr);
    uint32_t const memoryRead  = memory.GetRegister(EX_MEM_MemRead);
    uint32_t const memoryWrite = memory.GetRegister(EX_MEM_MemWrite);
    uint32_t const operation   = (instruction >> 26) & 0b111111;

    Address const address = Address::MakeFromWord(memory.GetRegister(EX_MEM_ALUResult));
    if (memoryRead)
    {
        if ((operation & 0b11) == 0b11)
            readData = memory.GetWord(address);
        else
            readData = SignExtend(memory.GetByte(address), 8);
    }
    if (memoryWrite)
    {
        if ((operation & 0b11) == 0b11)
            ADD_DELTA(Delta::MemoryWord(address, writeData));
        else
            ADD_DELTA(Delta::MemoryByte(address, static_cast<uint8_t>(writeData & 0xFF)));
    }
    ADD_DELTA(Delta::Register(MEM_WB_ReadData, readData));

    RETURN_DELTAS();
}

#pragma endregion MemoryAccess

// ---------------------------------------- WriteBack ------------------------------------------ //

#pragma region WriteBack

DATAPATH_INIT(WriteBack)
{

    REGISTER_READ(MEM_WB_PC);
    REGISTER_READ(MEM_WB_Instr);

    REGISTER_READ(MEM_WB_RegWrite);
    REGISTER_READ(MEM_WB_MemRead);

    REGISTER_READ(MEM_WB_ALUResult);
    REGISTER_READ(MEM_WB_DestReg);

    REGISTER_READ(MEM_WB_ReadData);

    REGISTER_WRITE(WB_PC);
}

DATAPATH_EXEC(WriteBack)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(MEM_WB_PC, WB_PC);

    uint32_t const instruction = memory.GetRegister(MEM_WB_Instr);
    uint32_t const regWrite    = memory.GetRegister(MEM_WB_RegWrite);
    uint32_t const memoryRead  = memory.GetRegister(MEM_WB_MemRead);
    uint32_t const aluResult   = memory.GetRegister(MEM_WB_ALUResult);
    uint32_t const destination = memory.GetRegister(MEM_WB_DestReg);
    uint32_t const readData    = memory.GetRegister(MEM_WB_ReadData);

    if (regWrite)
    {
        uint32_t destinationValue = 0;
        if (memoryRead)
            destinationValue = readData;
        else
            destinationValue = aluResult;
        ADD_DELTA((Delta::Register(destination, destinationValue)));
    }

    RETURN_DELTAS();
}

#pragma endregion WriteBack