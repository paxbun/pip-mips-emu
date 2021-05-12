// Copyright (c) 2021 Chanjung Kim (paxbun). All rights reserved.
// Licensed under the MIT License.

#include <pip-mips-emu/Emulator.hh>
#include <pip-mips-emu/File.hh>
#include <pip-mips-emu/Implementations.hh>
#include <pip-mips-emu/Memory.hh>

#include <charconv>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>

enum class BranchPredictionType
{
    AlwaysTaken,
    AlwaysNotTaken,
};

struct Options
{
    BranchPredictionType  predictionType     = BranchPredictionType::AlwaysTaken;
    std::optional<Range>  range              = std::nullopt;
    bool                  dumpEachTickTock   = false;
    bool                  dumpPcEachTickTock = false;
    uint32_t              numInstructions    = std::numeric_limits<uint32_t>::max();
    std::filesystem::path filePath {};
};

Options ParseCommandArgs(int argc, char* argv[])
{
    bool branchPredictionTypeGiven = false;
    bool filePathGiven             = false;

    Options options;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-atp") == 0)
        {
            if (branchPredictionTypeGiven)
                throw std::runtime_error { "Multiple branch prediction types are given" };
            branchPredictionTypeGiven = true;
            options.predictionType    = BranchPredictionType::AlwaysTaken;
        }
        else if (strcmp(argv[i], "-antp") == 0)
        {
            if (branchPredictionTypeGiven)
                throw std::runtime_error { "Multiple branch prediction types are given" };
            branchPredictionTypeGiven = true;
            options.predictionType    = BranchPredictionType::AlwaysNotTaken;
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            if (i == argc - 1)
                throw std::runtime_error { "Missing addresses after '-m'" };

            char const* input = argv[++i];

            Range range;
            auto  length   = strlen(input);
            auto  colonPos = strcspn(input, ":");

            if (length == colonPos)
                throw std::runtime_error { "Invalid address format" };

            if (!Address::Parse(input, input + colonPos, range.begin))
                throw std::runtime_error { "Invalid address format" };

            if (!Address::Parse(input + colonPos + 1, input + length, range.end))
                throw std::runtime_error { "Invalid address format" };

            options.range = range;
        }
        else if (strcmp(argv[i], "-d") == 0)
        {
            if (options.dumpEachTickTock)
                throw std::runtime_error { "Duplicate option: '-d'" };
            options.dumpEachTickTock = true;
        }
        else if (strcmp(argv[i], "-p") == 0)
        {
            if (options.dumpPcEachTickTock)
                throw std::runtime_error { "Duplicate option: '-p'" };
            options.dumpPcEachTickTock = true;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            if (i == argc - 1)
                throw std::runtime_error { "Missing number of instructions after '-n'" };

            char const* input = argv[++i];

            auto result = std::from_chars(input, input + strlen(input), options.numInstructions);
            if (result.ec != std::errc {})
                throw std::runtime_error { "Invalid number of instructions" };
        }
        else
        {
            if (filePathGiven)
                throw std::runtime_error { "Multiple files are given" };
            filePathGiven    = true;
            options.filePath = argv[i];
        }
    }

    if (!branchPredictionTypeGiven)
        throw std::runtime_error { "No branch prediction type is given" };

    if (!filePathGiven)
        throw std::runtime_error { "No file is given" };

    return options;
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>> LoadMemory(Options const& options)
{
    FileReadResult fileResult { ReadFile(options.filePath) };
    if (std::holds_alternative<CannotRead>(fileResult))
    {
        CannotRead  error = std::get<CannotRead>(fileResult);
        char const* msg   = "Unknown file I/O error";
        switch (error.error.type)
        {
        case FileReadError::Type::FileDoesNotExist:
        {
            msg = "File does not exist";
            break;
        }
        case FileReadError::Type::GivenPathIsDirectory:
        {
            msg = "File is directory";
            break;
        }
        case FileReadError::Type::InvalidFormat:
        {
            msg = "Invalid file";
            break;
        }
        case FileReadError::Type::SectionSizeDoesNotMatch:
        {
            msg = "Section size does not match";
            break;
        }
        }

        throw std::runtime_error { msg };
    }
    CanRead file = std::get<CanRead>(fileResult);

    return std::make_pair(std::move(file.text), std::move(file.data));
}

int main(int argc, char* argv[])
{
    try
    {
        std::ios::sync_with_stdio(false);

        Options         options = ParseCommandArgs(argc, argv);
        EmulatorBuilder builder;

        builder.AddDatapath<InstructionFetch>()
            .AddDatapath<InstructionDecode>()
            .AddDatapath<Execution>()
            .AddDatapath<MemoryAccess>()
            .AddDatapath<WriteBack>()
            .AddHandler<DefaultHandler>();

        if (options.predictionType == BranchPredictionType::AlwaysTaken)
            builder.AddController<ATPPipelineStateController>();
        else if (options.predictionType == BranchPredictionType::AlwaysNotTaken)
            builder.AddController<ANTPPipelineStateController>();

        auto [text, data]       = LoadMemory(options);
        auto [emulator, memory] = builder.Build(std::move(text), std::move(data));
        auto& handler           = emulator.GetHandler();

        TickTockResult result = TickTockResult::Success;

        uint32_t i, j = 0;
        for (i = 1; j < options.numInstructions && emulator.IsTerminated(memory); ++i)
        {
            result = emulator.TickTock(memory, j);
            if (result != TickTockResult::Success)
                break;

            std::cout << "===== Cycle " << i << " =====\n";

            if (options.dumpPcEachTickTock)
            {
                handler->DumpPCs(memory, std::cout);
                std::cout << '\n';
            }

            if (options.dumpEachTickTock)
            {
                handler->DumpRegisters(memory, std::cout);
                std::cout << '\n';
                if (options.range)
                {
                    auto& range = options.range.value();
                    handler->DumpMemory(memory, range, std::cout);
                    std::cout << '\n';
                }
            }
        }

        std::cout << "===== Completion cycle: " << (i - 1) << " =====\n";

        if (options.dumpPcEachTickTock)
        {
            handler->DumpPCs(memory, std::cout);
        }

        handler->DumpRegisters(memory, std::cout);
        std::cout << '\n';
        if (options.range)
        {
            auto& range = options.range.value();
            handler->DumpMemory(memory, range, std::cout);
            std::cout << '\n';
        }

        return 0;
    }
    catch (std::exception const& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}