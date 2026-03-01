#include "parser.hpp"
#include <cstdio>
#include <ctype.h>
#include <unordered_map>
#include <unordered_set>

namespace xilinx {

static std::string format_name_for_arch(Arch arch) {
    switch (arch) {
        case Arch::Zynq7000:
            return "Xilinx Zynq 7000 Boot Image";
        case Arch::ZynqMP:
            return "Xilinx Zynq UltraScale+ MPSoC Boot Image";
        case Arch::VersalGen1:
            return "Xilinx Versal Adaptive SoC Gen 1 PDI";
        case Arch::SpartanUltraScalePlus:
            return "Xilinx Spartan UltraScale+ PDI";
        case Arch::VersalGen2:
            return "Xilinx Versal AI Edge/Prime Gen 2 PDI";
        case Arch::PDI:
            return "Xilinx PDI Boot Image";
        case Arch::Unknown:
        default:
            return "";
    }
}

static void add_warning(ParsedImage& img, LogCallback logger, const std::string& message) {
    img.warnings.push_back(message);
    if (logger) {
        logger("WARNING: " + message + "\n");
    }
}

static bool is_placeholder_word(uint32_t value) {
    return value == 0 || value == 0xFFFFFFFF;
}

static bool has_non_placeholder_word(const uint32_t* words, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!is_placeholder_word(words[i])) {
            return true;
        }
    }
    return false;
}

static void set_key_source_metadata(ParsedImage& img, uint32_t key_source) {
    img.boot_header.key_source = key_source;
    img.boot_header.key_source_present = !is_placeholder_word(key_source);
}

static void set_iv_metadata(std::vector<uint32_t>& dest,
                            bool& present,
                            const uint32_t* words,
                            size_t count) {
    dest.assign(words, words + count);
    present = has_non_placeholder_word(words, count);
}

static void add_boot_attribute(ParsedImage& img, const std::string& name, uint32_t value) {
    img.boot_header.boot_attributes.push_back(BootAttributeWord{.name = name, .value = value});
}

static bool is_readable_range(Reader& reader, uint64_t offset, uint64_t length) {
    if (length == 0 || offset == 0xFFFFFFFFULL || length == 0xFFFFFFFFULL) {
        return false;
    }
    if (offset > UINT64_MAX - (length - 1)) {
        return false;
    }

    uint8_t byte = 0;
    if (!reader.read_bytes(offset, &byte, 1)) {
        return false;
    }
    if (!reader.read_bytes(offset + length - 1, &byte, 1)) {
        return false;
    }
    return true;
}

static void add_boot_image_range(ParsedImage& img,
                                 Reader& reader,
                                 LogCallback logger,
                                 const std::string& name,
                                 uint64_t offset,
                                 uint64_t length) {
    if (offset == 0 || offset == 0xFFFFFFFFULL || length == 0 || length == 0xFFFFFFFFULL) {
        return;
    }

    BootImageRange range;
    range.name = name;
    range.offset = offset;
    range.length = length;
    range.bounds_valid = is_readable_range(reader, offset, length);
    img.boot_header.image_ranges.push_back(range);

    if (!range.bounds_valid) {
        char msg[256] = {0};
        std::snprintf(msg,
                      sizeof(msg),
                      "%s range appears out-of-bounds or unreadable (offset=0x%llX, length=0x%llX).",
                      name.c_str(),
                      static_cast<unsigned long long>(offset),
                      static_cast<unsigned long long>(length));
        add_warning(img, logger, msg);
    }
}

static void add_boot_region_diagnostic(ParsedImage& img,
                                       Reader& reader,
                                       LogCallback logger,
                                       const std::string& name,
                                       uint64_t offset,
                                       uint64_t size,
                                       MetadataChecksumStatus checksum_status = MetadataChecksumStatus::NotPresent) {
    BootRegionDiagnostic diagnostic;
    diagnostic.name = name;
    diagnostic.present = offset != 0 && offset != 0xFFFFFFFFULL && size != 0 && size != 0xFFFFFFFFULL;
    diagnostic.offset = offset;
    diagnostic.size = size;
    diagnostic.bounds_valid = diagnostic.present && is_readable_range(reader, offset, size);
    diagnostic.checksum_status = checksum_status;
    img.boot_header.region_diagnostics.push_back(diagnostic);

    if (diagnostic.present && !diagnostic.bounds_valid) {
        char msg[256] = {0};
        std::snprintf(msg,
                      sizeof(msg),
                      "%s diagnostic region is unreadable (offset=0x%llX, size=0x%llX).",
                      name.c_str(),
                      static_cast<unsigned long long>(offset),
                      static_cast<unsigned long long>(size));
        add_warning(img, logger, msg);
    }
}

static OptionalDataChecksumStatus parse_optional_data_checksum_status(const std::vector<uint32_t>& words,
                                                                      uint32_t& checksum_word_out) {
    checksum_word_out = 0;
    if (words.size() < 2) {
        return OptionalDataChecksumStatus::NotPresent;
    }

    checksum_word_out = words.back();
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < words.size(); ++i) {
        sum += words[i];
    }
    return sum == checksum_word_out ? OptionalDataChecksumStatus::Valid : OptionalDataChecksumStatus::Invalid;
}

static void parse_versal_optional_data(Reader& reader,
                                       ParsedImage& img,
                                       LogCallback logger,
                                       const versal::ImageHeaderTable& iht,
                                       uint32_t image_header_table_offset) {
    const uint32_t total_optional_words = iht.optional_data_length;
    if (total_optional_words == 0 || total_optional_words == 0xFFFFFFFF) {
        return;
    }

    uint64_t cursor = static_cast<uint64_t>(image_header_table_offset) + sizeof(versal::ImageHeaderTable);
    uint32_t consumed_words = 0;

    while (consumed_words < total_optional_words) {
        uint32_t header_word = 0;
        if (!reader.read_bytes(cursor, &header_word, sizeof(header_word))) {
            add_warning(img, logger, "Optional data parsing stopped: failed to read optional-data entry header.");
            break;
        }

        OptionalDataEntry entry;
        entry.id = static_cast<uint16_t>(header_word & 0xFFFF);
        entry.size_words = static_cast<uint16_t>((header_word >> 16) & 0xFFFF);
        entry.offset = cursor;

        if (entry.size_words == 0) {
            add_warning(img, logger, "Optional data parsing stopped: encountered entry with zero size.");
            break;
        }
        if (consumed_words + entry.size_words > total_optional_words) {
            add_warning(img, logger, "Optional data parsing stopped: entry size exceeds declared optional-data length.");
            break;
        }

        std::vector<uint32_t> entry_words(entry.size_words, 0);
        if (!reader.read_bytes(cursor, entry_words.data(), static_cast<size_t>(entry.size_words) * sizeof(uint32_t))) {
            add_warning(img, logger, "Optional data parsing stopped: failed to read full optional-data entry payload.");
            break;
        }

        if (entry_words.size() > 2) {
            entry.data_words.assign(entry_words.begin() + 1, entry_words.end() - 1);
        }

        entry.checksum_status = parse_optional_data_checksum_status(entry_words, entry.checksum_word);
        if (entry.checksum_status == OptionalDataChecksumStatus::Invalid) {
            char msg[256] = {0};
            std::snprintf(msg,
                          sizeof(msg),
                          "Optional data entry ID=%u at 0x%llX failed checksum validation.",
                          static_cast<unsigned>(entry.id),
                          static_cast<unsigned long long>(entry.offset));
            add_warning(img, logger, msg);
        }

        img.boot_header.optional_data_entries.push_back(std::move(entry));

        cursor += static_cast<uint64_t>(entry.size_words) * 4;
        consumed_words += entry.size_words;
    }

    if (consumed_words != total_optional_words) {
        add_warning(img,
                    logger,
                    "Optional data parsing consumed fewer words than declared by Optional Data Length field.");
    }
}

struct ImageHeaderContext {
    std::string name;
    uint32_t partition_count = 0;
    uint32_t first_partition_header_offset = 0; // bytes
};

