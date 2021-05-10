// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_NAMED_ENTRY_MAP_HH
#define PIP_MIPS_EMU_NAMED_ENTRY_MAP_HH

#include <pip-mips-emu/Common.hh>
#include <pip-mips-emu/Memory.hh>

#include <string>
#include <unordered_map>
#include <vector>

enum class NamedEntryUsage : uint8_t
{
    Read      = 0b01,
    Write     = 0b10,
    ReadWrite = Read | Write,
};

/// <summary>
/// <c>NamedEntryMap</c> determines indices for each named entries.
/// </summary>
class NamedEntryMap
{
  protected:
    struct Entry
    {
        std::vector<uint32_t*> _readBy;
        std::vector<uint32_t*> _writtenBy;
    };

  protected:
    std::unordered_map<std::string, Entry> _entries;

  public:
    /// <summary>
    /// Adds a named entry.
    /// </summary>
    /// <param name="entryName">The name of the entry</param>
    /// <param name="ptr">A pointer to a variable which will be set to the index of the
    /// entry</param> <param name="usage">Identifies how the reigster is to be read or
    /// written</param>
    void AddEntry(std::string const& entryName, uint32_t* ptr, NamedEntryUsage usage);

    /// <summary>
    /// Calculates indices for each entry and set the indices to the pointers retrieved by
    /// <c>AddEntry</c> in advance.
    /// </summary>
    /// <param name="offset">First index assigned to a named entry</param>
    /// <param name="entryType">Used when an error occured</param>
    /// <returns>A collection mapping the names to the indices</returns>
    /// <exception cref="std::runtime_error">Thrown when there is a entry that is written or read
    /// by no one.</exception>
    std::unordered_map<std::string, uint32_t> Build(std::string const& entryType,
                                                    uint32_t           offset) const;
};

/// <summary>
/// <c>RegisterMap</c> determines indices for each named registers.
/// </summary>
class RegisterMap : private NamedEntryMap
{
  public:
    void AddEntry(std::string const& entryName, uint32_t* ptr, NamedEntryUsage usage);

    inline std::unordered_map<std::string, uint32_t> Build() const
    {
        return NamedEntryMap::Build("Register", Memory::PC + 1);
    }
};

/// <summary>
/// <c>SignalMap</c> determines indices for each signals.
/// </summary>
class SignalMap : private NamedEntryMap
{
  public:
    using NamedEntryMap::AddEntry;

  public:
    std::unordered_map<std::string, uint32_t> Build() const;
};

#endif