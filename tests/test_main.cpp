#include "parser.hpp"
#include <vector>
#include <iostream>
#include <cstring>
#include <cassert>

using namespace xilinx;

struct MemoryReader : public Reader {
    std::vector<uint8_t> data;
    
    MemoryReader(size_t size) : data(size, 0) {}
    
    bool read_bytes(uint64_t offset, void* buffer, size_t size) override {
        if (offset + size > data.size()) return false;
        std::memcpy(buffer, data.data() + offset, size);
        return true;
    }
};

const BootImageRange* find_range(const ParsedImage& img, const std::string& name) {
    for (const auto& range : img.boot_header.image_ranges) {
        if (range.name == name) {
            return &range;
        }
    }
    return nullptr;
}

const BootAttributeWord* find_boot_attr(const ParsedImage& img, const std::string& name) {
    for (const auto& attr : img.boot_header.boot_attributes) {
        if (attr.name == name) {
            return &attr;
        }
    }
    return nullptr;
}

const BootRegionDiagnostic* find_region_diag(const ParsedImage& img, const std::string& name) {
    for (const auto& diag : img.boot_header.region_diagnostics) {
        if (diag.name == name) {
            return &diag;
        }
    }
    return nullptr;
}

const PartitionInfo* find_partition(const ParsedImage& img, const std::string& name) {
    for (const auto& part : img.partitions) {
        if (part.name == name) {
            return &part;
        }
    }
    return nullptr;
}

bool has_warning_substring(const ParsedImage& img, const std::string& needle) {
    for (const auto& warning : img.warnings) {
        if (warning.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void test_unpack_name() {
    MemoryReader reader(128);
    uint8_t packed[] = { 'L', 'B', 'S', 'F', 'E', '.', '0', '1', '\0', '\0', 'F', 'L', 0,0,0,0 };
    std::memcpy(reader.data.data() + 0x10, packed, sizeof(packed));
    
    std::string name = unpack_image_name(reader, 0);
    assert(name == "FSBL10.ELF");
    std::cout << "[OK] unpack_name" << std::endl;
}

void test_zynq7000_detection() {
    MemoryReader reader(4096);
    auto* bh = reinterpret_cast<zynq7000::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->key_source = 0xA;
    bh->header_version = 0x01010000;
    bh->source_offset = 0x200;
    bh->fsbl_image_length = 0x20;
    bh->qspi_config_word = 0x12345678;
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::Zynq7000);
    assert(img.format_name == "Xilinx Zynq 7000 Boot Image");
    assert(img.load_supported);
    assert(img.processor_name == "arm");
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::AArch32);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::High);
    assert(img.processor_selection.source == "arch_default:zynq7000_boot_header");
    assert(img.boot_header.present);
    assert(img.boot_header.key_source_present);
    assert(img.boot_header.key_source == 0xA);
    const auto* fsbl_range = find_range(img, "FSBL");
    assert(fsbl_range != nullptr);
    assert(fsbl_range->bounds_valid);
    const auto* qspi_attr = find_boot_attr(img, "qspi_config_word");
    assert(qspi_attr != nullptr);
    assert(qspi_attr->value == 0x12345678);
    const auto* reg_diag = find_region_diag(img, "REGISTER_INIT");
    assert(reg_diag != nullptr);
    assert(reg_diag->present);
    assert(reg_diag->bounds_valid);
    assert(reg_diag->checksum_status == MetadataChecksumStatus::NotPresent);
    std::cout << "[OK] zynq7000_detection" << std::endl;
}