struct VersalImageHeaderContext {
    std::string name;
    uint32_t first_partition_header_offset = 0; // bytes
    uint32_t partition_count = 0;
};

static std::string sanitize_name(std::string value) {
    for (char& c : value) {
        if (!isalnum(c) && c != '_' && c != '.') {
            c = '_';
        }
    }
    return value;
}

static std::string parse_versal_image_name(const versal::ImageHeader& ih) {
    std::string name;
    for (char c : ih.image_name) {
        if (c == '\0') {
            break;
        }
        name.push_back(c);
    }
    return sanitize_name(name);
}

static std::vector<VersalImageHeaderContext> parse_versal_image_headers(Reader& reader,
                                                                         uint32_t first_image_header_word_offset,
                                                                         uint32_t total_images) {
    std::vector<VersalImageHeaderContext> contexts;
    if (first_image_header_word_offset == 0 || first_image_header_word_offset == 0xFFFFFFFF || total_images == 0 ||
        total_images == 0xFFFFFFFF) {
        return contexts;
    }

    const uint32_t first_image_header_offset = first_image_header_word_offset * 4;
    for (uint32_t i = 0; i < total_images; ++i) {
        versal::ImageHeader ih{};
        const uint32_t ih_offset = first_image_header_offset + i * static_cast<uint32_t>(sizeof(versal::ImageHeader));
        if (!reader.read_bytes(ih_offset, &ih, sizeof(ih))) {
            break;
        }

        VersalImageHeaderContext ctx;
        ctx.first_partition_header_offset = ih.first_partition_header_word_offset * 4;
        ctx.partition_count = ih.partition_count;
        ctx.name = parse_versal_image_name(ih);
        contexts.push_back(std::move(ctx));
    }

    return contexts;
}

static std::unordered_map<uint32_t, std::string> build_versal_partition_owner_map(
    Reader& reader,
    const std::vector<VersalImageHeaderContext>& image_contexts,
    ParsedImage& img,
    LogCallback logger) {
    std::unordered_map<uint32_t, std::string> owners;

    for (size_t i = 0; i < image_contexts.size(); ++i) {
        const auto& ctx = image_contexts[i];
        const std::string image_name = ctx.name.empty() ? ("IMAGE_" + std::to_string(i)) : ctx.name;

        if (ctx.first_partition_header_offset == 0 || ctx.first_partition_header_offset == 0xFFFFFFFF) {
            add_warning(img,
                        logger,
                        "Versal image header has no valid first partition pointer; synthetic partition naming may be used.");
            continue;
        }

        uint32_t current_partition_offset = ctx.first_partition_header_offset;
        uint32_t traversed = 0;
        while (current_partition_offset != 0 && current_partition_offset != 0xFFFFFFFF && traversed < ctx.partition_count) {
            owners[current_partition_offset] = image_name;

            versal::PartitionHeader ph{};
            if (!reader.read_bytes(current_partition_offset, &ph, sizeof(ph))) {
                break;
            }
            current_partition_offset = ph.next_partition_header_offset * 4;
            traversed++;
        }

        if (ctx.partition_count != 0 && traversed != ctx.partition_count) {
            char msg[256] = {0};
            std::snprintf(msg,
                          sizeof(msg),
                          "Versal image '%s' declares %u partition(s) but traversed %u from its first partition link.",
                          image_name.c_str(),
                          ctx.partition_count,
                          traversed);
            add_warning(img, logger, msg);
        }
    }

    return owners;
}

template <typename ImageHeaderT>
static std::unordered_map<uint32_t, ImageHeaderContext> walk_image_header_chain(Reader& reader,
                                                                                 uint32_t first_image_header_word_offset,
                                                                                 uint32_t max_images) {
    std::unordered_map<uint32_t, ImageHeaderContext> image_headers;
    std::unordered_set<uint32_t> seen_offsets;

    uint32_t current_word_offset = first_image_header_word_offset;
    uint32_t image_count = 0;

    while (current_word_offset != 0 && current_word_offset != 0xFFFFFFFF && image_count < max_images) {
        const uint32_t current_offset = current_word_offset * 4;
        if (!seen_offsets.insert(current_offset).second) {
            break;
        }

        ImageHeaderT ih{};
        if (!reader.read_bytes(current_offset, &ih, sizeof(ih))) {
            break;
        }

        ImageHeaderContext ctx;
        ctx.name = unpack_image_name(reader, current_offset);
        ctx.partition_count = ih.partition_count;
        ctx.first_partition_header_offset = ih.corresponding_partition_header * 4;
        image_headers[current_offset] = std::move(ctx);

        current_word_offset = ih.next_image_header_offset;
        image_count++;
    }

    return image_headers;
}

static void validate_image_header_partition_links(
    ParsedImage& img,
    LogCallback logger,
    const std::unordered_map<uint32_t, ImageHeaderContext>& image_headers,
    const std::unordered_map<uint32_t, uint32_t>& observed_partition_counts,
    const std::unordered_set<uint32_t>& traversed_partition_header_offsets) {
    for (const auto& [image_header_offset, ctx] : image_headers) {
        const auto observed_it = observed_partition_counts.find(image_header_offset);
        const uint32_t observed = observed_it == observed_partition_counts.end() ? 0 : observed_it->second;
        if (ctx.partition_count != 0 && observed != ctx.partition_count) {
            char msg[256] = {0};
            std::snprintf(msg,
                          sizeof(msg),
                          "Image header at 0x%08X reports %u partition(s) but parser traversed %u.",
                          image_header_offset,
                          ctx.partition_count,
                          observed);
            add_warning(img, logger, msg);
        }

        if (ctx.first_partition_header_offset != 0 &&
            ctx.first_partition_header_offset != 0xFFFFFFFF &&
            traversed_partition_header_offsets.count(ctx.first_partition_header_offset) == 0) {
            char msg[256] = {0};
            std::snprintf(msg,
                          sizeof(msg),
                          "Image header at 0x%08X points to first partition header 0x%08X which was not traversed.",
                          image_header_offset,
                          ctx.first_partition_header_offset);
            add_warning(img, logger, msg);
        }
    }
}

static const char* ida_processor_name_for_family(ProcessorFamily family) {
    switch (family) {
        case ProcessorFamily::Arm:
            return "arm";
        case ProcessorFamily::MicroBlaze:
            return "mblaze";
        case ProcessorFamily::Unknown:
        default:
            return "";
    }
}

static void set_processor_selection(ParsedImage& img,
                                    ProcessorFamily family,
                                    ArmBitnessHint arm_bitness_hint,
                                    ProcessorInferenceConfidence confidence,
                                    const std::string& source) {
    img.processor_selection.family = family;
    img.processor_selection.arm_bitness_hint = arm_bitness_hint;
    img.processor_selection.confidence = confidence;
    img.processor_selection.source = source;
    img.processor_name = ida_processor_name_for_family(family);
}

static void clear_processor_selection(ParsedImage& img) {
    img.processor_selection = ProcessorSelection{};
    img.processor_name.clear();
}

static bool is_present_length(uint32_t length) {
    return length != 0 && length != 0xFFFFFFFF;
}

static bool has_microblaze_plm_partition(const ParsedImage& img) {
    for (const auto& part : img.partitions) {
        if (part.name == "PLM" && part.processor_family == ProcessorFamily::MicroBlaze) {
            return true;
        }
    }
    return false;
}

static DestinationCpu decode_zynqmp_destination_cpu(uint32_t attributes) {
    switch ((attributes >> 8) & 0xF) {
        case 0x0: return DestinationCpu::None;
        case 0x1: return DestinationCpu::A53_0;
        case 0x2: return DestinationCpu::A53_1;
        case 0x3: return DestinationCpu::A53_2;
        case 0x4: return DestinationCpu::A53_3;
        case 0x5: return DestinationCpu::R5_0;
        case 0x6: return DestinationCpu::R5_1;
        case 0x7: return DestinationCpu::R5_Lockstep;
        case 0x8: return DestinationCpu::PMU;
        default:  return DestinationCpu::Unknown;
    }
}

