// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_EMULATOR_HH
#define PIP_MIPS_EMU_EMULATOR_HH

#include <pip-mips-emu/Components.hh>
#include <pip-mips-emu/Memory.hh>
#include <pip-mips-emu/NamedEntryMap.hh>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

/// <summary>
/// Indicates the result of each clock
/// </summary>
enum class TickTockResult
{
    UnknownError = -1,
    Success      = 0,

    /// <summary>
    /// The program is already terminated.
    /// </summary>
    AlreadyTerminated,

    /// <summary>
    /// The current instruction references invalid memory.
    /// </summary>
    MemoryOutOfRange,
};

/// <summary>
/// Manages datapath and control unit components.
/// </summary>
class Emulator
{
    friend class EmulatorBuilder;

  private:
    std::vector<DatapathPtr>   _tickDatapaths, _tockDatapaths, _datapaths;
    std::vector<ControllerPtr> _controllers;
    HandlerPtr                 _handler;
    // std::unordered_map<std::string, uint32_t> _namedRegisters;
    // std::unordered_map<std::string, uint32_t> _namedSignals;
    std::vector<uint16_t> _controls;

  private:
    Emulator(std::vector<std::pair<DatapathPtr, TickTockType>>&& datapaths,
             std::vector<ControllerPtr>&&                        controllers,
             HandlerPtr&&                                        handler,
             std::unordered_map<std::string, uint32_t>&&         namedRegisters,
             std::unordered_map<std::string, uint32_t>&&         namedSignals);

  public:
    Emulator(Emulator&&) = default;
    Emulator& operator=(Emulator&&) = default;
    ~Emulator()                     = default;

  public:
    /// <summary>
    /// Runs one instruction and mutate the given memory. If the result is not
    /// <c>TickTockResult::Success</c>, the memory is not mutated.
    /// </summary>
    /// <param name="memory">The memory to mutate</param>
    /// <returns>Execution result</returns>
    TickTockResult TickTock(Memory& memory) noexcept;

    /// <summary>
    /// Returns <c>true</c> if the program is terminated.
    /// </summary>
    bool IsTerminated(Memory const& memory) const noexcept;
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
    HandlerPtr                                        _handler;

  public:
    EmulatorBuilder() {}

  public:
    /// <summary>
    /// Adds a datapath component.
    /// </summary>
    /// <typeparam name="T">Type of the component</typeparam>
    template <typename T, std::enable_if_t<std::is_base_of_v<Datapath, T>, int> = 0>
    EmulatorBuilder& AddDatapath()
    {
        AddDatapath(std::make_unique<T>());
        return *this;
    }

    /// <summary>
    /// Adds a control unit component.
    /// </summary>
    /// <typeparam name="T">Type of the component</typeparam>
    template <typename T, std::enable_if_t<std::is_base_of_v<Controller, T>, int> = 0>
    EmulatorBuilder& AddController()
    {
        AddController(std::make_unique<T>());
        return *this;
    }

    /// <summary>
    /// Adds a handler.
    /// </summary>
    /// <typeparam name="T">Type of the handler</typeparam>
    template <typename T, std::enable_if_t<std::is_base_of_v<Handler, T>, int> = 0>
    EmulatorBuilder& AddHandler()
    {
        AddHandler(std::make_unique<T>());
        return *this;
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

    /// <summary>
    /// Adds a handler.
    /// </summary>
    /// <typeparam name="T">Pointer to the handler</typeparam>
    void AddHandler(HandlerPtr&& handler);
};

#endif