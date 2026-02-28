#include "parser.hpp"
#include <format>
#include <ctype.h>

namespace xilinx {

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
    uint32_t zynq_magic[4] = {0};
    reader.read_bytes(0x20, zynq_magic, 16);
    
    if (zynq_magic[0] == 0xAA995566 && check_magic(zynq_magic[1])) {
        if (zynq_magic[3] == 0x01010000) {
            img.arch = Arch::Zynq7000;
            img.format_name = "Xilinx Zynq 7000 Boot Image";
            img.processor_name = "arm";
        } else {
            img.arch = Arch::ZynqMP;
            img.format_name = "Xilinx Zynq UltraScale+ Boot Image";
            img.processor_name = "arm";
        }
    } else {
        uint32_t pdi_magic[2] = {0};
        reader.read_bytes(0x10, pdi_magic, 8);
        if (pdi_magic[0] == 0xAA995566 && check_magic(pdi_magic[1])) {
            img.arch = Arch::PDI;
            img.format_name = "Xilinx Versal/Spartan PDI";
            img.processor_name = "arm";
        }
    }

    if (img.arch == Arch::Unknown) return img;

    if (img.arch == Arch::Zynq7000) {
        zynq7000::BootHeader bh;
        if (!reader.read_bytes(0, &bh, sizeof(bh))) return img;

        img.bootloader_exec_address = bh.fsbl_execution_address;
        img.bootloader_load_address = bh.fsbl_load_address;
        img.bootloader_offset = bh.source_offset;
        img.bootloader_size = bh.fsbl_image_length;

        if (logger) logger(std::format("Zynq 7000 Boot Header parsed. FSBL Exec: 0x{:08X}\n", img.bootloader_exec_address));

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

        if (logger) logger(std::format("ZynqMP Boot Header parsed. FSBL Exec: 0x{:08X}\n", img.bootloader_exec_address));

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
    else if (img.arch == Arch::PDI) {
        versal::BootHeader bh;
        if (!reader.read_bytes(0, &bh, sizeof(bh))) return img;

        if (logger) logger(std::format("PDI Boot Header parsed. PLM Exec: 0xF0280000\n"));

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