static DestinationCpu decode_versal_gen1_destination_cpu(uint32_t attributes) {
    switch ((attributes >> 8) & 0xF) {
        case 0x0: return DestinationCpu::None;
        case 0x1: return DestinationCpu::A72_0;
        case 0x2: return DestinationCpu::A72_1;
        case 0x5: return DestinationCpu::R5_0;
        case 0x6: return DestinationCpu::R5_1;
        case 0x7: return DestinationCpu::R5_Lockstep;
        case 0x8: return DestinationCpu::PSM;
        case 0x9: return DestinationCpu::AIE;
        default:  return DestinationCpu::Unknown;
    }
}

static DestinationCpu decode_versal_gen2_destination_cpu(uint32_t attributes) {
    switch ((attributes >> 8) & 0xF) {
        case 0x0: return DestinationCpu::None;
        case 0x1: return DestinationCpu::A78_0;
        case 0x2: return DestinationCpu::A78_1;
        case 0x3: return DestinationCpu::A78_2;
        case 0x4: return DestinationCpu::A78_3;
        case 0x5: return DestinationCpu::R52_0;
        case 0x6: return DestinationCpu::R52_1;
        case 0x8: return DestinationCpu::ASU;
        case 0x9: return DestinationCpu::AIE;
        default:  return DestinationCpu::Unknown;
    }
}

static ProcessorFamily processor_family_for_destination_cpu(DestinationCpu destination_cpu) {
    switch (destination_cpu) {
        case DestinationCpu::A53_0:
        case DestinationCpu::A53_1:
        case DestinationCpu::A53_2:
        case DestinationCpu::A53_3:
        case DestinationCpu::R5_0:
        case DestinationCpu::R5_1:
        case DestinationCpu::R5_Lockstep:
        case DestinationCpu::A72_0:
        case DestinationCpu::A72_1:
        case DestinationCpu::A78_0:
        case DestinationCpu::A78_1:
        case DestinationCpu::A78_2:
        case DestinationCpu::A78_3:
        case DestinationCpu::R52_0:
        case DestinationCpu::R52_1:
            return ProcessorFamily::Arm;
        case DestinationCpu::PMU:
        case DestinationCpu::PSM:
        case DestinationCpu::ASU:
            return ProcessorFamily::MicroBlaze;
        case DestinationCpu::AIE:
        case DestinationCpu::None:
        case DestinationCpu::Unknown:
        default:
            return ProcessorFamily::Unknown;
    }
}

static bool is_a5x_family_destination_cpu(DestinationCpu destination_cpu) {
    return destination_cpu == DestinationCpu::A53_0 ||
           destination_cpu == DestinationCpu::A53_1 ||
           destination_cpu == DestinationCpu::A53_2 ||
           destination_cpu == DestinationCpu::A53_3 ||
           destination_cpu == DestinationCpu::A72_0 ||
           destination_cpu == DestinationCpu::A72_1 ||
           destination_cpu == DestinationCpu::A78_0 ||
           destination_cpu == DestinationCpu::A78_1 ||
           destination_cpu == DestinationCpu::A78_2 ||
           destination_cpu == DestinationCpu::A78_3;
}

static ArmBitnessHint decode_a5x_exec_state(uint32_t attributes) {
    return ((attributes >> 3) & 0x1) ? ArmBitnessHint::AArch32 : ArmBitnessHint::AArch64;
}

static PartitionChecksumType decode_zynq7000_checksum_type(uint32_t attributes) {
    switch ((attributes >> 12) & 0x7) {
        case 0x0:
            return PartitionChecksumType::None;
        case 0x1:
            return PartitionChecksumType::Md5;
        default:
            return PartitionChecksumType::Other;
    }
}

static PartitionChecksumType decode_zynqmp_checksum_type(uint32_t attributes) {
    switch ((attributes >> 12) & 0x7) {
        case 0x0:
            return PartitionChecksumType::None;
        case 0x3:
            return PartitionChecksumType::Sha3;
        default:
            return PartitionChecksumType::Other;
    }
}

static PartitionHashAlgorithm hash_algo_for_checksum_type(PartitionChecksumType checksum_type) {
    switch (checksum_type) {
        case PartitionChecksumType::Md5:
            return PartitionHashAlgorithm::Md5;
        case PartitionChecksumType::Sha3:
            return PartitionHashAlgorithm::Sha3;
        case PartitionChecksumType::None:
        case PartitionChecksumType::Unknown:
        case PartitionChecksumType::Other:
        default:
            return PartitionHashAlgorithm::Unknown;
    }
}

static bool has_valid_word_offset(uint32_t value) {
    return value != 0 && value != 0xFFFFFFFF;
}

static void add_partition_security_warning(PartitionInfo& part,
                                           ParsedImage& img,
                                           const std::string& warning_message) {
    part.security_warnings.push_back(warning_message);

    std::string partition_name = part.name.empty() ? "<unnamed_partition>" : part.name;
    img.security_warnings.push_back(partition_name + ": " + warning_message);
}

static bool has_valid_exec_address(uint64_t exec_address) {
    return exec_address != 0 && exec_address != 0xFFFFFFFFULL;
}

struct ProcessorFamilyScores {
    int arm = 0;
    int microblaze = 0;
};

static bool is_known_processor_family(ProcessorFamily family) {
    return family == ProcessorFamily::Arm || family == ProcessorFamily::MicroBlaze;
}

enum class ChecksumStatus {
    NotPresent,
    Valid,
    Invalid,
};

static ChecksumStatus validate_inverse_sum_checksum(Reader& reader,
                                                    uint32_t start_offset,
                                                    uint32_t checksum_offset) {
    if (checksum_offset < start_offset || ((checksum_offset - start_offset) % 4) != 0) {
        return ChecksumStatus::NotPresent;
    }

    uint32_t checksum_word = 0;
    if (!reader.read_bytes(checksum_offset, &checksum_word, sizeof(checksum_word))) {
        return ChecksumStatus::NotPresent;
    }
    if (checksum_word == 0 || checksum_word == 0xFFFFFFFF) {
        return ChecksumStatus::NotPresent;
    }

    uint32_t sum = 0;
    for (uint32_t off = start_offset; off < checksum_offset; off += 4) {
        uint32_t word = 0;
        if (!reader.read_bytes(off, &word, sizeof(word))) {
            return ChecksumStatus::NotPresent;
        }
        sum += word;
    }

    const uint32_t inverse_sum = ~sum;
    const uint32_t negated_sum = static_cast<uint32_t>(0u - sum);
    if (checksum_word == inverse_sum || checksum_word == negated_sum) {
        return ChecksumStatus::Valid;
    }
    return ChecksumStatus::Invalid;
}

static void warn_if_invalid_checksum(ParsedImage& img,
                                     LogCallback logger,
                                     const std::string& label,
                                     ChecksumStatus status) {
    if (status == ChecksumStatus::Invalid) {
        add_warning(img, logger, label + " checksum mismatch; parser continues in degraded-trust mode.");
    }
}

static void add_score(ProcessorFamilyScores& scores, ProcessorFamily family, int score) {
    if (family == ProcessorFamily::Arm) {
        scores.arm += score;
    } else if (family == ProcessorFamily::MicroBlaze) {
        scores.microblaze += score;
    }
}

static ArmBitnessHint derive_arm_bitness_hint_from_partitions(const ParsedImage& img,
                                                               ArmBitnessHint fallback) {
    for (const auto& part : img.partitions) {
        if (part.processor_family == ProcessorFamily::Arm &&
            part.arm_bitness_hint != ArmBitnessHint::Unknown) {
            return part.arm_bitness_hint;
        }
    }
    return fallback;
}

