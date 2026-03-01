#include "parser.hpp"
#include <cstdio>
#include <ctype.h>

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
    
    // Sanitize
    for (char& c : result) {
        if (!isalnum(c) && c != '_' && c != '.') c = '_';
    }
    return result;
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
    img.processor_name = "arm";

    if (img.arch == Arch::Zynq7000) {
        zynq7000::BootHeader bh;
        if (!reader.read_bytes(0, &bh, sizeof(bh))) return img;

        img.bootloader_exec_address = bh.fsbl_execution_address;
        img.bootloader_load_address = bh.fsbl_load_address;
        img.bootloader_offset = bh.source_offset;
        img.bootloader_size = bh.fsbl_image_length;

        if (logger) {
            char msg[128] = {0};
            std::snprintf(msg, sizeof(msg), "Zynq 7000 Boot Header parsed. FSBL Exec: 0x%08X\n",
                          static_cast<unsigned int>(img.bootloader_exec_address));
            logger(msg);
        }

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

        if (actual_iht_offset != 0 && actual_iht_offset != 0xFFFFFFFF) {
            zynq7000::ImageHeaderTable iht;
            if (reader.read_bytes(actual_iht_offset, &iht, sizeof(iht))) {
                uint32_t ph_offset = iht.first_partition_header_offset * 4;
                uint32_t count = 0;
                while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
                    zynq7000::PartitionHeader ph;
                    if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) break;
                    
                    if ((ph.unencrypted_partition_length == 0 && ph.total_partition_word_length == 0) || 
                        ph.unencrypted_partition_length == 0xFFFFFFFF) break;

                    PartitionInfo pinfo;
                    pinfo.load_address = ph.destination_load_address;
                    pinfo.exec_address = ph.destination_execution_address;
                    pinfo.data_offset = ph.data_word_offset * 4;
                    pinfo.data_size = ph.unencrypted_partition_length * 4;
                    pinfo.name = unpack_image_name(reader, ph.image_header_word_offset * 4);
                    if (pinfo.name.empty()) pinfo.name = "PART_" + std::to_string(count);

                    img.partitions.push_back(pinfo);
                    ph_offset += sizeof(zynq7000::PartitionHeader);
                    count++;
                }
            }
        }
    } 
    else if (img.arch == Arch::ZynqMP) {
        zynqmp::BootHeader bh;
        if (!reader.read_bytes(0, &bh, sizeof(bh))) return img;

        img.bootloader_exec_address = bh.fsbl_execution_address;
        img.bootloader_load_address = bh.fsbl_execution_address;
        img.bootloader_offset = bh.source_offset;
        img.bootloader_size = bh.fsbl_image_length;

        if (logger) {
            char msg[128] = {0};
            std::snprintf(msg, sizeof(msg), "ZynqMP Boot Header parsed. FSBL Exec: 0x%08X\n",
                          static_cast<unsigned int>(img.bootloader_exec_address));
            logger(msg);
        }

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

        if (actual_iht_offset != 0 && actual_iht_offset != 0xFFFFFFFF) {
            zynqmp::ImageHeaderTable iht;
            if (reader.read_bytes(actual_iht_offset, &iht, sizeof(iht))) {
                uint32_t ph_offset = iht.first_partition_header_offset * 4;
                uint32_t count = 0;
                while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
                    zynqmp::PartitionHeader ph;
                    if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) break;
                    
                    if ((ph.unencrypted_data_word_length == 0 && ph.total_partition_word_length == 0) || 
                        ph.unencrypted_data_word_length == 0xFFFFFFFF) break;

                    PartitionInfo pinfo;
                    pinfo.load_address = (static_cast<uint64_t>(ph.destination_load_address_hi) << 32) | ph.destination_load_address_lo;
                    pinfo.exec_address = (static_cast<uint64_t>(ph.destination_execution_address_hi) << 32) | ph.destination_execution_address_lo;
                    pinfo.data_offset = ph.actual_partition_word_offset * 4;
                    pinfo.data_size = ph.unencrypted_data_word_length * 4;
                    pinfo.name = unpack_image_name(reader, ph.image_header_word_offset * 4);
                    if (pinfo.name.empty()) pinfo.name = "PART_" + std::to_string(count);

                    img.partitions.push_back(pinfo);
                    ph_offset = ph.next_partition_header_offset * 4;
                    count++;
                }
            }
        }
    }
    else if (img.arch == Arch::PDI || img.arch == Arch::VersalGen1) {
        versal::BootHeader bh;
        if (!reader.read_bytes(0, &bh, sizeof(bh))) return img;

        if (logger) logger("PDI Boot Header parsed. PLM Exec: 0xF0280000\n");

        if (bh.plm_length > 0 && bh.plm_length != 0xFFFFFFFF) {
            PartitionInfo plm;
            plm.load_address = 0xF0280000;
            plm.exec_address = 0xF0280000;
            plm.data_offset = bh.plm_source_offset;
            plm.data_size = bh.plm_length;
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

        if (bh.meta_header_offset != 0 && bh.meta_header_offset != 0xFFFFFFFF) {
            versal::ImageHeaderTable iht;
            if (reader.read_bytes(bh.meta_header_offset, &iht, sizeof(iht))) {
                uint32_t ph_offset = iht.partition_header_offset * 4;
                uint32_t count = 0;
                while (ph_offset != 0 && ph_offset != 0xFFFFFFFF && count < 32) {
                    versal::PartitionHeader ph;
                    if (!reader.read_bytes(ph_offset, &ph, sizeof(ph))) break;
                    
                    if ((ph.unencrypted_data_word_length == 0 && ph.total_partition_word_length == 0) || 
                        ph.unencrypted_data_word_length == 0xFFFFFFFF) break;

                    PartitionInfo pinfo;
                    pinfo.load_address = (static_cast<uint64_t>(ph.destination_load_address_hi) << 32) | ph.destination_load_address_lo;
                    pinfo.exec_address = (static_cast<uint64_t>(ph.destination_execution_address_hi) << 32) | ph.destination_execution_address_lo;
                    pinfo.data_offset = ph.actual_partition_word_offset * 4;
                    pinfo.data_size = ph.unencrypted_data_word_length * 4;
                    pinfo.name = "PDI_PART_" + std::to_string(count);

                    img.partitions.push_back(pinfo);
                    ph_offset = ph.next_partition_header_offset * 4;
                    count++;
                }
            }
        }
    }

    return img;
}

} // namespace xilinx