void test_zynqmp_detection() {
    MemoryReader reader(4096);
    auto* bh = reinterpret_cast<zynqmp::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->key_source = 0x7;
    bh->fsbl_execution_address = 0x08000000; // Not 0x01010000
    bh->source_offset = 0x200;
    bh->fsbl_image_length = 0x20;
    bh->fsbl_image_attributes = 0x123;
    bh->secure_header_iv[0] = 0x11111111;
    bh->secure_header_iv[1] = 0x22222222;
    bh->secure_header_iv[2] = 0x33333333;
    bh->obfuscated_black_key_iv[0] = 0xAAAAAAA1;
    bh->obfuscated_black_key_iv[1] = 0xAAAAAAA2;
    bh->obfuscated_black_key_iv[2] = 0xAAAAAAA3;
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.format_name == "Xilinx Zynq UltraScale+ MPSoC Boot Image");
    assert(img.load_supported);
    assert(img.processor_name == "arm");
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::Medium);
    assert(img.processor_selection.source == "arch_default:zynqmp_without_partition_attr_decode");
    assert(img.boot_header.present);
    assert(img.boot_header.key_source_present);
    assert(img.boot_header.key_source == 0x7);
    assert(img.boot_header.secure_header_iv_present);
    assert(img.boot_header.secure_header_iv.size() == 3);
    assert(img.boot_header.secure_header_iv[0] == 0x11111111);
    assert(img.boot_header.obfuscated_black_key_iv_present);
    assert(img.boot_header.obfuscated_black_key_iv.size() == 3);
    assert(img.boot_header.obfuscated_black_key_iv[2] == 0xAAAAAAA3);
    const auto* fsbl_attr = find_boot_attr(img, "fsbl_image_attributes");
    assert(fsbl_attr != nullptr);
    assert(fsbl_attr->value == 0x123);
    const auto* reg_diag = find_region_diag(img, "REGISTER_INIT");
    assert(reg_diag != nullptr);
    assert(reg_diag->present);
    assert(reg_diag->bounds_valid);
    const auto* puf_diag = find_region_diag(img, "PUF_HELPER_DATA");
    assert(puf_diag != nullptr);
    assert(puf_diag->present);
    assert(puf_diag->bounds_valid);
    std::cout << "[OK] zynqmp_detection" << std::endl;
}

void test_zynqmp_pmufw_prefix() {
    MemoryReader reader(4096);

    auto* bh = reinterpret_cast<zynqmp::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->fsbl_execution_address = 0x08000000;
    bh->source_offset = 0x400;
    bh->pmu_image_length = 0x80;
    bh->total_pmu_fw_length = 0xA0;
    bh->fsbl_image_length = 0x200;

    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.load_supported);
    assert(img.bootloader_offset == 0x4A0);
    assert(img.bootloader_size == 0x200);
    const auto* pmufw = find_partition(img, "PMUFW");
    assert(pmufw != nullptr);
    assert(pmufw->processor_family == ProcessorFamily::MicroBlaze);
    assert(pmufw->destination_cpu == DestinationCpu::PMU);
    assert(pmufw->arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(pmufw->load_address == 0xFFDC0000ULL);
    assert(pmufw->data_offset == 0x400);
    assert(pmufw->data_size == 0x80);
    const auto* fsbl = find_partition(img, "FSBL");
    assert(fsbl != nullptr);
    assert(fsbl->is_bootloader_partition);
    assert(fsbl->data_offset == 0x4A0);
    assert(fsbl->data_size == 0x200);

    std::cout << "[OK] zynqmp_pmufw_prefix" << std::endl;
}