static void apply_mixed_cpu_policy(ParsedImage& img, LogCallback logger) {
    if (!img.load_supported) {
        return;
    }

    ProcessorFamilyScores scores;

    if (img.arch == Arch::Zynq7000 || img.arch == Arch::ZynqMP) {
        add_score(scores, ProcessorFamily::Arm, 3);
    } else if (img.arch == Arch::VersalGen1 && has_microblaze_plm_partition(img)) {
        add_score(scores, ProcessorFamily::MicroBlaze, 3);
    }

    add_score(scores, img.processor_selection.family, 1);

    for (const auto& part : img.partitions) {
        if (!is_known_processor_family(part.processor_family)) {
            continue;
        }

        int part_score = 1;
        if (has_valid_exec_address(part.exec_address)) {
            part_score += 1;
        }
        add_score(scores, part.processor_family, part_score);
    }

    if (scores.arm == 0 && scores.microblaze == 0) {
        return;
    }

    ProcessorFamily selected_family = ProcessorFamily::Unknown;
    if (scores.arm > scores.microblaze) {
        selected_family = ProcessorFamily::Arm;
    } else if (scores.microblaze > scores.arm) {
        selected_family = ProcessorFamily::MicroBlaze;
    } else {
        switch (img.arch) {
            case Arch::VersalGen1:
                selected_family = ProcessorFamily::MicroBlaze;
                break;
            case Arch::Zynq7000:
            case Arch::ZynqMP:
                selected_family = ProcessorFamily::Arm;
                break;
            default:
                selected_family = is_known_processor_family(img.processor_selection.family)
                                      ? img.processor_selection.family
                                      : ProcessorFamily::Arm;
                break;
        }
    }

    const bool mixed_candidates_present = scores.arm > 0 && scores.microblaze > 0;
    if (mixed_candidates_present) {
        char source_msg[80] = {0};
        std::snprintf(source_msg,
                      sizeof(source_msg),
                      "mixed_policy:arm=%d,mblaze=%d",
                      scores.arm,
                      scores.microblaze);

        ArmBitnessHint arm_bitness_hint = ArmBitnessHint::Unknown;
        if (selected_family == ProcessorFamily::Arm) {
            arm_bitness_hint = derive_arm_bitness_hint_from_partitions(img, img.processor_selection.arm_bitness_hint);
            if (arm_bitness_hint == ArmBitnessHint::Unknown && img.arch == Arch::Zynq7000) {
                arm_bitness_hint = ArmBitnessHint::AArch32;
            }
        }

        set_processor_selection(img,
                                selected_family,
                                arm_bitness_hint,
                                ProcessorInferenceConfidence::Medium,
                                source_msg);
    }

    size_t mismatched_executable_partitions = 0;
    std::string first_mismatch_name;

    for (const auto& part : img.partitions) {
        if (!has_valid_exec_address(part.exec_address)) {
            continue;
        }
        if (!is_known_processor_family(part.processor_family)) {
            continue;
        }
        if (part.processor_family == img.processor_selection.family) {
            continue;
        }
        mismatched_executable_partitions++;
        if (first_mismatch_name.empty()) {
            first_mismatch_name = part.name;
        }
    }

    if (mismatched_executable_partitions > 0) {
        const char* selected_name = ida_processor_name_for_family(img.processor_selection.family);
        char warning_msg[256] = {0};
        std::snprintf(warning_msg,
                      sizeof(warning_msg),
                      "Mixed-CPU image: selected processor '%s' cannot represent %zu executable partition(s); example '%s'.",
                      selected_name,
                      mismatched_executable_partitions,
                      first_mismatch_name.empty() ? "unknown" : first_mismatch_name.c_str());
        add_warning(img, logger, warning_msg);
    }
}

static bool read_u32_at(Reader& reader, uint32_t offset, uint32_t& value) {
    return reader.read_bytes(offset, &value, sizeof(value));
}

static bool has_magic_at(Reader& reader, uint32_t width_offset, uint32_t signature_offset) {
    uint32_t width = 0;
    uint32_t signature = 0;
    if (!read_u32_at(reader, width_offset, width)) {
        return false;
    }
    if (!read_u32_at(reader, signature_offset, signature)) {
        return false;
    }
    return width == 0xAA995566 && check_magic(signature);
}

static bool is_valid_word_offset(uint32_t value) {
    return value != 0 && value != 0xFFFFFFFF && (value % 4) == 0;
}

static bool is_pdi_identification_string(uint32_t value) {
    return value == 0x49445046 || value == 0x49445050; // FPDI / PPDI
}

static bool is_versal_gen1_iht_version(uint32_t version) {
    return version == 0x00040000 || version == 0x00030000 || version == 0x00020000;
}

static bool has_versal_gen1_layout(Reader& reader, uint32_t meta_header_offset) {
    if (!is_valid_word_offset(meta_header_offset) || meta_header_offset < 0xF80) {
        return false;
    }

    uint32_t version = 0;
    uint32_t total_images = 0;
    uint32_t image_header_offset = 0;
    uint32_t total_partitions = 0;
    uint32_t partition_header_offset = 0;
    uint32_t identification = 0;

    if (!read_u32_at(reader, meta_header_offset + 0x00, version)) return false;
    if (!read_u32_at(reader, meta_header_offset + 0x04, total_images)) return false;
    if (!read_u32_at(reader, meta_header_offset + 0x08, image_header_offset)) return false;
    if (!read_u32_at(reader, meta_header_offset + 0x0C, total_partitions)) return false;
    if (!read_u32_at(reader, meta_header_offset + 0x10, partition_header_offset)) return false;
    if (!read_u32_at(reader, meta_header_offset + 0x28, identification)) return false;

    if (!is_versal_gen1_iht_version(version)) return false;
    if (total_images == 0 || total_images > 64) return false;
    if (total_partitions == 0 || total_partitions > 256) return false;
    if (!is_valid_word_offset(image_header_offset)) return false;
    if (!is_valid_word_offset(partition_header_offset)) return false;
    if (!is_pdi_identification_string(identification)) return false;

    return true;
}

static bool has_spartan_layout(Reader& reader) {
    uint32_t source_offset = 0;
    uint32_t plm_length = 0;
    uint32_t total_plm_length = 0;
    uint32_t checksum_at_33c = 0;

    if (!read_u32_at(reader, 0x1C, source_offset)) return false;
    if (!read_u32_at(reader, 0x2C, plm_length)) return false;
    if (!read_u32_at(reader, 0x30, total_plm_length)) return false;
    if (!read_u32_at(reader, 0x33C, checksum_at_33c)) return false;

    (void)checksum_at_33c;

    if (!is_valid_word_offset(source_offset)) return false;
    if (source_offset < 0x340 || source_offset >= 0xF80) return false;
    if (plm_length == 0 || plm_length == 0xFFFFFFFF) return false;
    if (total_plm_length == 0 || total_plm_length == 0xFFFFFFFF) return false;
    if (total_plm_length < plm_length) return false;

    return true;
}

static bool has_versal_gen2_iht_layout(Reader& reader, uint32_t iht_offset) {
    uint32_t version = 0;
    uint32_t total_images = 0;
    uint32_t image_header_offset = 0;
    uint32_t total_partitions = 0;
    uint32_t partition_header_offset = 0;
    uint32_t identification = 0;
    uint32_t header_sizes = 0;

    if (!read_u32_at(reader, iht_offset + 0x00, version)) return false;
    if (!read_u32_at(reader, iht_offset + 0x04, total_images)) return false;
    if (!read_u32_at(reader, iht_offset + 0x08, image_header_offset)) return false;
    if (!read_u32_at(reader, iht_offset + 0x0C, total_partitions)) return false;
    if (!read_u32_at(reader, iht_offset + 0x10, partition_header_offset)) return false;
    if (!read_u32_at(reader, iht_offset + 0x28, identification)) return false;
    if (!read_u32_at(reader, iht_offset + 0x2C, header_sizes)) return false;

    if (version != 0x00010000) return false;
    if (total_images == 0 || total_images > 32) return false;
    if (total_partitions == 0 || total_partitions > 32) return false;
    if (!is_valid_word_offset(image_header_offset)) return false;
    if (!is_valid_word_offset(partition_header_offset)) return false;
    if (!is_pdi_identification_string(identification)) return false;

    const uint32_t iht_words = header_sizes & 0xFF;
    const uint32_t image_header_words = (header_sizes >> 8) & 0xFF;
    const uint32_t partition_header_words = (header_sizes >> 16) & 0xFF;
    if (iht_words == 0 || image_header_words == 0 || partition_header_words == 0) {
        return false;
    }

    return true;
}

