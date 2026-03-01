#pragma once

#include <cstdint>

namespace xilinx {

inline bool check_magic(uint32_t magic) {
    // 0x584C4E58 in little-endian is 'X' 'N' 'L' 'X'
    return magic == 0x584C4E58;
}

namespace zynq7000 {
    struct BootHeader {
        uint32_t arm_vector_table[8];
        uint32_t width_detection_word; // 0x20
        uint32_t header_signature;     // 0x24 "XNLX"
        uint32_t key_source;           // 0x28
        uint32_t header_version;       // 0x2C 0x01010000
        uint32_t source_offset;        // 0x30
        uint32_t fsbl_image_length;    // 0x34
        uint32_t fsbl_load_address;    // 0x38
        uint32_t fsbl_execution_address; // 0x3C
        uint32_t total_fsbl_length;    // 0x40
        uint32_t qspi_config_word;     // 0x44
        uint32_t boot_header_checksum; // 0x48
        uint32_t user_defined_fields[19]; // 0x4C
        uint32_t image_header_table_offset; // 0x98
        uint32_t partition_header_table_offset; // 0x9C
    };

    struct RegisterInitTable {
        uint8_t data[0x800]; // 0xA0 - 0x89F
    };
    
    struct ImageHeaderTable {
        uint32_t version;               // 0x00
        uint32_t count_of_image_header; // 0x04
        uint32_t first_partition_header_offset; // 0x08
        uint32_t first_image_header_offset;     // 0x0C
        uint32_t header_authentication_certificate; // 0x10
    };

    struct ImageHeader {
        uint32_t next_image_header_offset;      // 0x00 (word offset)
        uint32_t corresponding_partition_header; // 0x04 (word offset)
        uint32_t reserved;                      // 0x08
        uint32_t partition_count;               // 0x0C
        uint32_t image_name_words[1];           // 0x10 (variable)
    };
    
    struct PartitionHeader {
        uint32_t encrypted_partition_length; // 0x00
        uint32_t unencrypted_partition_length; // 0x04
        uint32_t total_partition_word_length;  // 0x08
        uint32_t destination_load_address;     // 0x0C
        uint32_t destination_execution_address; // 0x10
        uint32_t data_word_offset;             // 0x14
        uint32_t attributes;                   // 0x18
        uint32_t section_count;                // 0x1C
        uint32_t checksum_word_offset;         // 0x20
        uint32_t image_header_word_offset;     // 0x24
        uint32_t ac_offset;                    // 0x28
        uint32_t reserved[4];                  // 0x2C - 0x38
        uint32_t header_checksum;              // 0x3C
    };
} // namespace zynq7000

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

    struct RegisterInitTable {
        uint8_t data[0x800]; // 0xB8 - 0x8B7
    };

    struct PufHelperData {
        uint8_t data[0x608]; // 0x8B8 - 0xF2F
    };

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
        uint32_t next_image_header_offset;      // 0x00 (word offset)
        uint32_t corresponding_partition_header; // 0x04 (word offset)
        uint32_t reserved;                      // 0x08
        uint32_t partition_count;               // 0x0C
        uint32_t image_name_words[1];           // 0x10 (variable)
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
} // namespace zynqmp

namespace versal {
    struct BootHeader {
        uint32_t smap_bus_width[4];    // 0x00
        uint32_t width_detection_word; // 0x10
        uint32_t header_signature;     // 0x14
        uint32_t key_source;           // 0x18
        uint32_t plm_source_offset;    // 0x1C
        uint32_t pmc_data_load_address; // 0x20
        uint32_t pmc_data_length;      // 0x24
        uint32_t total_pmc_data_length; // 0x28
        uint32_t plm_length;           // 0x2C
        uint32_t total_plm_length;     // 0x30
        uint32_t attributes;           // 0x34
        uint32_t black_key[8];         // 0x38
        uint32_t black_iv[3];          // 0x58
        uint32_t secure_header_iv[3];  // 0x64
        uint32_t puf_shutter_value;    // 0x70
        uint32_t secure_header_iv_pmc[3]; // 0x74
        uint32_t reserved1[17];        // 0x80
        uint32_t meta_header_offset;   // 0xC4
        uint32_t reserved2[24];        // 0xC8 - 0x124
    };

    struct RegisterInitTable {
        uint8_t data[0x800]; // 0x128 - 0x927
    };

    struct PufHelperData {
        uint8_t data[0x608]; // 0x928 - 0xF2F
    };
    
    struct ImageHeaderTable {
        uint32_t version;               // 0x00
        uint32_t total_number_of_images; // 0x04
        uint32_t image_header_offset;   // 0x08
        uint32_t total_number_of_partitions; // 0x0C
        uint32_t partition_header_offset; // 0x10
        uint32_t secondary_boot_device_address; // 0x14
        uint32_t id_code;               // 0x18 (Versal Edge) or reserved (Versal)
        uint32_t attributes;            // 0x1C
        uint32_t pdi_id;                // 0x20
        uint32_t parent_id;             // 0x24
        uint32_t identification_string; // 0x28
        uint32_t headers_size;          // 0x2C
        uint32_t total_meta_header_length; // 0x30
        uint32_t iv_meta_header[3];     // 0x34
        uint32_t encryption_status;     // 0x40
        uint32_t extended_id_code;      // 0x44 (Edge) or AC offset (Versal)
        uint32_t meta_header_ac_offset; // 0x48
        uint32_t meta_header_black_iv[3]; // 0x4C
        uint32_t optional_data_length;  // 0x58
        uint32_t authentication_header; // 0x5C
        uint32_t hash_block_length;     // 0x60
        uint32_t hash_block_offset;     // 0x64
        uint32_t total_ppk_size;        // 0x68
        uint32_t actual_ppk_size;       // 0x6C
        uint32_t total_hash_block_sig_size; // 0x70
        uint32_t actual_hash_block_sig_size; // 0x74
        uint32_t reserved3;             // 0x78
        uint32_t checksum;              // 0x7C
    };

    struct ImageHeader {
        uint32_t first_partition_header_word_offset; // 0x00
        uint32_t partition_count;                    // 0x04
        uint32_t revoke_id;                          // 0x08
        uint32_t image_attributes;                   // 0x0C
        char image_name[16];                         // 0x10-0x1C
        uint32_t image_node_id;                      // 0x20
        uint32_t unique_id;                          // 0x24
        uint32_t parent_unique_id;                   // 0x28
        uint32_t function_id;                        // 0x2C
        uint32_t copy_to_memory_address_low;         // 0x30
        uint32_t copy_to_memory_address_high;        // 0x34
        uint32_t reserved;                           // 0x38
        uint32_t checksum;                           // 0x3C
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
        uint32_t partition_id;                         // 0x30
        uint32_t hash_block_ac_offset;                 // 0x34
        uint32_t iv[3];                                // 0x38
        uint32_t encryption_key_select;                // 0x44
        uint32_t iv_kek_decryption[3];                 // 0x48
        uint32_t partition_revoke_id;                  // 0x54
        uint32_t measured_boot_address;                // 0x58
        uint32_t authentication_header;                // 0x5C
        uint32_t hash_block_length;                    // 0x60
        uint32_t hash_block_offset;                    // 0x64
        uint32_t total_ppk_size;                       // 0x68
        uint32_t actual_ppk_size;                      // 0x6C
        uint32_t total_hash_block_sig_size;            // 0x70
        uint32_t actual_hash_block_sig_size;           // 0x74
        uint32_t reserved;                             // 0x78
        uint32_t header_checksum;                      // 0x7C
    };
} // namespace versal

} // namespace xilinx
