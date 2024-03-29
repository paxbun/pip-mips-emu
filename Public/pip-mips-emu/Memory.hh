// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_MEMORY_HH
#define PIP_MIPS_EMU_MEMORY_HH

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

/// <summary>
/// Represents an address in the memory.
/// </summary>
struct Address
{
    enum class BaseType : uint32_t
    {
        Text = 0x400000,
        Data = 0x10000000
    };

    constexpr static Address MakeText(uint32_t offset) noexcept
    {
        return Address { Address::BaseType::Text, offset };
    }

    constexpr static Address MakeData(uint32_t offset) noexcept
    {
        return Address { Address::BaseType::Data, offset };
    }

    constexpr static Address MakeFromWord(uint32_t address) noexcept
    {
        if (static_cast<uint32_t>(Address::BaseType::Data) <= address)
            return Address::MakeData(address - static_cast<uint32_t>(Address::BaseType::Data));
        else
            return Address::MakeText(address - static_cast<uint32_t>(Address::BaseType::Text));
    }

    static bool Parse(char const* begin, char const* end, Address& out) noexcept;

    BaseType base;
    uint32_t offset;

    constexpr void MoveToNext() noexcept
    {
        offset += 4;
    }

    constexpr operator uint32_t() const noexcept
    {
        return static_cast<uint32_t>(base) + offset;
    }

    friend std::ostream& operator<<(std::ostream& os, Address address)
    {
        std::ios_base::fmtflags flags = os.flags();
        os << "0x" << std::hex << static_cast<uint32_t>(address);
        os.flags(flags);
        return os;
    }
};

/// <summary>
/// Represents a range in the memory. Note that <c>end</c> is inclusive.
/// </summary>
struct Range
{
    Address begin;
    Address end;
};

/// <summary>
/// Represents a state of the device at the specific time point.
/// </summary>
class Memory
{
  public:
    constexpr static uint32_t PC   = 32;
    constexpr static uint32_t RA   = 31;
    constexpr static uint32_t Zero = 0;

  private:
    std::vector<uint32_t> _registers;
    std::vector<uint8_t>  _text;
    std::vector<uint8_t>  _data;
    uint32_t              _numRegisters, _textSize, _dataSize;

  public:
    uint32_t GetNumRegisters() const noexcept
    {
        return _numRegisters;
    }

    uint32_t GetTextSize() const noexcept
    {
        return _textSize;
    }

    uint32_t GetDataSize() const noexcept
    {
        return _dataSize;
    }

  private:
    std::vector<uint8_t>&       GetSegmentByBase(Address::BaseType base);
    std::vector<uint8_t> const& GetSegmentByBase(Address::BaseType base) const;

  public:
    Memory(uint32_t numAdditionalRegs, uint32_t textSize, uint32_t dataSize);
    Memory(uint32_t numAdditionalRegs, std::vector<uint8_t>&& text, std::vector<uint8_t>&& data);
    Memory(Memory const&)     = default;
    Memory(Memory&&) noexcept = default;
    Memory& operator=(Memory const&) = default;
    Memory& operator=(Memory&&) noexcept = default;

  public:
    /// <summary>
    /// Loads data to the given segment.
    /// </summary>
    void Load(Address::BaseType base, std::vector<uint8_t> const& data) noexcept;

    /// <summary>
    /// Returns the value of the given register. Note that R32 is PC.
    /// </summary>
    uint32_t GetRegister(uint32_t registerIdx) const;

    /// <summary>
    /// Assign the given word to the given reigster. Note that R32 is PC.
    /// </summary>
    void SetRegister(uint32_t registerIdx, uint32_t newValue);

    /// <summary>
    /// Returns the byte at the given address.
    /// </summary>
    uint8_t GetByte(Address address) const noexcept;

    /// <summary>
    /// Assign the given byte to the given memory location.
    /// </summary>
    void SetByte(Address address, uint8_t byte);

    /// <summary>
    /// Returns the word at the given address in big endian.
    /// </summary>
    uint32_t GetWord(Address address) const noexcept;

    /// <summary>
    /// Assign the given word to the given memory location. Note that the word is interpreted in big
    /// endian format.
    /// </summary>
    void SetWord(Address address, uint32_t word);
};

#endif