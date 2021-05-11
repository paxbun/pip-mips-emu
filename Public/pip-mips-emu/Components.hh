// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_COMPONENTS_HH
#define PIP_MIPS_EMU_COMPONENTS_HH

#include <pip-mips-emu/NamedEntryMap.hh>

#include <memory>

/// <summary>
/// Represents a change in memory.
/// </summary>
struct Delta
{
    /// <summary>
    /// Type of the change
    /// </summary>
    enum class Type : uint16_t
    {
        /// <summary>
        /// Register value change
        /// </summary>
        Register,

        /// <summary>
        /// Conditioned register value change controlled by a control signal
        /// </summary>
        Conditioned,

        /// <summary>
        ///
        /// </summary>
        MemoryWord,

        /// <summary>
        ///
        /// </summary>
        MemoryByte,
    };

    /// <summary>
    /// A register index or an address
    /// </summary>
    uint32_t target;

    /// <summary>
    /// The value to assign
    /// </summary>
    uint32_t value;

    /// <summary>
    /// A signal index
    /// </summary>
    uint32_t signal;

    /// <summary>
    /// This delta is applied if and only if the signal has this value
    /// </summary>
    uint16_t condition;

    /// <summary>
    /// Type of the change
    /// </summary>
    Type type;

    static Delta Register(uint32_t idx, uint32_t value)
    {
        return Delta { idx, value, 0, 0, Type::Register };
    }

    static Delta Conditioned(uint32_t idx, uint32_t value, uint32_t signal, uint16_t condition)
    {
        return Delta { idx, value, signal, condition, Type::Conditioned };
    }

    static Delta MemoryWord(uint32_t address, uint32_t value)
    {
        return Delta { address, value, 0, 0, Type::MemoryWord };
    }

    static Delta MemoryByte(uint32_t address, uint8_t value)
    {
        return Delta { address, static_cast<uint32_t>(value), 0, 0, Type::MemoryByte };
    }
};

enum class TickTockType
{
    NoPreference = 0,
    Tick,
    Tock,
};

/// <summary>
/// Represents a component in the datapath.
/// </summary>
class Datapath
{
  public:
    /// <summary>
    /// Implementers must create named registers and signals in this function. The indices will be
    /// assigned automatically when <c>Datapath::Initialize</c> is called for all <c>Datapath</c>
    /// instances. Implementers also can determine which half of cycle this component will be
    /// executed at.
    /// </summary>
    /// <param name="regMap">Register collections</param>
    /// <param name="sigMap">Signal collections</param>
    /// <param name="tickTock">Half of cycle at which this component will be executed</param>
    virtual void Initialize(RegisterMap& regMap, SignalMap& sigMap, TickTockType& tickTock) = 0;

    /// <summary>
    /// Generates deltas.
    /// </summary>
    /// <param name="memory">The current state of the device</param>
    /// <returns>List of deltas</returns>
    virtual std::vector<Delta> Execute(Memory const& memory) const = 0;
};

using DatapathPtr = std::unique_ptr<Datapath>;

#define DATAPATH_INIT()                                                                            \
    virtual void Initialize(RegisterMap& regMap, SignalMap& sigMap, TickTockType& tickTock) override

#define REGISTER_USAGE(registerName, registerUsage)                                                \
    regMap.AddEntry(#registerName, &registerName, registerUsage)

#define REGISTER_READ(registerName) REGISTER_USAGE(registerName, NamedEntryUsage::Read)

#define REGISTER_WRITE(registerName) REGISTER_USAGE(registerName, NamedEntryUsage::Write)

#define REGISTER_READ_WRITE(registerName) REGISTER_USAGE(registerName, NamedEntryUsage::ReadWrite)

#define SIGNAL(signalName) sigMap.AddEntry(#signalName, &signalName, NamedEntryUsage::Read)

#define TICK() tickTock = TickTockType::Tick

#define TOCK() tickTock = TickTockType::Tock

/// <summary>
/// Represents a control signal.
/// </summary>
struct Control
{
    uint32_t signal;
    uint16_t value;
};

/// <summary>
/// Represents a component in the control unit. Generates control signals.
/// </summary>
class Controller
{
  public:
    /// <summary>
    /// Implementers must create named registers and signals in this function. The indices will be
    /// assigned automatically when <c>Controller::Initialize</c> is called for all
    /// <c>Controller</c> instances.
    /// </summary>
    virtual void Initialize(RegisterMap& regMap, SignalMap& sigMap) = 0;

    /// <summary>
    /// Generates control signals. Implementers
    /// </summary>
    /// <param name="memory">The current state of the device</param>
    /// <returns>List of control signals</returns>
    virtual std::vector<Control> Execute(Memory const& memory) const = 0;
};

using ControllerPtr = std::unique_ptr<Controller>;

#define CONTROLLER_INIT() virtual void Initialize(RegisterMap& regMap, SignalMap& sigMap) override

#define MAKE_SIGNAL(signalName) sigMap.AddEntry(#signalName, &signalName, NamedEntryUsage::Write)

#endif