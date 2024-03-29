// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <pip-mips-emu/Memory.hh>

TEST(MemoryTest, Init)
{
    Memory memory { 15, 7, 9 };

    ASSERT_EQ(memory.GetNumRegisters(), 15 + 33);
    for (uint8_t i = 0; i < 15 + 33; ++i)
    {
        if (i != 32)
            ASSERT_EQ(memory.GetRegister(i), 0);
    }
    ASSERT_EQ(memory.GetRegister(32), Address::MakeText(0));

    ASSERT_EQ(memory.GetTextSize(), 7);
    for (uint32_t i = 0; i < 7; ++i) ASSERT_EQ(memory.GetByte(Address::MakeText(i)), 0);

    ASSERT_EQ(memory.GetDataSize(), 9);
    for (uint32_t i = 0; i < 9; ++i) ASSERT_EQ(memory.GetByte(Address::MakeData(i)), 0);
}

TEST(MemoryTest, Load)
{
    Memory memory { 0, 5, 10 };

    std::vector<uint8_t> text { 1, 2, 3, 4, 5 };
    std::vector<uint8_t> data { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };

    memory.Load(Address::BaseType::Text, text);
    memory.Load(Address::BaseType::Data, data);

    ASSERT_EQ(memory.GetWord(Address::MakeText(1)), 0x02030405);
    ASSERT_EQ(memory.GetWord(Address::MakeData(2)), 0x08070605);
}

TEST(MemoryTest, Register)
{
    Memory memory { 17, 0, 0 };

    EXPECT_THROW(memory.GetRegister(50), std::out_of_range);

    memory.SetRegister(18, 0x1234);
    ASSERT_EQ(memory.GetRegister(18), 0x1234);
}

TEST(MemoryTest, ValidAddressParse)
{
    {
        const char input[] = "0x12345678";

        Address addr;
        ASSERT_TRUE(Address::Parse(input, input + strlen(input), addr));
        ASSERT_EQ(addr.base, Address::BaseType::Data);
        ASSERT_EQ(addr.offset, 0x02345678);
    }

    {
        const char input[] = "0x400000:0x400010";

        auto    colonPos = strcspn(input, ":");
        Address addr;

        ASSERT_TRUE(Address::Parse(input, input + colonPos, addr));
        ASSERT_EQ(addr.base, Address::BaseType::Text);
        ASSERT_EQ(addr.offset, 0);

        ASSERT_TRUE(Address::Parse(input + colonPos + 1, input + strlen(input), addr));
        ASSERT_EQ(addr.base, Address::BaseType::Text);
        ASSERT_EQ(addr.offset, 0x10);
    }
}

TEST(MemoryTest, InvalidAddressParse)
{
    const char inputs[][20] = { "0xkhjasd129678", "Hello world!", "128763", "0x12345678909" };

    Address addr;
    for (auto& input : inputs) ASSERT_FALSE(Address::Parse(input, input + strlen(input), addr));
}