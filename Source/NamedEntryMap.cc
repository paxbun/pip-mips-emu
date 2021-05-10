// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Memory.hh>
#include <pip-mips-emu/NamedEntryMap.hh>

#include <exception>

void NamedEntryMap::AddEntry(std::string const& entryName, uint32_t* ptr, NamedEntryUsage usage)
{
    auto it = _entries.find(entryName);
    if (it == _entries.end())
        it = _entries.insert(std::make_pair(entryName, Entry {})).first;

    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(NamedEntryUsage::Read))
        it->second._readBy.push_back(ptr);

    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(NamedEntryUsage::Write))
        it->second._writtenBy.push_back(ptr);
}

std::unordered_map<std::string, uint32_t> NamedEntryMap::Build(std::string const& entryType,
                                                               uint32_t           offset) const
{
    std::unordered_map<std::string, uint32_t> rtn;
    rtn.reserve(_entries.size());

    uint32_t idx = offset;
    for (auto& [name, entry] : _entries)
    {
        rtn.insert(std::make_pair(name, idx));

        if (entry._readBy.empty())
        {
            std::string message = entryType + " '" + name + "' is not read";
            throw std::runtime_error { message };
        }

        if (entry._writtenBy.empty())
        {
            std::string message = entryType + " '" + name + "' is not written";
            throw std::runtime_error { message };
        }

        for (auto ptr : entry._readBy) *ptr = idx;

        for (auto ptr : entry._writtenBy) *ptr = idx;

        ++idx;
    }

    return rtn;
}

void RegisterMap::AddEntry(std::string const& entryName, uint32_t* ptr, NamedEntryUsage usage)
{
    if (entryName == "PC")
    {
        *ptr = Memory::PC;
        return;
    }

    if (entryName == "RA")
    {
        *ptr = Memory::RA;
        return;
    }

    if (entryName == "Zero")
    {
        *ptr = Memory::Zero;
        return;
    }

    NamedEntryMap::AddEntry(entryName, ptr, usage);
}

std::unordered_map<std::string, uint32_t> SignalMap::Build() const
{
    for (auto& [name, entry] : _entries)
    {
        if (entry._writtenBy.size() > 1)
        {
            std::string message = "Signal '" + name + "' is being written by multiple controllers";
            throw std::runtime_error { message };
        }
    }

    return NamedEntryMap::Build("Signal", 0);
}