static bool find_versal_gen2_iht_offset(Reader& reader,
                                        uint32_t source_offset,
                                        uint32_t total_plm_length,
                                        uint32_t total_pmc_data_length,
                                        uint32_t& out_iht_offset) {
    const uint64_t candidate_offsets[] = {
        static_cast<uint64_t>(source_offset) + total_plm_length,
        static_cast<uint64_t>(source_offset) + total_plm_length + total_pmc_data_length,
    };

    for (uint64_t candidate : candidate_offsets) {
        if (candidate > 0xFFFFFFFFULL) continue;
        const uint32_t candidate32 = static_cast<uint32_t>(candidate);
        if (!is_valid_word_offset(candidate32)) continue;
        if (has_versal_gen2_iht_layout(reader, candidate32)) {
            out_iht_offset = candidate32;
            return true;
        }
    }

    uint64_t scan_start = source_offset;
    if (scan_start < 0x1140) {
        scan_start = 0x1140;
    }
    const uint64_t scan_end = scan_start + 0x40000;
    for (uint64_t off = scan_start; off < scan_end; off += 4) {
        if (off > 0xFFFFFFFFULL) break;
        uint32_t version = 0;
        if (!read_u32_at(reader, static_cast<uint32_t>(off), version)) {
            break;
        }
        if (version != 0x00010000) {
            continue;
        }
        if (has_versal_gen2_iht_layout(reader, static_cast<uint32_t>(off))) {
            out_iht_offset = static_cast<uint32_t>(off);
            return true;
        }
    }

    return false;
}

struct VersalGen2Probe {
    bool boot_header_layout_valid = false;
    bool iht_layout_valid = false;
    uint32_t iht_offset = 0;
};

static VersalGen2Probe probe_versal_gen2(Reader& reader) {
    VersalGen2Probe probe;

    uint32_t source_offset = 0;
    uint32_t total_pmc_data_length = 0;
    uint32_t plm_length = 0;
    uint32_t total_plm_length = 0;
    uint32_t checksum_at_113c = 0;

    if (!read_u32_at(reader, 0x1C, source_offset)) return probe;
    if (!read_u32_at(reader, 0x28, total_pmc_data_length)) return probe;
    if (!read_u32_at(reader, 0x2C, plm_length)) return probe;
    if (!read_u32_at(reader, 0x30, total_plm_length)) return probe;
    if (!read_u32_at(reader, 0x113C, checksum_at_113c)) return probe;

    (void)checksum_at_113c;

    if (!is_valid_word_offset(source_offset)) return probe;
    if (source_offset < 0x1140) return probe;
    if (plm_length == 0 || plm_length == 0xFFFFFFFF) return probe;
    if (total_plm_length == 0 || total_plm_length == 0xFFFFFFFF) return probe;
    if (total_plm_length < plm_length) return probe;

    probe.boot_header_layout_valid = true;
    probe.iht_layout_valid = find_versal_gen2_iht_offset(reader,
                                                          source_offset,
                                                          total_plm_length,
                                                          total_pmc_data_length,
                                                          probe.iht_offset);
    return probe;
}

static Arch classify_pdi_arch(Reader& reader, LogCallback logger) {
    const VersalGen2Probe gen2_probe = probe_versal_gen2(reader);
    if (gen2_probe.boot_header_layout_valid && gen2_probe.iht_layout_valid) {
        return Arch::VersalGen2;
    }
    if (gen2_probe.boot_header_layout_valid && !gen2_probe.iht_layout_valid) {
        if (logger) {
            logger("PDI rejected: Gen2-like boot header found but Gen2 IHT layout is inconsistent.\n");
        }
        return Arch::Unknown;
    }

    if (has_spartan_layout(reader)) {
        return Arch::SpartanUltraScalePlus;
    }

    uint32_t meta_header_offset = 0;
    if (read_u32_at(reader, 0xC4, meta_header_offset) && has_versal_gen1_layout(reader, meta_header_offset)) {
        return Arch::VersalGen1;
    }

    if (logger) {
        logger("PDI rejected: signature matched at 0x10/0x14 but family-specific layout checks failed.\n");
    }
    return Arch::Unknown;
}

std::string unpack_image_name(Reader& reader, uint32_t image_header_offset) {
    if (image_header_offset == 0 || image_header_offset == 0xFFFFFFFF) return "";
    
    uint32_t buffer[16] = {0}; // up to 64 bytes
    if (!reader.read_bytes(image_header_offset + 0x10, buffer, sizeof(buffer))) return "";

    std::string result;
    for (size_t i = 0; i < 16; ++i) {
        uint32_t word = buffer[i];
        if (word == 0) break;
        result += static_cast<char>((word >> 24) & 0xFF);
        result += static_cast<char>((word >> 16) & 0xFF);
        result += static_cast<char>((word >>  8) & 0xFF);
        result += static_cast<char>((word      ) & 0xFF);
    }
    while (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    
    return sanitize_name(result);
}

static void parse_zynq7000(Reader& reader, ParsedImage& img, LogCallback logger) {
    zynq7000::BootHeader bh;
    if (!reader.read_bytes(0, &bh, sizeof(bh))) {
        return;
    }

    img.boot_header.present = true;
    set_key_source_metadata(img, bh.key_source);
    add_boot_attribute(img, "header_version", bh.header_version);
    add_boot_attribute(img, "qspi_config_word", bh.qspi_config_word);
    add_boot_region_diagnostic(img,
                               reader,
                               logger,
                               "REGISTER_INIT",
                               0xA0,
                               sizeof(zynq7000::RegisterInitTable));

    img.bootloader_exec_address = bh.fsbl_execution_address;
    img.bootloader_load_address = bh.fsbl_load_address;
    img.bootloader_offset = bh.source_offset;
    img.bootloader_size = bh.fsbl_image_length;

    add_boot_image_range(img, reader, logger, "FSBL", bh.source_offset, bh.fsbl_image_length);
    add_boot_image_range(img, reader, logger, "FSBL_TOTAL", bh.source_offset, bh.total_fsbl_length);

    if (logger) {
        char msg[128] = {0};
        std::snprintf(msg, sizeof(msg), "Zynq 7000 Boot Header parsed. FSBL Exec: 0x%08X\n",
                      static_cast<unsigned int>(img.bootloader_exec_address));
        logger(msg);
    }

    warn_if_invalid_checksum(img,
                             logger,
                             "Zynq7000 boot header",
                             validate_inverse_sum_checksum(reader, 0x20, 0x48));

    uint32_t iht_offset = bh.image_header_table_offset;
    uint32_t version = reader.read_u32(iht_offset);
    uint32_t actual_iht_offset = 0;
    if (version == 0x01020000 || version == 0x01010000) {
        actual_iht_offset = iht_offset;
    } else {
        version = reader.read_u32(iht_offset * 4);
        if (version == 0x01020000 || version == 0x01010000) {
            actual_iht_offset = iht_offset * 4;
        }
    }

    if (actual_iht_offset == 0 || actual_iht_offset == 0xFFFFFFFF) {
        return;
    }

    add_boot_image_range(img,
                         reader,
                         logger,
                         "IMAGE_HEADER_TABLE",
                         actual_iht_offset,
                         sizeof(zynq7000::ImageHeaderTable));

    zynq7000::ImageHeaderTable iht;
    if (!reader.read_bytes(actual_iht_offset, &iht, sizeof(iht))) {
        return;
    }

    uint32_t max_images = iht.count_of_image_header;
    if (max_images == 0 || max_images == 0xFFFFFFFF) {
        max_images = 64;
    }
    const auto image_headers =
        walk_image_header_chain<zynq7000::ImageHeader>(reader, iht.first_image_header_offset, max_images);
    std::unordered_map<uint32_t, uint32_t> observed_partition_counts;
    std::unordered_set<uint32_t> traversed_partition_header_offsets;
    std::unordered_set<uint32_t> unknown_image_header_refs;

    uint32_t ph_offset = iht.first_partition_header_offset * 4;
    uint32_t count = 0;
    while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
        traversed_partition_header_offsets.insert(ph_offset);

        zynq7000::PartitionHeader ph;
        if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) {
            break;
        }

        if ((ph.unencrypted_partition_length == 0 && ph.total_partition_word_length == 0) ||
            ph.unencrypted_partition_length == 0xFFFFFFFF) {
            break;
        }

        PartitionInfo pinfo;
        pinfo.load_address = ph.destination_load_address;
        pinfo.exec_address = ph.destination_execution_address;
        pinfo.data_offset = ph.data_word_offset * 4;
        pinfo.data_size = ph.unencrypted_partition_length * 4;
        pinfo.is_encrypted = is_present_length(ph.encrypted_partition_length);
        pinfo.has_auth_certificate = has_valid_word_offset(ph.ac_offset) || (((ph.attributes >> 15) & 0x1) != 0);
        pinfo.checksum_type = decode_zynq7000_checksum_type(ph.attributes);
        pinfo.hash_algo = hash_algo_for_checksum_type(pinfo.checksum_type);
        const uint32_t image_header_offset = ph.image_header_word_offset * 4;
        auto ctx_it = image_headers.find(image_header_offset);
        if (ctx_it != image_headers.end() && !ctx_it->second.name.empty()) {
            pinfo.name = ctx_it->second.name;
            observed_partition_counts[image_header_offset]++;
        } else {
            pinfo.name = "PART_" + std::to_string(count);
            if (image_header_offset != 0 && image_header_offset != 0xFFFFFFFF &&
                unknown_image_header_refs.insert(image_header_offset).second) {
                char msg[192] = {0};
                std::snprintf(msg,
                              sizeof(msg),
                              "Partition references image header 0x%08X that was not found in traversed IH chain.",
                              image_header_offset);
                add_warning(img, logger, msg);
            }
        }

        if (pinfo.is_encrypted) {
            add_partition_security_warning(pinfo,
                                           img,
                                           "partition marked encrypted; payload may be ciphertext and not directly executable.");
        }

        img.partitions.push_back(pinfo);
        ph_offset += sizeof(zynq7000::PartitionHeader);
        count++;
    }

    validate_image_header_partition_links(
        img, logger, image_headers, observed_partition_counts, traversed_partition_header_offsets);
}

