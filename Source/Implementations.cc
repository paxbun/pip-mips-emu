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

    REGISTER_READ(IF_ID_Instr);
    REGISTER_READ(ID_EX_Instr);
    REGISTER_READ(EX_MEM_Instr);
    REGISTER_READ(MEM_WB_Instr);
    REGISTER_READ(WB_Instr);
}

HANDLER_IS_TERMINATED(DefaultHandler)
{
    return memory.GetRegister(WB_PC) + 4
           >= static_cast<uint32_t>(Address::MakeText(memory.GetTextSize()));
}

HANDLER_CALC_NUM_INSTRS(DefaultHandler)
{
    return WB_PC && WB_Instr;
}

HANDLER_DUMP_PCS(DefaultHandler)
{
    std::ios_base::fmtflags flags = stream.flags();

    uint32_t registers[]    = { IF_ID_PC, ID_EX_PC, EX_MEM_PC, MEM_WB_PC, WB_PC };
    uint32_t instructions[] = { IF_ID_Instr, ID_EX_Instr, EX_MEM_Instr, MEM_WB_Instr, WB_Instr };

    stream << "Current pipeline PC state:\n";

    for (int i = 0; i < 5; ++i)
    {
        if (i == 0)
            stream << '{';
        else
            stream << '|';

        uint32_t const reg   = registers[i];
        uint32_t const instr = instructions[i];

        uint32_t const content = memory.GetRegister(reg);
        if (content && instr)
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

    REGISTER_READ(ID_EX_MemRead);
    REGISTER_READ(ID_EX_Reg2);

    MAKE_SIGNAL(nextPCType);
    MAKE_SIGNAL(pipelineState);
}

CONTROLLER_EXEC(NextPCController)
{
    DEFINE_CONTROLS();

    uint32_t const if_id_instr     = memory.GetRegister(IF_ID_Instr);
    uint32_t const if_id_operation = (if_id_instr >> 26) & 0b111111;
    uint32_t const if_id_function  = (if_id_instr >> 0) & 0b111111;
    if ((if_id_operation == 0 && IsOneOf(if_id_function, JRFormatFn::JR))
        || IsOneOf(if_id_operation, JFormatOp::J, JFormatOp::JAL))
    {
        ADD_CONTROL((Control::New(nextPCType, NextPCType::JumpResult)));
        ADD_CONTROL((Control::New(pipelineState, PipelineState::Flushed)));
        RETURN_CONTROLS();
    }

    if (IsOneOf(if_id_operation, BIFormatOp::BEQ, BIFormatOp::BNE))
    {
        // TODO
        RETURN_CONTROLS();
    }

    uint32_t const id_ex_memRead = memory.GetRegister(ID_EX_MemRead);
    uint32_t const if_id_reg1    = (if_id_instr >> 26) & 0b11111;
    uint32_t const if_id_reg2    = (if_id_instr >> 21) & 0b11111;
    uint32_t const id_ex_reg2    = memory.GetRegister(ID_EX_Reg2);
    if (id_ex_memRead && (if_id_reg1 == id_ex_reg2 || if_id_reg2 == id_ex_reg2))
    {
        // load-use stall
        ADD_CONTROL((Control::New(nextPCType, NextPCType::NotMutated)));
        ADD_CONTROL((Control::New(pipelineState, PipelineState::Stalled)));
    }
    else
    {
        ADD_CONTROL((Control::New(nextPCType, NextPCType::AdvancedPC)));
        ADD_CONTROL((Control::New(pipelineState, PipelineState::Normal)));
    }

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
    SIGNAL(pipelineState);
}

DATAPATH_EXEC(InstructionFetch)
{
    DEFINE_DELTAS();

    uint32_t const pcValue = memory.GetRegister(PC);
    if (pcValue >= memory.GetTextSize())
    {
        // program is terminated
        ADD_DELTA((Delta::Register(IF_ID_NextPC, 0)));
        ADD_DELTA((Delta::Register(IF_ID_Instr, 0)));
        RETURN_DELTAS();
    }

    {
        uint32_t const instruction = memory.GetWord(Address::MakeFromWord(pcValue));
        uint32_t const newPCValue  = pcValue + 4;

        ADD_DELTA((Delta::Conditioned(PC, newPCValue, nextPCType, NextPCType::AdvancedPC)));

        ADD_DELTA((Delta::Conditioned(IF_ID_PC, pcValue, pipelineState, PipelineState::Normal)));
        ADD_DELTA(
            (Delta::Conditioned(IF_ID_NextPC, newPCValue, pipelineState, PipelineState::Normal)));
        ADD_DELTA(
            (Delta::Conditioned(IF_ID_Instr, instruction, pipelineState, PipelineState::Normal)));

        ADD_DELTA((Delta::Conditioned(IF_ID_Instr, instruction, 0, PipelineState::Flushed)));

        // Do not mutate when stalled

        ADD_DELTA((Delta::Conditioned(IF_ID_Instr, instruction, 0, PipelineState::Flushed3)));

        RETURN_DELTAS();
    }
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

    REGISTER_WRITE(ID_EX_RAWrite);
    REGISTER_WRITE(ID_EX_RAValue);

    REGISTER_WRITE(PC);

    SIGNAL(nextPCType);
    SIGNAL(pipelineState);
}

DATAPATH_EXEC(InstructionDecode)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(IF_ID_PC, ID_EX_PC);
    FORWARD_REGISTER(IF_ID_NextPC, ID_EX_NextPC);

    uint32_t const instruction = memory.GetRegister(IF_ID_Instr);
    ADD_DELTA((Delta::Conditioned(ID_EX_Instr, instruction, pipelineState, PipelineState::Normal)));
    ADD_DELTA((Delta::Conditioned(ID_EX_Instr, 0, pipelineState, PipelineState::Stalled)));
    ADD_DELTA(
        (Delta::Conditioned(ID_EX_Instr, instruction, pipelineState, PipelineState::Flushed)));
    ADD_DELTA((Delta::Conditioned(ID_EX_Instr, 0, pipelineState, PipelineState::Flushed3)));

    uint32_t const register1 = (instruction >> 21) & 0b11111;
    uint32_t const register2 = (instruction >> 16) & 0b11111;
    uint32_t const register3 = (instruction >> 11) & 0b11111;
    uint32_t const immediate = (instruction >> 0) & 0xFFFF;

    uint32_t const newPCValue     = memory.GetRegister(IF_ID_NextPC);
    uint32_t       register1Value = memory.GetRegister(register1);
    uint32_t       register2Value = memory.GetRegister(register2);

    uint32_t regWrite = 0, memWrite = 0, memRead = 0, raWrite = 0, raValue = 0;

    uint32_t const operation = (instruction >> 26) & 0b111111;
    uint32_t const function  = (instruction >> 0) & 0b111111;

    if (operation == 0)
    {
        if (IsOneOf(function, JRFormatFn::JR))
            ADD_DELTA((Delta::Conditioned(PC, register1Value, nextPCType, NextPCType::JumpResult)));
        else
            regWrite = 1;
    }
    else if (IsOneOf(operation, JFormatOp::J, JFormatOp::JAL))
    {
        uint32_t const target = ((instruction & 0x03FFFFFF) << 2) | (newPCValue & 0xF0000000);
        ADD_DELTA((Delta::Conditioned(PC, target, nextPCType, NextPCType::JumpResult)));

        if (IsOneOf(operation, JFormatOp::JAL))
        {
            raWrite = 1;
            raValue = newPCValue;
        }
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
    else if (IsOneOf(operation, BIFormatOp::BEQ, BIFormatOp::BNE))
    {
        uint32_t const target = newPCValue + SignExtend(immediate, 16) * 4;
        ADD_DELTA((Delta::Conditioned(PC, target, nextPCType, NextPCType::BranchResultID)));
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

    ADD_DELTA((Delta::Conditioned(ID_EX_RegWrite, regWrite, pipelineState, PipelineState::Normal)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemWrite, memWrite, pipelineState, PipelineState::Normal)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemRead, memRead, pipelineState, PipelineState::Normal)));

    ADD_DELTA((Delta::Conditioned(ID_EX_RegWrite, 0, pipelineState, PipelineState::Stalled)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemWrite, 0, pipelineState, PipelineState::Stalled)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemRead, 0, pipelineState, PipelineState::Stalled)));

    ADD_DELTA(
        (Delta::Conditioned(ID_EX_RegWrite, regWrite, pipelineState, PipelineState::Flushed)));
    ADD_DELTA(
        (Delta::Conditioned(ID_EX_MemWrite, memWrite, pipelineState, PipelineState::Flushed)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemRead, memRead, pipelineState, PipelineState::Flushed)));

    ADD_DELTA((Delta::Conditioned(ID_EX_RegWrite, 0, pipelineState, PipelineState::Flushed3)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemWrite, 0, pipelineState, PipelineState::Flushed3)));
    ADD_DELTA((Delta::Conditioned(ID_EX_MemRead, 0, pipelineState, PipelineState::Flushed3)));

    ADD_DELTA((Delta::Register(ID_EX_Reg1Value, register1Value)));
    ADD_DELTA((Delta::Register(ID_EX_Reg2Value, register2Value)));

    ADD_DELTA((Delta::Register(ID_EX_Imm, immediate)));
    ADD_DELTA((Delta::Register(ID_EX_Reg1, register1)));
    ADD_DELTA((Delta::Register(ID_EX_Reg2, register2)));
    ADD_DELTA((Delta::Register(ID_EX_Reg3, register3)));

    ADD_DELTA((Delta::Register(ID_EX_RAWrite, raWrite)));
    ADD_DELTA((Delta::Register(ID_EX_RAValue, raValue)));

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

    REGISTER_READ(ID_EX_RAWrite);
    REGISTER_READ(ID_EX_RAValue);

    REGISTER_WRITE(EX_MEM_PC);
    REGISTER_WRITE(EX_MEM_NextPC);
    REGISTER_WRITE(EX_MEM_Instr);

    REGISTER_READ_WRITE(EX_MEM_RegWrite);
    REGISTER_WRITE(EX_MEM_MemWrite);
    REGISTER_WRITE(EX_MEM_MemRead);
    REGISTER_WRITE(EX_MEM_Reg2Value);

    REGISTER_WRITE(EX_MEM_Reg2);

    REGISTER_WRITE(EX_MEM_RAWrite);
    REGISTER_WRITE(EX_MEM_RAValue);

    REGISTER_READ_WRITE(EX_MEM_ALUResult);
    REGISTER_READ_WRITE(EX_MEM_DestReg);

    REGISTER_READ(MEM_WB_RegWrite);
    REGISTER_READ(MEM_WB_MemRead);
    REGISTER_READ(MEM_WB_DestReg);
    REGISTER_READ(MEM_WB_ReadData);

    SIGNAL(pipelineState);
}

