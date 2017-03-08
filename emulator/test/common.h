#pragma once

#include <gtest/gtest.h>
#include <memory.h>
#include <cpu.h>

struct MemoryTest : testing::Test {
    Frankenstein::Memory ram;

    MemoryTest()
    {
    }

    virtual ~MemoryTest()
    {
    }
};

struct CPUTest : MemoryTest {
    Frankenstein::Cpu cpu;

    CPUTest() : cpu ( ram )
    {
    }

    virtual ~CPUTest()
    {
    }
};

struct RomTest : testing::Test {
    Frankenstein::Memory ram;
    Frankenstein::Cpu cpu;

    MemoryTest() : ram(), cpu(ram)
    {
    }

    virtual ~MemoryTest()
    {
    }
};