static void parse_zynqmp(Reader& reader, ParsedImage& img, LogCallback logger) {
    static constexpr uint64_t kZynqmpPmuRamBase = 0xFFDC0000ULL;

    zynqmp::BootHeader bh;
    if (!reader.read_bytes(0, &bh, sizeof(bh))) {
        return;
    }

    img.boot_header.present = true;
    set_key_source_metadata(img, bh.key_source);
    set_iv_metadata(img.boot_header.secure_header_iv,
                    img.boot_header.secure_header_iv_present,
                    bh.secure_header_iv,
                    3);
    set_iv_metadata(img.boot_header.obfuscated_black_key_iv,
                    img.boot_header.obfuscated_black_key_iv_present,
                    bh.obfuscated_black_key_iv,
                    3);
    add_boot_attribute(img, "fsbl_image_attributes", bh.fsbl_image_attributes);
    add_boot_region_diagnostic(img,
                               reader,
                               logger,
                               "REGISTER_INIT",
                               0xB8,
                               sizeof(zynqmp::RegisterInitTable));
    add_boot_region_diagnostic(img,
                               reader,
                               logger,
                               "PUF_HELPER_DATA",
                               0x8B8,
                               sizeof(zynqmp::PufHelperData));
    if (0xB8ULL + sizeof(zynqmp::RegisterInitTable) != 0x8B8ULL) {
        add_warning(img,
                    logger,
                    "ZynqMP register-init and PUF helper regions are not contiguous as expected by UG1283.");
    }

    img.bootloader_exec_address = bh.fsbl_execution_address;
    img.bootloader_load_address = bh.fsbl_execution_address;
    img.bootloader_size = bh.fsbl_image_length;

    if (is_present_length(bh.pmu_image_length)) {
        PartitionInfo pmufw;
        pmufw.load_address = kZynqmpPmuRamBase;
        pmufw.exec_address = 0;
        pmufw.data_offset = bh.source_offset;
        pmufw.data_size = bh.pmu_image_length;
        pmufw.processor_family = ProcessorFamily::MicroBlaze;
        pmufw.destination_cpu = DestinationCpu::PMU;
        pmufw.arm_bitness_hint = ArmBitnessHint::Unknown;
        pmufw.name = "PMUFW";
        img.partitions.push_back(pmufw);

        add_boot_image_range(img, reader, logger, "PMUFW", bh.source_offset, bh.pmu_image_length);
        add_boot_image_range(img,
                             reader,
                             logger,
                             "PMUFW_TOTAL",
                             bh.source_offset,
                             bh.total_pmu_fw_length);

        uint32_t pmu_prefix_length = bh.total_pmu_fw_length;
        if (!is_present_length(pmu_prefix_length)) {
            pmu_prefix_length = bh.pmu_image_length;
        }
        img.bootloader_offset = static_cast<uint64_t>(bh.source_offset) + pmu_prefix_length;
    } else {
        img.bootloader_offset = bh.source_offset;
    }

    if (is_present_length(bh.fsbl_image_length)) {
        PartitionInfo fsbl;
        fsbl.load_address = img.bootloader_load_address;
        fsbl.exec_address = img.bootloader_exec_address;
        fsbl.data_offset = img.bootloader_offset;
        fsbl.data_size = bh.fsbl_image_length;
        fsbl.is_bootloader_partition = true;
        fsbl.processor_family = ProcessorFamily::Arm;
        fsbl.destination_cpu = DestinationCpu::A53_0;
        fsbl.arm_bitness_hint = ArmBitnessHint::Unknown;
        fsbl.name = "FSBL";
        img.partitions.push_back(fsbl);
    }

    add_boot_image_range(img,
                         reader,
                         logger,
                         "FSBL",
                         img.bootloader_offset,
                         bh.fsbl_image_length);
    add_boot_image_range(img,
                         reader,
                         logger,
                         "FSBL_TOTAL",
                         img.bootloader_offset,
                         bh.total_fsbl_length);

    if (logger) {
        char msg[128] = {0};
        std::snprintf(msg, sizeof(msg), "ZynqMP Boot Header parsed. FSBL Exec: 0x%08X\n",
                      static_cast<unsigned int>(img.bootloader_exec_address));
        logger(msg);
    }

    warn_if_invalid_checksum(img,
                             logger,
                             "ZynqMP boot header",
                             validate_inverse_sum_checksum(reader, 0x20, 0x48));

    uint32_t iht_offset = bh.image_header_table_offset;
    uint32_t version = reader.read_u32(iht_offset);
    uint32_t actual_iht_offset = 0;
    if (version == 0x01020000 || version == 0x01010000) {
        actual_iht_offset = iht_offset;
    } else {
        version = reader.read_u32(iht_offset * 4);
        if (version == 0x01020000 || version == 0x01010000) {
            actual_iht_offset = iht_offset * 4;
        }
    }

    if (actual_iht_offset == 0 || actual_iht_offset == 0xFFFFFFFF) {
        return;
    }

    add_boot_image_range(img,
                         reader,
                         logger,
                         "IMAGE_HEADER_TABLE",
                         actual_iht_offset,
                         sizeof(zynqmp::ImageHeaderTable));

    zynqmp::ImageHeaderTable iht;
    if (!reader.read_bytes(actual_iht_offset, &iht, sizeof(iht))) {
        return;
    }

    warn_if_invalid_checksum(img,
                             logger,
                             "ZynqMP image header table",
                             validate_inverse_sum_checksum(reader, actual_iht_offset, actual_iht_offset + 0x3C));

    uint32_t max_images = iht.count_of_image_header;
    if (max_images == 0 || max_images == 0xFFFFFFFF) {
        max_images = 64;
    }
    const auto image_headers =
        walk_image_header_chain<zynqmp::ImageHeader>(reader, iht.first_image_header_offset, max_images);
    std::unordered_map<uint32_t, uint32_t> observed_partition_counts;
    std::unordered_set<uint32_t> traversed_partition_header_offsets;
    std::unordered_set<uint32_t> unknown_image_header_refs;

    uint32_t ph_offset = iht.first_partition_header_offset * 4;
    uint32_t count = 0;
    while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
        traversed_partition_header_offsets.insert(ph_offset);

        zynqmp::PartitionHeader ph;
        if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) {
            break;
        }

        if ((ph.unencrypted_data_word_length == 0 && ph.total_partition_word_length == 0) ||
            ph.unencrypted_data_word_length == 0xFFFFFFFF) {
            break;
        }

        PartitionInfo pinfo;
        pinfo.load_address = (static_cast<uint64_t>(ph.destination_load_address_hi) << 32) | ph.destination_load_address_lo;
        pinfo.exec_address = (static_cast<uint64_t>(ph.destination_execution_address_hi) << 32) | ph.destination_execution_address_lo;
        pinfo.data_offset = ph.actual_partition_word_offset * 4;
        pinfo.data_size = ph.unencrypted_data_word_length * 4;
        pinfo.is_encrypted = (((ph.attributes >> 7) & 0x1) != 0) ||
                             is_present_length(ph.encrypted_partition_data_word_length);
        pinfo.has_auth_certificate = has_valid_word_offset(ph.ac_offset) || (((ph.attributes >> 15) & 0x1) != 0);
        pinfo.checksum_type = decode_zynqmp_checksum_type(ph.attributes);
        pinfo.hash_algo = hash_algo_for_checksum_type(pinfo.checksum_type);
        pinfo.destination_cpu = decode_zynqmp_destination_cpu(ph.attributes);
        pinfo.processor_family = processor_family_for_destination_cpu(pinfo.destination_cpu);
        if (is_a5x_family_destination_cpu(pinfo.destination_cpu)) {
            pinfo.arm_bitness_hint = decode_a5x_exec_state(ph.attributes);
        }
        const uint32_t image_header_offset = ph.image_header_word_offset * 4;
        auto ctx_it = image_headers.find(image_header_offset);
        if (ctx_it != image_headers.end() && !ctx_it->second.name.empty()) {
            pinfo.name = ctx_it->second.name;
            observed_partition_counts[image_header_offset]++;
        } else {
            pinfo.name = "PART_" + std::to_string(count);
            if (image_header_offset != 0 && image_header_offset != 0xFFFFFFFF &&
                unknown_image_header_refs.insert(image_header_offset).second) {
                char msg[192] = {0};
                std::snprintf(msg,
                              sizeof(msg),
                              "Partition references image header 0x%08X that was not found in traversed IH chain.",
                              image_header_offset);
                add_warning(img, logger, msg);
            }
        }

        if (pinfo.is_encrypted) {
            add_partition_security_warning(pinfo,
                                           img,
                                           "partition marked encrypted; payload may be ciphertext and not directly executable.");
        }

        img.partitions.push_back(pinfo);
        ph_offset = ph.next_partition_header_offset * 4;
        count++;
    }

    validate_image_header_partition_links(
        img, logger, image_headers, observed_partition_counts, traversed_partition_header_offsets);
}

