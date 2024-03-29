// Copyright (c) 2021 Chanjung Kim (paxbun). All rights reserved.
// Licensed under the MIT License.

#ifndef PIP_MIPS_EMU_COMMON_HH
#define PIP_MIPS_EMU_COMMON_HH

#include <cstdint>

/// <summary>
/// Parses a hexadecimal number.
/// </summary>
bool ParseWord(char const* begin, char const* end, uint32_t& out) noexcept;

#endif