DATAPATH_EXEC(Execution)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(ID_EX_PC, EX_MEM_PC);
    FORWARD_REGISTER(ID_EX_NextPC, EX_MEM_NextPC);

    uint32_t const instruction = memory.GetRegister(ID_EX_Instr);
    uint32_t const regWrite    = memory.GetRegister(ID_EX_RegWrite);
    uint32_t const memWrite    = memory.GetRegister(ID_EX_MemWrite);
    uint32_t const memRead     = memory.GetRegister(ID_EX_MemRead);

    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_Instr, instruction, pipelineState, PipelineState::Normal)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_RegWrite, regWrite, pipelineState, PipelineState::Normal)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_MemWrite, memWrite, pipelineState, PipelineState::Normal)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_MemRead, memRead, pipelineState, PipelineState::Normal)));

    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_Instr, instruction, pipelineState, PipelineState::Stalled)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_RegWrite, regWrite, pipelineState, PipelineState::Stalled)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_MemWrite, memWrite, pipelineState, PipelineState::Stalled)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_MemRead, memRead, pipelineState, PipelineState::Stalled)));

    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_Instr, instruction, pipelineState, PipelineState::Flushed)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_RegWrite, regWrite, pipelineState, PipelineState::Flushed)));
    ADD_DELTA(
        (Delta::Conditioned(EX_MEM_MemWrite, memWrite, pipelineState, PipelineState::Flushed)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_MemRead, memRead, pipelineState, PipelineState::Flushed)));

    ADD_DELTA((Delta::Conditioned(EX_MEM_Instr, 0, pipelineState, PipelineState::Flushed3)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_RegWrite, 0, pipelineState, PipelineState::Flushed3)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_MemWrite, 0, pipelineState, PipelineState::Flushed3)));
    ADD_DELTA((Delta::Conditioned(EX_MEM_MemRead, 0, pipelineState, PipelineState::Flushed3)));

    FORWARD_REGISTER(ID_EX_Reg2Value, EX_MEM_Reg2Value);

    FORWARD_REGISTER(ID_EX_Reg2, EX_MEM_Reg2);

    FORWARD_REGISTER(ID_EX_RAWrite, EX_MEM_RAWrite);
    FORWARD_REGISTER(ID_EX_RAValue, EX_MEM_RAValue);

    uint32_t const operation = (instruction >> 26) & 0b111111;
    uint32_t const register1 = memory.GetRegister(ID_EX_Reg1);
    uint32_t const register2 = memory.GetRegister(ID_EX_Reg2);
    uint32_t const register3 = memory.GetRegister(ID_EX_Reg3);

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
    else if (IsOneOf(operation, BIFormatOp::BEQ, BIFormatOp::BNE))
    {
        destinationValue = ((source1Value == source2Value) == IsOneOf(operation, BIFormatOp::BEQ));
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
    // Registers to read
    REGISTER_READ(EX_MEM_PC);
    REGISTER_READ(EX_MEM_NextPC);
    REGISTER_READ(EX_MEM_Instr);

    REGISTER_READ(EX_MEM_RegWrite);
    REGISTER_READ(EX_MEM_MemWrite);
    REGISTER_READ(EX_MEM_MemRead);
    REGISTER_READ(EX_MEM_Reg2Value);

    REGISTER_READ(EX_MEM_Reg2);

    REGISTER_READ(EX_MEM_ALUResult);
    REGISTER_READ(EX_MEM_DestReg);

    REGISTER_READ(EX_MEM_RAWrite);
    REGISTER_READ(EX_MEM_RAValue);

    // Registers to forward
    REGISTER_WRITE(MEM_WB_PC);
    REGISTER_WRITE(MEM_WB_Instr);

    REGISTER_READ_WRITE(MEM_WB_RegWrite);
    REGISTER_READ_WRITE(MEM_WB_MemRead);

    REGISTER_WRITE(MEM_WB_ALUResult);
    REGISTER_READ_WRITE(MEM_WB_DestReg);

    REGISTER_WRITE(MEM_WB_RAWrite);
    REGISTER_WRITE(MEM_WB_RAValue);

    // Registers to write
    REGISTER_READ_WRITE(MEM_WB_ReadData);

    REGISTER_WRITE(PC);

    // Signals
    SIGNAL(nextPCType);
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

    FORWARD_REGISTER(EX_MEM_RAWrite, MEM_WB_RAWrite);
    FORWARD_REGISTER(EX_MEM_RAValue, MEM_WB_RAValue);

    uint32_t const newPCValue  = memory.GetRegister(EX_MEM_NextPC);
    uint32_t const instruction = memory.GetRegister(EX_MEM_Instr);
    uint32_t const memoryRead  = memory.GetRegister(EX_MEM_MemRead);
    uint32_t const memoryWrite = memory.GetRegister(EX_MEM_MemWrite);
    uint32_t const operation   = (instruction >> 26) & 0b111111;
    uint32_t const immediate   = (instruction >> 0) & 0xFFFF;

    if (IsOneOf(operation, BIFormatOp::BEQ, BIFormatOp::BNE))
    {
        uint32_t const target = newPCValue + SignExtend(immediate, 16) * 4;
        ADD_DELTA((Delta::Conditioned(PC, target, nextPCType, NextPCType::BranchResultMem)));
    }

    uint32_t      readData = 0;
    Address const address  = Address::MakeFromWord(memory.GetRegister(EX_MEM_ALUResult));
    if (memoryRead)
    {
        if ((operation & 0b11) == 0b11)
            readData = memory.GetWord(address);
        else
            readData = SignExtend(memory.GetByte(address), 8);
    }
    if (memoryWrite)
    {
        uint32_t       writeData      = memory.GetRegister(EX_MEM_Reg2Value);
        uint32_t const mem_wb_destReg = memory.GetRegister(MEM_WB_DestReg);
        if (memory.GetRegister(MEM_WB_RegWrite) && memory.GetRegister(MEM_WB_MemRead)
            && mem_wb_destReg)
        {
            if (mem_wb_destReg == EX_MEM_Reg2)
                writeData = memory.GetRegister(MEM_WB_ReadData);
        }

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
    TICK();

    REGISTER_READ(MEM_WB_PC);
    REGISTER_READ(MEM_WB_Instr);

    REGISTER_READ(MEM_WB_RegWrite);
    REGISTER_READ(MEM_WB_MemRead);

    REGISTER_READ(MEM_WB_ALUResult);
    REGISTER_READ(MEM_WB_DestReg);

    REGISTER_READ(MEM_WB_ReadData);

    REGISTER_READ(MEM_WB_RAWrite);
    REGISTER_READ(MEM_WB_RAValue);

    REGISTER_WRITE(WB_PC);
    REGISTER_WRITE(WB_Instr);

    REGISTER_WRITE(RA);
}

DATAPATH_EXEC(WriteBack)
{
    DEFINE_DELTAS();

    FORWARD_REGISTER(MEM_WB_PC, WB_PC);
    FORWARD_REGISTER(MEM_WB_Instr, WB_Instr);

    uint32_t const instruction = memory.GetRegister(MEM_WB_Instr);
    uint32_t const regWrite    = memory.GetRegister(MEM_WB_RegWrite);
    uint32_t const memoryRead  = memory.GetRegister(MEM_WB_MemRead);
    uint32_t const aluResult   = memory.GetRegister(MEM_WB_ALUResult);
    uint32_t const destination = memory.GetRegister(MEM_WB_DestReg);
    uint32_t const readData    = memory.GetRegister(MEM_WB_ReadData);
    uint32_t const raWrite     = memory.GetRegister(MEM_WB_RAWrite);
    uint32_t const raValue     = memory.GetRegister(MEM_WB_RAValue);

    if (raWrite)
    {
        ADD_DELTA((Delta::Register(RA, raValue)));
    }

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