static void parse_versal_gen1(Reader& reader, ParsedImage& img, LogCallback logger) {
    versal::BootHeader bh;
    if (!reader.read_bytes(0, &bh, sizeof(bh))) {
        return;
    }

    img.boot_header.present = true;
    set_key_source_metadata(img, bh.key_source);
    set_iv_metadata(img.boot_header.black_iv,
                    img.boot_header.black_iv_present,
                    bh.black_iv,
                    3);
    set_iv_metadata(img.boot_header.secure_header_iv,
                    img.boot_header.secure_header_iv_present,
                    bh.secure_header_iv,
                    3);
    set_iv_metadata(img.boot_header.secure_header_iv_aux,
                    img.boot_header.secure_header_iv_aux_present,
                    bh.secure_header_iv_pmc,
                    3);
    add_boot_attribute(img, "boot_attributes", bh.attributes);
    add_boot_region_diagnostic(img,
                               reader,
                               logger,
                               "REGISTER_INIT",
                               0x128,
                               sizeof(versal::RegisterInitTable));
    add_boot_region_diagnostic(img,
                               reader,
                               logger,
                               "PUF_HELPER_DATA",
                               0x928,
                               sizeof(versal::PufHelperData));
    if (0x128ULL + sizeof(versal::RegisterInitTable) != 0x928ULL) {
        add_warning(img,
                    logger,
                    "Versal register-init and PUF helper regions are not contiguous as expected by UG1283.");
    }

    add_boot_image_range(img, reader, logger, "PLM", bh.plm_source_offset, bh.plm_length);
    add_boot_image_range(img, reader, logger, "PLM_TOTAL", bh.plm_source_offset, bh.total_plm_length);
    add_boot_image_range(img,
                         reader,
                         logger,
                         "PMC_DATA",
                         static_cast<uint64_t>(bh.plm_source_offset) + bh.total_plm_length,
                         bh.pmc_data_length);
    add_boot_image_range(img,
                         reader,
                         logger,
                         "PMC_DATA_TOTAL",
                         static_cast<uint64_t>(bh.plm_source_offset) + bh.total_plm_length,
                         bh.total_pmc_data_length);

    if (logger) {
        logger("Versal Gen1 Boot Header parsed. PLM Exec: 0xF0280000\n");
    }

    warn_if_invalid_checksum(img,
                             logger,
                             "Versal Gen1 boot header",
                             validate_inverse_sum_checksum(reader, 0x10, 0xF30));

    if (bh.plm_length > 0 && bh.plm_length != 0xFFFFFFFF) {
        PartitionInfo plm;
        plm.load_address = 0xF0280000;
        plm.exec_address = 0xF0280000;
        plm.data_offset = bh.plm_source_offset;
        plm.data_size = bh.plm_length;
        plm.processor_family = ProcessorFamily::MicroBlaze;
        plm.destination_cpu = DestinationCpu::PSM;
        plm.arm_bitness_hint = ArmBitnessHint::Unknown;
        plm.name = "PLM";
        img.partitions.push_back(plm);
    }

    if (bh.pmc_data_length > 0 && bh.pmc_data_length != 0xFFFFFFFF && bh.pmc_data_load_address != 0xFFFFFFFF) {
        PartitionInfo data;
        data.load_address = bh.pmc_data_load_address;
        data.exec_address = 0;
        data.data_offset = bh.plm_source_offset + bh.total_plm_length;
        data.data_size = bh.pmc_data_length;
        data.name = "PMC_DATA";
        img.partitions.push_back(data);
    }

    if (bh.meta_header_offset == 0 || bh.meta_header_offset == 0xFFFFFFFF) {
        return;
    }

    add_boot_image_range(img,
                         reader,
                         logger,
                         "IMAGE_HEADER_TABLE",
                         bh.meta_header_offset,
                         sizeof(versal::ImageHeaderTable));

    versal::ImageHeaderTable iht;
    if (!reader.read_bytes(bh.meta_header_offset, &iht, sizeof(iht))) {
        return;
    }

    warn_if_invalid_checksum(img,
                             logger,
                             "Versal Gen1 image header table",
                             validate_inverse_sum_checksum(reader, bh.meta_header_offset, bh.meta_header_offset + 0x7C));

    parse_versal_optional_data(reader, img, logger, iht, bh.meta_header_offset);

    const auto image_contexts =
        parse_versal_image_headers(reader, iht.image_header_offset, iht.total_number_of_images);
    const auto partition_owner_names = build_versal_partition_owner_map(reader, image_contexts, img, logger);
    std::unordered_set<uint32_t> unknown_owner_offsets;

    uint32_t ph_offset = iht.partition_header_offset * 4;
    uint32_t count = 0;
    while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
        versal::PartitionHeader ph;
        if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) {
            break;
        }

        if ((ph.unencrypted_data_word_length == 0 && ph.total_partition_word_length == 0) ||
            ph.unencrypted_data_word_length == 0xFFFFFFFF) {
            break;
        }

        PartitionInfo pinfo;
        pinfo.load_address = (static_cast<uint64_t>(ph.destination_load_address_hi) << 32) | ph.destination_load_address_lo;
        pinfo.exec_address = (static_cast<uint64_t>(ph.destination_execution_address_hi) << 32) | ph.destination_execution_address_lo;
        pinfo.data_offset = ph.actual_partition_word_offset * 4;
        pinfo.data_size = ph.unencrypted_data_word_length * 4;
        pinfo.is_encrypted = has_valid_word_offset(ph.encryption_key_select);
        pinfo.has_auth_certificate = has_valid_word_offset(ph.hash_block_ac_offset) ||
                                     has_valid_word_offset(ph.authentication_header);
        pinfo.checksum_type = PartitionChecksumType::Unknown;
        pinfo.hash_algo = PartitionHashAlgorithm::Unknown;
        pinfo.destination_cpu = decode_versal_gen1_destination_cpu(ph.attributes);
        pinfo.processor_family = processor_family_for_destination_cpu(pinfo.destination_cpu);
        if (is_a5x_family_destination_cpu(pinfo.destination_cpu)) {
            pinfo.arm_bitness_hint = decode_a5x_exec_state(ph.attributes);
        }

        auto owner_it = partition_owner_names.find(ph_offset);
        if (owner_it != partition_owner_names.end() && !owner_it->second.empty()) {
            pinfo.name = owner_it->second;
        } else {
            pinfo.name = "PDI_PART_" + std::to_string(count);
            if (unknown_owner_offsets.insert(ph_offset).second) {
                char msg[224] = {0};
                std::snprintf(msg,
                              sizeof(msg),
                              "Versal partition header at 0x%08X has no owning image-header context; using fallback name '%s'.",
                              ph_offset,
                              pinfo.name.c_str());
                add_warning(img, logger, msg);
            }
        }

        if (pinfo.is_encrypted) {
            add_partition_security_warning(pinfo,
                                           img,
                                           "partition encryption key selector is present; payload may be ciphertext.");
        }

        img.partitions.push_back(pinfo);
        ph_offset = ph.next_partition_header_offset * 4;
        count++;
    }
}