void test_versal_detection() {
    MemoryReader reader(8192);
    auto write_u32 = [&](size_t off, uint32_t value) {
        *reinterpret_cast<uint32_t*>(reader.data.data() + off) = value;
    };

    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x10);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58; // XNLX
    auto* bh = reinterpret_cast<versal::BootHeader*>(reader.data.data());
    bh->key_source = 0x5;
    bh->plm_source_offset = 0x200;
    bh->plm_length = 0x40;
    bh->total_plm_length = 0x40;
    bh->attributes = 0xDEAD0001;
    bh->black_iv[0] = 0x100;
    bh->black_iv[1] = 0x101;
    bh->black_iv[2] = 0x102;
    bh->secure_header_iv[0] = 0x200;
    bh->secure_header_iv[1] = 0x201;
    bh->secure_header_iv[2] = 0x202;
    bh->secure_header_iv_pmc[0] = 0x300;
    bh->secure_header_iv_pmc[1] = 0x301;
    bh->secure_header_iv_pmc[2] = 0x302;
    write_u32(0xC4, 0x1000); // meta header offset (bytes)

    write_u32(0x1000 + 0x00, 0x00040000); // Gen1 IHT version
    write_u32(0x1000 + 0x04, 1);          // total images
    write_u32(0x1000 + 0x08, 0x300 / 4);  // image header offset (word)
    write_u32(0x1000 + 0x0C, 1);          // total partitions
    write_u32(0x1000 + 0x10, 0x200 / 4);  // partition header offset (word)
    write_u32(0x1000 + 0x28, 0x49445046); // FPDI
    write_u32(0x1000 + 0x58, 3);          // optional data length (words)

    const uint32_t opt_entry_header = (3u << 16) | 0x21u;
    const uint32_t opt_entry_data = 0x11223344u;
    const uint32_t opt_entry_checksum = opt_entry_header + opt_entry_data;
    write_u32(0x1080 + 0x00, opt_entry_header);
    write_u32(0x1080 + 0x04, opt_entry_data);
    write_u32(0x1080 + 0x08, opt_entry_checksum);
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::VersalGen1);
    assert(img.format_name == "Xilinx Versal Adaptive SoC Gen 1 PDI");
    assert(img.load_supported);
    assert(img.processor_name == "mblaze");
    assert(img.processor_selection.family == ProcessorFamily::MicroBlaze);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::High);
    assert(img.processor_selection.source == "partition_context:versal_plm_ppu_microblaze_default");
    assert(img.boot_header.present);
    assert(img.boot_header.key_source_present);
    assert(img.boot_header.key_source == 0x5);
    assert(img.boot_header.black_iv_present);
    assert(img.boot_header.black_iv.size() == 3);
    assert(img.boot_header.black_iv[2] == 0x102);
    assert(img.boot_header.secure_header_iv_present);
    assert(img.boot_header.secure_header_iv_aux_present);
    const auto* plm_range = find_range(img, "PLM");
    assert(plm_range != nullptr);
    assert(plm_range->bounds_valid);
    const auto* boot_attr = find_boot_attr(img, "boot_attributes");
    assert(boot_attr != nullptr);
    assert(boot_attr->value == 0xDEAD0001);
    const auto* reg_diag = find_region_diag(img, "REGISTER_INIT");
    assert(reg_diag != nullptr);
    assert(reg_diag->present);
    assert(reg_diag->bounds_valid);
    const auto* puf_diag = find_region_diag(img, "PUF_HELPER_DATA");
    assert(puf_diag != nullptr);
    assert(puf_diag->present);
    assert(puf_diag->bounds_valid);
    assert(img.boot_header.optional_data_entries.size() == 1);
    assert(img.boot_header.optional_data_entries[0].id == 0x21);
    assert(img.boot_header.optional_data_entries[0].size_words == 3);
    assert(img.boot_header.optional_data_entries[0].checksum_status == OptionalDataChecksumStatus::Valid);
    assert(img.boot_header.optional_data_entries[0].data_words.size() == 1);
    assert(img.boot_header.optional_data_entries[0].data_words[0] == 0x11223344u);
    std::cout << "[OK] versal_detection" << std::endl;
}

