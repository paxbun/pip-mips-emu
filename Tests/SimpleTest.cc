// Copyright (c) 2021 Chanjung Kim (paxbun). All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <pip-mips-emu/Lib.hh>

TEST(SimpleTest, ValidCase)
{
    ASSERT_DOUBLE_EQ(Add(2.4, 1.7), 4.1);
}