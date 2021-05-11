// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_EMULATOR_HH
#define PIP_MIPS_EMU_EMULATOR_HH

#include <pip-mips-emu/Components.hh>
#include <pip-mips-emu/Memory.hh>
#include <pip-mips-emu/NamedEntryMap.hh>

#include <memory>
#include <type_traits>
#include <utility>

/// <summary>
/// Manages datapath and control unit components.
/// </summary>
class Emulator
{
    friend class EmulatorBuilder;

  private:
    std::vector<DatapathPtr>   _tickDatapaths, _tockDatapaths, _datapaths;
    std::vector<ControllerPtr> _controllers;
    // std::unordered_map<std::string, uint32_t> _namedRegisters;
    // std::unordered_map<std::string, uint32_t> _namedSignals;
    std::vector<uint16_t> _controls;

  private:
    Emulator(std::vector<std::pair<DatapathPtr, TickTockType>>&& datapaths,
             std::vector<ControllerPtr>&&                        controllers,
             std::unordered_map<std::string, uint32_t>&&         namedRegisters,
             std::unordered_map<std::string, uint32_t>&&         namedSignals);

  public:
    Emulator(Emulator&&) = default;
    Emulator& operator=(Emulator&&) = default;
    ~Emulator()                     = default;

  public:
    void TickTock(Memory& memory);
};

/// <summary>
/// Implements builder pattern for <c>Emulator</c>.
/// </summary>
class EmulatorBuilder
{
  private:
    std::vector<std::pair<DatapathPtr, TickTockType>> _datapaths;
    std::vector<ControllerPtr>                        _controllers;
    RegisterMap                                       _regMap;
    SignalMap                                         _sigMap;

  public:
    EmulatorBuilder() {}

  public:
    /// <summary>
    /// Adds a datapath component.
    /// </summary>
    /// <typeparam name="T">Type of the component</typeparam>
    template <typename T, std::enable_if_t<std::is_base_of_v<Datapath, T>, int> = 0>
    void AddDatapath()
    {
        AddDatapath(std::make_unique<T>());
    }

    /// <summary>
    /// Adds a control unit component.
    /// </summary>
    /// <typeparam name="T">Type of the component</typeparam>
    template <typename T, std::enable_if_t<std::is_base_of_v<Controller, T>, int> = 0>
    void AddController()
    {
        AddController(std::make_unique<T>());
    }

    /// <summary>
    /// Validates register and signal names.
    /// </summary>
    /// <param name="text">A byte list which will be loaded to the text segment</param>
    /// <param name="data">A byte list which will be loaded to the data segment</param>
    /// <returns>A pair of an <c>Emulator</c> instance and a <c>Memory</c> instance</returns>
    std::pair<Emulator, Memory> Build(std::vector<uint8_t>&& text, std::vector<uint8_t>&& data);

  private:
    /// <summary>
    /// Adds a datapath component.
    /// </summary>
    /// <param name="component">Pointer to the component</param>
    void AddDatapath(DatapathPtr&& component);

    /// <summary>
    /// Adds a control unit component.
    /// </summary>
    /// <param name="control">Pointer to the component</param>
    void AddController(ControllerPtr&& component);
};

#endif