void test_zynqmp_partition_attr_decode() {
    MemoryReader reader(4096);

    auto* bh = reinterpret_cast<zynqmp::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->fsbl_execution_address = 0x08000000;
    bh->source_offset = 0x400;
    bh->pmu_image_length = 0;
    bh->fsbl_image_length = 0x100;
    bh->image_header_table_offset = 0x100;

    auto* iht = reinterpret_cast<zynqmp::ImageHeaderTable*>(reader.data.data() + 0x100);
    iht->version = 0x01010000;
    iht->first_partition_header_offset = 0x200 / 4;

    auto* ph1 = reinterpret_cast<zynqmp::PartitionHeader*>(reader.data.data() + 0x200);
    ph1->unencrypted_data_word_length = 0x10;
    ph1->total_partition_word_length = 0x10;
    ph1->next_partition_header_offset = 0x240 / 4;
    ph1->actual_partition_word_offset = 0x300 / 4;
    ph1->attributes = (8u << 8); // PMU

    auto* ph2 = reinterpret_cast<zynqmp::PartitionHeader*>(reader.data.data() + 0x240);
    ph2->unencrypted_data_word_length = 0x10;
    ph2->total_partition_word_length = 0x10;
    ph2->next_partition_header_offset = 0;
    ph2->actual_partition_word_offset = 0x340 / 4;
    ph2->attributes = (2u << 8) | (1u << 3) | (1u << 7) | (3u << 12) | (1u << 15); // A53-1, AArch32, encrypted, SHA3, auth
    ph2->ac_offset = 0x500 / 4;

    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.partitions.size() == 3);
    const auto* fsbl = find_partition(img, "FSBL");
    assert(fsbl != nullptr);
    assert(fsbl->is_bootloader_partition);
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.source.find("mixed_policy:") == 0);
    assert(img.partitions[1].destination_cpu == DestinationCpu::PMU);
    assert(img.partitions[1].processor_family == ProcessorFamily::MicroBlaze);
    assert(img.partitions[1].arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.partitions[2].destination_cpu == DestinationCpu::A53_1);
    assert(img.partitions[2].processor_family == ProcessorFamily::Arm);
    assert(img.partitions[2].arm_bitness_hint == ArmBitnessHint::AArch32);
    assert(img.partitions[2].is_encrypted);
    assert(img.partitions[2].has_auth_certificate);
    assert(img.partitions[2].checksum_type == PartitionChecksumType::Sha3);
    assert(img.partitions[2].hash_algo == PartitionHashAlgorithm::Sha3);
    assert(!img.partitions[2].security_warnings.empty());
    assert(!img.security_warnings.empty());

    std::cout << "[OK] zynqmp_partition_attr_decode" << std::endl;
}

void test_zynqmp_iht_checksum_warning() {
    MemoryReader reader(4096);

    auto* bh = reinterpret_cast<zynqmp::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->fsbl_execution_address = 0x08000000;
    bh->source_offset = 0x400;
    bh->fsbl_image_length = 0x100;
    bh->image_header_table_offset = 0x100;

    auto* iht = reinterpret_cast<zynqmp::ImageHeaderTable*>(reader.data.data() + 0x100);
    iht->version = 0x01010000;
    iht->first_partition_header_offset = 0x200 / 4;
    iht->checksum = 0x1; // intentionally invalid, but present

    auto* ph = reinterpret_cast<zynqmp::PartitionHeader*>(reader.data.data() + 0x200);
    ph->unencrypted_data_word_length = 0x10;
    ph->total_partition_word_length = 0x10;
    ph->next_partition_header_offset = 0;
    ph->actual_partition_word_offset = 0x300 / 4;

    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.load_supported);
    assert(has_warning_substring(img, "ZynqMP image header table checksum mismatch"));

    std::cout << "[OK] zynqmp_iht_checksum_warning" << std::endl;
}

void test_spartan_detection() {
    MemoryReader reader(8192);
    auto write_u32 = [&](size_t off, uint32_t value) {
        *reinterpret_cast<uint32_t*>(reader.data.data() + off) = value;
    };

    write_u32(0x10, 0xAA995566);
    write_u32(0x14, 0x584C4E58);
    write_u32(0x1C, 0x400);      // source offset (bytes)
    write_u32(0x2C, 0x100);      // PLM length
    write_u32(0x30, 0x180);      // total PLM length
    write_u32(0x33C, 0xA5A5A5A5);// checksum slot present
    write_u32(0xC4, 0x24);       // user-defined revision field, not meta offset

    auto img = parse_image(reader);
    assert(img.arch == Arch::SpartanUltraScalePlus);
    assert(img.format_name == "Xilinx Spartan UltraScale+ PDI");
    assert(!img.load_supported);
    assert(img.processor_name.empty());
    assert(img.processor_selection.family == ProcessorFamily::Unknown);
    assert(!img.warnings.empty());
    std::cout << "[OK] spartan_detection" << std::endl;
}

