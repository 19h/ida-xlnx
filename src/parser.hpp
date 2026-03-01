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

enum class PartitionChecksumType {
    Unknown,
    None,
    Md5,
    Sha3,
    Other,
};

enum class PartitionHashAlgorithm {
    Unknown,
    Md5,
    Sha3,
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
    bool is_bootloader_partition = false;
    bool is_encrypted = false;
    bool has_auth_certificate = false;
    PartitionChecksumType checksum_type = PartitionChecksumType::Unknown;
    PartitionHashAlgorithm hash_algo = PartitionHashAlgorithm::Unknown;
    ProcessorFamily processor_family = ProcessorFamily::Unknown;
    DestinationCpu destination_cpu = DestinationCpu::Unknown;
    ArmBitnessHint arm_bitness_hint = ArmBitnessHint::Unknown;
    std::vector<std::string> security_warnings;
    std::string name;
};

struct BootAttributeWord {
    std::string name;
    uint32_t value = 0;
};

struct BootImageRange {
    std::string name;
    uint64_t offset = 0;
    uint64_t length = 0;
    bool bounds_valid = false;
};

enum class MetadataChecksumStatus {
    NotPresent,
    Valid,
    Invalid,
};

struct BootRegionDiagnostic {
    std::string name;
    bool present = false;
    uint64_t offset = 0;
    uint64_t size = 0;
    bool bounds_valid = false;
    MetadataChecksumStatus checksum_status = MetadataChecksumStatus::NotPresent;
};

enum class OptionalDataChecksumStatus {
    NotPresent,
    Valid,
    Invalid,
};

struct OptionalDataEntry {
    uint16_t id = 0;
    uint16_t size_words = 0;
    uint64_t offset = 0;
    std::vector<uint32_t> data_words;
    uint32_t checksum_word = 0;
    OptionalDataChecksumStatus checksum_status = OptionalDataChecksumStatus::NotPresent;
};

struct BootHeaderMetadata {
    bool present = false;

    bool key_source_present = false;
    uint32_t key_source = 0;

    bool secure_header_iv_present = false;
    std::vector<uint32_t> secure_header_iv;

    bool secure_header_iv_aux_present = false;
    std::vector<uint32_t> secure_header_iv_aux;

    bool obfuscated_black_key_iv_present = false;
    std::vector<uint32_t> obfuscated_black_key_iv;

    bool black_iv_present = false;
    std::vector<uint32_t> black_iv;

    std::vector<BootAttributeWord> boot_attributes;
    std::vector<BootImageRange> image_ranges;
    std::vector<BootRegionDiagnostic> region_diagnostics;
    std::vector<OptionalDataEntry> optional_data_entries;
};

struct ParsedImage {
    Arch arch = Arch::Unknown;
    std::string format_name;
    std::string processor_name;
    ProcessorSelection processor_selection;
    bool load_supported = false;
    std::vector<std::string> warnings;
    std::vector<std::string> security_warnings;
    BootHeaderMetadata boot_header;

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
