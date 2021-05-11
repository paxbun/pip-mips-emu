// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Emulator.hh>

namespace
{

std::vector<DatapathPtr>
FilterDatapath(std::vector<std::pair<DatapathPtr, TickTockType>>&& datapaths, TickTockType target)
{
    std::vector<DatapathPtr> rtn;
    for (auto&& [datapath, tickTock] : datapaths)
    {
        if (tickTock == target)
            rtn.push_back(std::move(datapath));
    }
    return rtn;
}

void ApplyDeltas(Memory&                                memory,
                 std::vector<uint16_t> const&           controls,
                 std::vector<std::vector<Delta>> const& deltaLists)
{
    for (auto const& deltaList : deltaLists)
    {
        for (auto const& delta : deltaList)
        {
            switch (delta.type)
            {
            case Delta::Type::Register:
            {
                memory.SetRegister(delta.target, delta.value);
                break;
            }
            case Delta::Type::Conditioned:
            {
                if (controls[delta.signal] == delta.condition)
                    memory.SetRegister(delta.target, delta.value);
                break;
            }
            case Delta::Type::MemoryWord:
            {
                memory.SetWord(Address::MakeFromWord(delta.target), delta.value);
                break;
            }
            case Delta::Type::MemoryByte:
            {
                memory.SetByte(Address::MakeFromWord(delta.target),
                               static_cast<uint8_t>(delta.value));
                break;
            }
            }
        }
    }
}

}

Emulator::Emulator(std::vector<std::pair<DatapathPtr, TickTockType>>&& datapaths,
                   std::vector<ControllerPtr>&&                        controllers,
                   std::unordered_map<std::string, uint32_t>&&         namedRegisters,
                   std::unordered_map<std::string, uint32_t>&&         namedSignals) :
    _tickDatapaths(FilterDatapath(std::move(datapaths), TickTockType::Tick)),
    _tockDatapaths(FilterDatapath(std::move(datapaths), TickTockType::Tock)),
    _datapaths(FilterDatapath(std::move(datapaths), TickTockType::NoPreference)),
    _controllers { std::move(controllers) },
    // _namedRegisters { std::move(namedRegisters) },
    // _namedSignals { std::move(namedSignals) },
    _controls(namedSignals.size(), 0)
{}

void Emulator::TickTock(Memory& memory)
{
    for (auto const& controller : _controllers)
    {
        for (auto const& control : controller->Execute(memory))
            _controls[control.signal] = control.value;
    }

    std::vector<std::vector<Delta>> tickDeltas;
    for (auto const& datapath : _datapaths) tickDeltas.push_back(datapath->Execute(memory));
    for (auto const& datapath : _tickDatapaths) tickDeltas.push_back(datapath->Execute(memory));

    ApplyDeltas(memory, _controls, tickDeltas);

    std::vector<std::vector<Delta>> tockDeltas;
    for (auto const& datapath : _tockDatapaths) tockDeltas.push_back(datapath->Execute(memory));

    ApplyDeltas(memory, _controls, tockDeltas);
}

void EmulatorBuilder::AddDatapath(DatapathPtr&& component)
{
    TickTockType tickTock = TickTockType::NoPreference;
    component->Initialize(_regMap, _sigMap, tickTock);
    _datapaths.push_back(std::make_pair(std::move(component), tickTock));
}

void EmulatorBuilder::AddController(ControllerPtr&& component)
{
    component->Initialize(_regMap, _sigMap);
    _controllers.push_back(std::move(component));
}

std::pair<Emulator, Memory> EmulatorBuilder::Build(std::vector<uint8_t>&& text,
                                                   std::vector<uint8_t>&& data)
{
    auto registers = _regMap.Build();
    auto signals   = _sigMap.Build();

    Emulator emulator {
        std::move(_datapaths),
        std::move(_controllers),
        std::move(registers),
        std::move(signals),
    };

    Memory memory {
        static_cast<uint32_t>(registers.size()),
        std::move(text),
        std::move(data),
    };

    return std::make_pair(std::move(emulator), std::move(memory));
}
