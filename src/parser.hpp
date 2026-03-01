#pragma once
#include "xilinx_boot.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace xilinx {

enum class Arch { Unknown, Zynq7000, ZynqMP, PDI, SpartanUltraScalePlus, VersalGen1, VersalGen2 };

enum class ProcessorFamily {
    Unknown,
    Arm,
    MicroBlaze,
};

enum class ArmBitnessHint {
    Unknown,
    AArch32,
    AArch64,
};

enum class ProcessorInferenceConfidence {
    Unknown,
    Low,
    Medium,
    High,
};

enum class DestinationCpu {
    Unknown,
    None,
    A53_0,
    A53_1,
    A53_2,
    A53_3,
    R5_0,
    R5_1,
    R5_Lockstep,
    PMU,
    A72_0,
    A72_1,
    PSM,
    AIE,
    A78_0,
    A78_1,
    A78_2,
    A78_3,
    R52_0,
    R52_1,
    ASU,
};

struct ProcessorSelection {
    ProcessorFamily family = ProcessorFamily::Unknown;
    ArmBitnessHint arm_bitness_hint = ArmBitnessHint::Unknown;
    ProcessorInferenceConfidence confidence = ProcessorInferenceConfidence::Unknown;
    std::string source;
};

struct PartitionInfo {
    uint64_t load_address = 0;
    uint64_t exec_address = 0;
    uint64_t data_offset = 0;
    uint64_t data_size = 0;
    ProcessorFamily processor_family = ProcessorFamily::Unknown;
    DestinationCpu destination_cpu = DestinationCpu::Unknown;
    ArmBitnessHint arm_bitness_hint = ArmBitnessHint::Unknown;
    std::string name;
};

struct ParsedImage {
    Arch arch = Arch::Unknown;
    std::string format_name;
    std::string processor_name;
    ProcessorSelection processor_selection;
    bool load_supported = false;
    std::vector<std::string> warnings;

    uint64_t bootloader_load_address = 0;
    uint64_t bootloader_exec_address = 0;
    uint64_t bootloader_offset = 0;
    uint64_t bootloader_size = 0;

    std::vector<PartitionInfo> partitions;
};

// Abstract Reader interface so parsing can run without IDA SDK
struct Reader {
    virtual ~Reader() = default;
    virtual bool read_bytes(uint64_t offset, void* buffer, size_t size) = 0;
    
    uint32_t read_u32(uint64_t offset) {
        uint32_t val = 0;
        if (!read_bytes(offset, &val, 4)) return 0;
        return val;
    }
};

using LogCallback = std::function<void(const std::string&)>;

// Core parser logic
ParsedImage parse_image(Reader& reader, LogCallback logger = nullptr);

// Helper for image name unpacking
std::string unpack_image_name(Reader& reader, uint32_t image_header_offset);

} // namespace xilinx
