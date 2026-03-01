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

void test_unpack_name() {
    MemoryReader reader(128);
    uint8_t packed[] = { 'L', 'B', 'S', 'F', 'E', '.', '0', '1', '\0', '\0', 'F', 'L', 0,0,0,0 };
    std::memcpy(reader.data.data() + 0x10, packed, sizeof(packed));
    
    std::string name = unpack_image_name(reader, 0);
    assert(name == "FSBL10.ELF");
    std::cout << "[OK] unpack_name" << std::endl;
}

void test_zynq7000_detection() {
    MemoryReader reader(1024);
    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x20);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58; // XNLX
    words[3] = 0x01010000; // version
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::Zynq7000);
    assert(img.format_name == "Xilinx Zynq 7000 Boot Image");
    assert(img.load_supported);
    assert(img.processor_name == "arm");
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::AArch32);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::High);
    assert(img.processor_selection.source == "arch_default:zynq7000_boot_header");
    std::cout << "[OK] zynq7000_detection" << std::endl;
}

void test_zynqmp_detection() {
    MemoryReader reader(1024);
    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x20);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58; // XNLX
    words[3] = 0x0; // Not 01010000
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.format_name == "Xilinx Zynq UltraScale+ MPSoC Boot Image");
    assert(img.load_supported);
    assert(img.processor_name == "arm");
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::Medium);
    assert(img.processor_selection.source == "arch_default:zynqmp_without_partition_attr_decode");
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
    assert(!img.partitions.empty());
    assert(img.partitions[0].name == "PMUFW");
    assert(img.partitions[0].processor_family == ProcessorFamily::MicroBlaze);
    assert(img.partitions[0].destination_cpu == DestinationCpu::PMU);
    assert(img.partitions[0].arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.partitions[0].load_address == 0xFFDC0000ULL);
    assert(img.partitions[0].data_offset == 0x400);
    assert(img.partitions[0].data_size == 0x80);

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
    bh->plm_source_offset = 0x200;
    bh->plm_length = 0x40;
    bh->total_plm_length = 0x40;
    write_u32(0xC4, 0x1000); // meta header offset (bytes)

    write_u32(0x1000 + 0x00, 0x00040000); // Gen1 IHT version
    write_u32(0x1000 + 0x04, 1);          // total images
    write_u32(0x1000 + 0x08, 0x300 / 4);  // image header offset (word)
    write_u32(0x1000 + 0x0C, 1);          // total partitions
    write_u32(0x1000 + 0x10, 0x200 / 4);  // partition header offset (word)
    write_u32(0x1000 + 0x28, 0x49445046); // FPDI
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::VersalGen1);
    assert(img.format_name == "Xilinx Versal Adaptive SoC Gen 1 PDI");
    assert(img.load_supported);
    assert(img.processor_name == "mblaze");
    assert(img.processor_selection.family == ProcessorFamily::MicroBlaze);
    assert(img.processor_selection.arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.processor_selection.confidence == ProcessorInferenceConfidence::High);
    assert(img.processor_selection.source == "partition_context:versal_plm_ppu_microblaze_default");
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
    ph2->attributes = (2u << 8) | (1u << 3); // A53-1, AArch32

    auto img = parse_image(reader);
    assert(img.arch == Arch::ZynqMP);
    assert(img.partitions.size() == 2);
    assert(img.processor_selection.family == ProcessorFamily::Arm);
    assert(img.processor_selection.source.find("mixed_policy:") == 0);
    assert(img.partitions[0].destination_cpu == DestinationCpu::PMU);
    assert(img.partitions[0].processor_family == ProcessorFamily::MicroBlaze);
    assert(img.partitions[0].arm_bitness_hint == ArmBitnessHint::Unknown);
    assert(img.partitions[1].destination_cpu == DestinationCpu::A53_1);
    assert(img.partitions[1].processor_family == ProcessorFamily::Arm);
    assert(img.partitions[1].arm_bitness_hint == ArmBitnessHint::AArch32);

    std::cout << "[OK] zynqmp_partition_attr_decode" << std::endl;
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
    
    auto* ph1 = reinterpret_cast<versal::PartitionHeader*>(reader.data.data() + 0x200);
    ph1->unencrypted_data_word_length = 0x400 / 4;
    ph1->destination_load_address_lo = 0xBBBBBBBB;
    ph1->destination_load_address_hi = 0xAAAAAAAA;
    ph1->destination_execution_address_lo = 0xDDDDDDDD;
    ph1->destination_execution_address_hi = 0xCCCCCCCC;
    ph1->actual_partition_word_offset = 0x800 / 4;
    ph1->attributes = (2u << 8) | (1u << 3); // A72-1, AArch32
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
    
    assert(img.partitions[2].name == "PDI_PART_0");
    assert(img.partitions[2].destination_cpu == DestinationCpu::A72_1);
    assert(img.partitions[2].processor_family == ProcessorFamily::Arm);
    assert(img.partitions[2].arm_bitness_hint == ArmBitnessHint::AArch32);
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
    test_versal_detection();
    test_spartan_detection();
    test_versal_gen2_detection();
    test_weak_pdi_rejected();
    test_zynq7000_partitions();
    test_versal_partitions();
    
    std::cout << "All headless tests passed!" << std::endl;
    return 0;
}
