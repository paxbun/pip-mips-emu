// Copyright (c) 2021 Chanjung Kim. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <pip-mips-emu/NamedEntryMap.hh>

TEST(RegisterMapTest, ValidCase)
{
    uint32_t register1;
    uint32_t register2;
    uint32_t register3;

    RegisterMap map;
    map.AddEntry("register1", &register1, NamedEntryUsage::ReadWrite);
    map.AddEntry("register2", &register2, NamedEntryUsage::Write);
    map.AddEntry("register2", &register3, NamedEntryUsage::Read);

    auto list = map.Build();
    ASSERT_EQ(list.size(), 2);
    ASSERT_NE(list.find("register1"), list.end());
    ASSERT_NE(list.find("register2"), list.end());

    ASSERT_EQ(register1, list["register1"]);
    ASSERT_EQ(register2, list["register2"]);
    ASSERT_EQ(register3, list["register2"]);
}

TEST(RegisterMapTest, InvalidCase)
{
    uint32_t register1;
    uint32_t register2;

    RegisterMap map;
    map.AddEntry("register1", &register1, NamedEntryUsage::ReadWrite);
    map.AddEntry("register2", &register2, NamedEntryUsage::Write);

    EXPECT_THROW({ map.Build(); }, std::runtime_error);
}

TEST(SignalMapTest, ValidCase)
{
    uint32_t signal1;
    uint32_t signal2;
    uint32_t signal3;

    SignalMap map;
    map.AddEntry("signal1", &signal1, NamedEntryUsage::ReadWrite);
    map.AddEntry("signal2", &signal2, NamedEntryUsage::Write);
    map.AddEntry("signal2", &signal3, NamedEntryUsage::Read);

    auto list = map.Build();
    ASSERT_EQ(list.size(), 2);
    ASSERT_NE(list.find("signal1"), list.end());
    ASSERT_NE(list.find("signal2"), list.end());

    ASSERT_EQ(signal1, list["signal1"]);
    ASSERT_EQ(signal2, list["signal2"]);
    ASSERT_EQ(signal3, list["signal2"]);
}

TEST(SignalMapTest, InvalidCase)
{
    uint32_t signal1;

    SignalMap map;
    map.AddEntry("signal1", &signal1, NamedEntryUsage::Read);

    EXPECT_THROW({ map.Build(); }, std::runtime_error);
}
