#pragma once

#include <cstdint>

namespace zynqmp {

struct BootHeader {
    uint32_t arm_vector_table[8];
    uint32_t width_detection_word; // 0x20
    uint32_t header_signature;     // 0x24 "XNLX"
    uint32_t key_source;
    uint32_t fsbl_execution_address;
    uint32_t source_offset;
    uint32_t pmu_image_length;
    uint32_t total_pmu_fw_length;
    uint32_t fsbl_image_length;
    uint32_t total_fsbl_length;
    uint32_t fsbl_image_attributes;
    uint32_t boot_header_checksum;
    uint32_t obfuscated_black_key_storage[8];
    uint32_t shutter_value;
    uint32_t user_defined_fields[10]; // 0x70
    uint32_t image_header_table_offset; // 0x98
    uint32_t partition_header_table_offset; // 0x9C
    uint32_t secure_header_iv[3]; // 0xA0
    uint32_t obfuscated_black_key_iv[3]; // 0xAC
};

// Based on UG1283 documentation
struct ImageHeaderTable {
    uint32_t version;               // 0x00
    uint32_t count_of_image_header; // 0x04
    uint32_t first_partition_header_offset; // 0x08
    uint32_t first_image_header_offset;     // 0x0C
    uint32_t header_authentication_certificate; // 0x10
    uint32_t secondary_boot_device; // 0x14
    uint32_t padding[8];            // 0x18
    uint32_t checksum;              // 0x3C
};

struct ImageHeader {
    uint32_t next_image_header_offset; // 0x00
    uint32_t corresponding_partition_header; // 0x04
    uint32_t reserved1;                // 0x08
    uint32_t partition_count;          // 0x0C
    uint32_t image_name[1];            // 0x10 - variable length
};

struct PartitionHeader {
    uint32_t encrypted_partition_data_word_length; // 0x00
    uint32_t unencrypted_data_word_length;         // 0x04
    uint32_t total_partition_word_length;          // 0x08
    uint32_t next_partition_header_offset;         // 0x0C
    uint32_t destination_execution_address_lo;     // 0x10
    uint32_t destination_execution_address_hi;     // 0x14
    uint32_t destination_load_address_lo;          // 0x18
    uint32_t destination_load_address_hi;          // 0x1C
    uint32_t actual_partition_word_offset;         // 0x20
    uint32_t attributes;                           // 0x24
    uint32_t section_count;                        // 0x28
    uint32_t checksum_word_offset;                 // 0x2C
    uint32_t image_header_word_offset;             // 0x30
    uint32_t ac_offset;                            // 0x34
    uint32_t partition_number;                     // 0x38
    uint32_t header_checksum;                      // 0x3C
};

inline bool check_magic(uint32_t magic) {
    // 0x584C4E58 in little-endian is 'X' 'N' 'L' 'X'
    return magic == 0x584C4E58;
}

} // namespace zynqmp