void test_versal_gen2_detection() {
    MemoryReader reader(0x50000);
    auto write_u32 = [&](size_t off, uint32_t value) {
        *reinterpret_cast<uint32_t*>(reader.data.data() + off) = value;
    };

    write_u32(0x10, 0xAA995566);
    write_u32(0x14, 0x584C4E58);
    write_u32(0x1C, 0x1200);      // source offset
    write_u32(0x28, 0xA0);        // total PMC data length
    write_u32(0x2C, 0x200);       // PLM length
    write_u32(0x30, 0x280);       // total PLM length
    write_u32(0x113C, 0xDEADBEEF);// checksum slot present

    const uint32_t iht_offset = 0x1200 + 0x280 + 0xA0;
    write_u32(iht_offset + 0x00, 0x00010000); // Gen2 IHT version
    write_u32(iht_offset + 0x04, 1);          // total images
    write_u32(iht_offset + 0x08, 0x40);       // image header offset
    write_u32(iht_offset + 0x0C, 1);          // total partitions
    write_u32(iht_offset + 0x10, 0x60);       // partition header offset
    write_u32(iht_offset + 0x28, 0x49445046); // FPDI
    write_u32(iht_offset + 0x2C, 0x00202020); // non-zero header sizes

    auto img = parse_image(reader);
    assert(img.arch == Arch::VersalGen2);
    assert(img.format_name == "Xilinx Versal AI Edge/Prime Gen 2 PDI");
    assert(!img.load_supported);
    assert(img.processor_name.empty());
    assert(img.processor_selection.family == ProcessorFamily::Unknown);
    assert(!img.warnings.empty());
    std::cout << "[OK] versal_gen2_detection" << std::endl;
}

void test_weak_pdi_rejected() {
    MemoryReader reader(1024);
    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x10);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58;

    auto img = parse_image(reader);
    assert(img.arch == Arch::Unknown);
    assert(!img.load_supported);
    assert(img.processor_name.empty());
    assert(img.processor_selection.family == ProcessorFamily::Unknown);
    std::cout << "[OK] weak_pdi_rejected" << std::endl;
}

void test_zynq7000_partitions() {
    MemoryReader reader(4096);
    // Boot Header
    auto* bh = reinterpret_cast<zynq7000::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->header_version = 0x01010000;
    bh->image_header_table_offset = 0x100 / 4; // Word offset -> 0x100
    bh->fsbl_execution_address = 0x11223344;
    
    // Image Header Table
    auto* iht = reinterpret_cast<zynq7000::ImageHeaderTable*>(reader.data.data() + 0x100);
    iht->version = 0x01010000;
    iht->first_partition_header_offset = 0x200 / 4; // -> 0x200
    
    // Partition Header 1
    auto* ph1 = reinterpret_cast<zynq7000::PartitionHeader*>(reader.data.data() + 0x200);
    ph1->unencrypted_partition_length = 0x400 / 4; // 1024 bytes
    ph1->destination_load_address = 0x10000000;
    ph1->destination_execution_address = 0x10000000;
    ph1->data_word_offset = 0x1000 / 4;
    ph1->image_header_word_offset = 0x800 / 4;
    
    // Set image name for PH1 at 0x800
    uint8_t packed[] = { 'L', 'B', 'S', 'F', 'E', '.', '0', '1', '\0', '\0', 'F', 'L', 0,0,0,0 };
    std::memcpy(reader.data.data() + 0x810, packed, sizeof(packed));

    // Partition Header 2 (end)
    auto* ph2 = reinterpret_cast<zynq7000::PartitionHeader*>(reader.data.data() + 0x200 + sizeof(zynq7000::PartitionHeader));
    ph2->unencrypted_partition_length = 0;
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::Zynq7000);
    assert(img.load_supported);
    assert(img.bootloader_exec_address == 0x11223344);
    assert(img.partitions.size() == 1);
    assert(img.partitions[0].name == "FSBL10.ELF");
    assert(img.partitions[0].load_address == 0x10000000);
    assert(img.partitions[0].data_size == 1024);
    
    std::cout << "[OK] zynq7000_partitions" << std::endl;
}

int main_old() {
    test_unpack_name();
    test_zynq7000_detection();
    test_zynqmp_detection();
    test_versal_detection();
    test_zynq7000_partitions();
    
    std::cout << "All headless tests passed!" << std::endl;
    return 0;
}

