// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Common.hh>
#include <pip-mips-emu/Memory.hh>

#include <algorithm>

bool Address::Parse(char const* begin, char const* end, Address& out) noexcept
{
    uint32_t word;
    if (!ParseWord(begin, end, word))
        return false;

    out = MakeFromWord(word);
    return true;
}

std::vector<uint8_t>& Memory::GetSegmentByBase(Address::BaseType base)
{
    if (base == Address::BaseType::Text)
        return _text;
    else if (base == Address::BaseType::Data)
        return _data;
    else
        throw std::invalid_argument { "Invalid address" };
}

std::vector<uint8_t> const& Memory::GetSegmentByBase(Address::BaseType base) const
{
    if (base == Address::BaseType::Text)
        return _text;
    else if (base == Address::BaseType::Data)
        return _data;
    else
        throw std::invalid_argument { "Invalid address" };
}

Memory::Memory(uint32_t numAdditionalRegs, uint32_t textSize, uint32_t dataSize) :
    Memory {
        numAdditionalRegs,
        std::vector<uint8_t>(static_cast<size_t>(textSize), 0),
        std::vector<uint8_t>(static_cast<size_t>(dataSize), 0),
    }
{}

Memory::Memory(uint32_t               numAdditionalRegs,
               std::vector<uint8_t>&& text,
               std::vector<uint8_t>&& data) :
    _registers(static_cast<size_t>(numAdditionalRegs) + 33, 0),
    _numRegisters { numAdditionalRegs + 33 },
    _text(std::move(text)),
    _textSize { static_cast<uint32_t>(_text.size()) },
    _data(std::move(data)),
    _dataSize { static_cast<uint32_t>(_data.size()) }
{
    _registers[PC] = Address::MakeText(0);
}

bool Memory::IsTerminated() const noexcept
{
    return GetRegister(PC) >= static_cast<uint32_t>(Address::MakeText(_textSize));
}

void Memory::AdvancePC() noexcept
{
    SetRegister(PC, GetRegister(PC) + 4);
}

void Memory::Load(Address::BaseType base, std::vector<uint8_t> const& data) noexcept
{
    auto& segment = GetSegmentByBase(base);
    std::copy_n(data.begin(), std::min(data.size(), segment.size()), segment.begin());
}

uint32_t Memory::GetRegister(uint32_t registerIdx) const
{
    return _registers.at(registerIdx);
}

void Memory::SetRegister(uint32_t registerIdx, uint32_t newValue)
{
    if (registerIdx != Zero)
        _registers.at(registerIdx) = newValue;
}

uint8_t Memory::GetByte(Address address) const noexcept
{
    auto& segment = GetSegmentByBase(address.base);
    if (static_cast<size_t>(address.offset) < segment.size())
        return segment[address.offset];
    else
        return 0;
}

void Memory::SetByte(Address address, uint8_t byte)
{
    GetSegmentByBase(address.base).at(address.offset) = byte;
}

uint32_t Memory::GetWord(Address address) const noexcept
{
    auto& segment = GetSegmentByBase(address.base);
    if (static_cast<size_t>(address.offset) + 3 >= segment.size())
    {
        uint32_t rtn = 0;

        uint8_t shift = 24;
        while (address.offset < segment.size())
        {
            rtn |= static_cast<uint32_t>(segment[address.offset]) << shift;
            address.offset += 1;
            shift -= 8;
        }

        return rtn;
    }
    else
    {
        uint32_t       rtn = 0;
        uint8_t const* ptr = std::addressof(segment[address.offset]);

        rtn |= static_cast<uint32_t>(ptr[0]) << 24;
        rtn |= static_cast<uint32_t>(ptr[1]) << 16;
        rtn |= static_cast<uint32_t>(ptr[2]) << 8;
        rtn |= static_cast<uint32_t>(ptr[3]) << 0;

        return rtn;
    }
}

void Memory::SetWord(Address address, uint32_t word)
{
    auto& segment = GetSegmentByBase(address.base);
    if (static_cast<size_t>(address.offset) + 3 > segment.size())
        throw std::out_of_range { "address out of range" };

    uint8_t* ptr = std::addressof(segment[address.offset]);

    ptr[0] = static_cast<uint8_t>(word >> 24 & 0xFF);
    ptr[1] = static_cast<uint8_t>(word >> 16 & 0xFF);
    ptr[2] = static_cast<uint8_t>(word >> 8 & 0xFF);
    ptr[3] = static_cast<uint8_t>(word >> 0 & 0xFF);
}