static void parse_spartan(Reader& reader, ParsedImage& img, LogCallback logger) {
    uint32_t source_offset = 0;
    uint32_t plm_length = 0;
    read_u32_at(reader, 0x1C, source_offset);
    read_u32_at(reader, 0x2C, plm_length);

    warn_if_invalid_checksum(img,
                             logger,
                             "Spartan boot header",
                             validate_inverse_sum_checksum(reader, 0x10, 0x33C));

    if (logger) {
        char msg[192] = {0};
        std::snprintf(msg, sizeof(msg),
                      "Spartan UltraScale+ parse entry point reached (source=0x%08X, plm_len=0x%08X). Detailed partition parsing pending.\n",
                      source_offset, plm_length);
        logger(msg);
    }
}

static void parse_versal_gen2(Reader& reader, ParsedImage& img, LogCallback logger) {
    VersalGen2Probe probe = probe_versal_gen2(reader);

    warn_if_invalid_checksum(img,
                             logger,
                             "Versal Gen2 boot header",
                             validate_inverse_sum_checksum(reader, 0x10, 0x113C));
    DestinationCpu first_partition_cpu = DestinationCpu::Unknown;
    if (probe.iht_layout_valid) {
        versal::ImageHeaderTable iht{};
        if (reader.read_bytes(probe.iht_offset, &iht, sizeof(iht))) {
            warn_if_invalid_checksum(img,
                                     logger,
                                     "Versal Gen2 image header table",
                                     validate_inverse_sum_checksum(reader, probe.iht_offset, probe.iht_offset + 0x7C));
            parse_versal_optional_data(reader, img, logger, iht, probe.iht_offset);
            const uint32_t first_partition_header_offset = iht.partition_header_offset * 4;
            if (first_partition_header_offset != 0 && first_partition_header_offset != 0xFFFFFFFF) {
                versal::PartitionHeader ph{};
                if (reader.read_bytes(first_partition_header_offset, &ph, sizeof(ph))) {
                    first_partition_cpu = decode_versal_gen2_destination_cpu(ph.attributes);
                }
            }
        }
    }

    if (logger) {
        char msg[224] = {0};
        std::snprintf(msg, sizeof(msg),
                      "Versal Gen2 parse entry point reached (iht_valid=%u, iht_offset=0x%08X, first_dest_cpu=%u). Detailed partition parsing pending.\n",
                      probe.iht_layout_valid ? 1U : 0U,
                      probe.iht_offset,
                      static_cast<unsigned int>(first_partition_cpu));
        logger(msg);
    }
}

ParsedImage parse_image(Reader& reader, LogCallback logger) {
    ParsedImage img;

    // Detect Arch
    if (has_magic_at(reader, 0x20, 0x24)) {
        uint32_t header_version = 0;
        if (!read_u32_at(reader, 0x2C, header_version)) {
            return img;
        }
        if (header_version == 0x01010000) {
            img.arch = Arch::Zynq7000;
        } else {
            img.arch = Arch::ZynqMP;
        }
    } else if (has_magic_at(reader, 0x10, 0x14)) {
        img.arch = classify_pdi_arch(reader, logger);
    }

    if (img.arch == Arch::Unknown) return img;
    img.format_name = format_name_for_arch(img.arch);

    switch (img.arch) {
        case Arch::Zynq7000:
            img.load_supported = true;
            set_processor_selection(img,
                                    ProcessorFamily::Arm,
                                    ArmBitnessHint::AArch32,
                                    ProcessorInferenceConfidence::High,
                                    "arch_default:zynq7000_boot_header");
            parse_zynq7000(reader, img, logger);
            break;
        case Arch::ZynqMP:
            img.load_supported = true;
            set_processor_selection(img,
                                    ProcessorFamily::Arm,
                                    ArmBitnessHint::Unknown,
                                    ProcessorInferenceConfidence::Medium,
                                    "arch_default:zynqmp_without_partition_attr_decode");
            parse_zynqmp(reader, img, logger);
            break;
        case Arch::VersalGen1:
            img.load_supported = true;
            set_processor_selection(img,
                                    ProcessorFamily::Arm,
                                    ArmBitnessHint::Unknown,
                                    ProcessorInferenceConfidence::Low,
                                    "legacy_fallback:versal_gen1_until_destination_cpu_decode");
            parse_versal_gen1(reader, img, logger);
            if (has_microblaze_plm_partition(img)) {
                set_processor_selection(img,
                                        ProcessorFamily::MicroBlaze,
                                        ArmBitnessHint::Unknown,
                                        ProcessorInferenceConfidence::High,
                                        "partition_context:versal_plm_ppu_microblaze_default");
            }
            break;
        case Arch::PDI:
            add_warning(img, logger,
                        "Generic PDI family detected without deterministic sub-family classification; loading is disabled for safety.");
            break;
        case Arch::SpartanUltraScalePlus:
            parse_spartan(reader, img, logger);
            add_warning(img, logger,
                        "Spartan UltraScale+ family is detected but full partition mapping is not implemented yet; image load is disabled to avoid unsafe mapping.");
            break;
        case Arch::VersalGen2:
            parse_versal_gen2(reader, img, logger);
            add_warning(img, logger,
                        "Versal Gen2 family is detected but full partition mapping is not implemented yet; image load is disabled to avoid unsafe mapping.");
            break;
        case Arch::Unknown:
        default:
            break;
    }

    apply_mixed_cpu_policy(img, logger);

    if (!img.load_supported) {
        clear_processor_selection(img);
    }

    return img;
}

} // namespace xilinx