void test_versal_partitions() {
    MemoryReader reader(0x5000);
    auto* bh = reinterpret_cast<versal::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->plm_source_offset = 0x1000;
    bh->plm_length = 0x2000;
    bh->pmc_data_load_address = 0x11223344;
    bh->pmc_data_length = 0x500;
    bh->total_plm_length = 0x2100;
    bh->meta_header_offset = 0x1000;
    
    auto* iht = reinterpret_cast<versal::ImageHeaderTable*>(reader.data.data() + 0x1000);
    iht->version = 0x00040000;
    iht->total_number_of_images = 1;
    iht->image_header_offset = 0x300 / 4;
    iht->total_number_of_partitions = 1;
    iht->partition_header_offset = 0x200 / 4;
    iht->identification_string = 0x49445046; // FPDI

    auto* ih1 = reinterpret_cast<versal::ImageHeader*>(reader.data.data() + 0x300);
    ih1->first_partition_header_word_offset = 0x200 / 4;
    ih1->partition_count = 1;
    std::memcpy(ih1->image_name, "APP_CPU", 7);
    
    auto* ph1 = reinterpret_cast<versal::PartitionHeader*>(reader.data.data() + 0x200);
    ph1->unencrypted_data_word_length = 0x400 / 4;
    ph1->destination_load_address_lo = 0xBBBBBBBB;
    ph1->destination_load_address_hi = 0xAAAAAAAA;
    ph1->destination_execution_address_lo = 0xDDDDDDDD;
    ph1->destination_execution_address_hi = 0xCCCCCCCC;
    ph1->actual_partition_word_offset = 0x800 / 4;
    ph1->attributes = (2u << 8) | (1u << 3); // A72-1, AArch32
    ph1->encryption_key_select = 0xA5C3C5A3;
    ph1->hash_block_ac_offset = 0x700 / 4;
    ph1->next_partition_header_offset = 0; // End of list
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::VersalGen1);
    assert(img.load_supported);
    assert(img.processor_name == "mblaze");
    assert(img.processor_selection.family == ProcessorFamily::MicroBlaze);
    assert(img.processor_selection.source.find("mixed_policy:") == 0);
    assert(!img.warnings.empty());
    assert(img.warnings[0].find("Mixed-CPU image:") == 0);
    assert(img.partitions.size() == 3); // PLM + PMC + Partition
    
    assert(img.partitions[0].name == "PLM");
    assert(img.partitions[0].processor_family == ProcessorFamily::MicroBlaze);
    assert(img.partitions[0].load_address == 0xF0280000);
    assert(img.partitions[0].data_size == 0x2000);
    
    assert(img.partitions[1].name == "PMC_DATA");
    assert(img.partitions[1].load_address == 0x11223344);
    assert(img.partitions[1].data_size == 0x500);
    assert(img.partitions[1].data_offset == 0x1000 + 0x2100);
    
    assert(img.partitions[2].name == "APP_CPU");
    assert(img.partitions[2].destination_cpu == DestinationCpu::A72_1);
    assert(img.partitions[2].processor_family == ProcessorFamily::Arm);
    assert(img.partitions[2].arm_bitness_hint == ArmBitnessHint::AArch32);
    assert(img.partitions[2].is_encrypted);
    assert(img.partitions[2].has_auth_certificate);
    assert(!img.partitions[2].security_warnings.empty());
    assert(img.partitions[2].load_address == 0xAAAAAAAABBBBBBBBULL);
    assert(img.partitions[2].exec_address == 0xCCCCCCCCDDDDDDDDULL);
    assert(img.partitions[2].data_size == 0x400);
    
    std::cout << "[OK] versal_partitions" << std::endl;
}

int main() {
    test_unpack_name();
    test_zynq7000_detection();
    test_zynqmp_detection();
    test_zynqmp_pmufw_prefix();
    test_zynqmp_partition_attr_decode();
    test_zynqmp_iht_checksum_warning();
    test_versal_detection();
    test_spartan_detection();
    test_versal_gen2_detection();
    test_weak_pdi_rejected();
    test_zynq7000_partitions();
    test_versal_partitions();
    
    std::cout << "All headless tests passed!" << std::endl;
    return 0;
}
