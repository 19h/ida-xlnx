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
    std::cout << "[OK] zynqmp_detection" << std::endl;
}

void test_versal_detection() {
    MemoryReader reader(8192);
    auto write_u32 = [&](size_t off, uint32_t value) {
        *reinterpret_cast<uint32_t*>(reader.data.data() + off) = value;
    };

    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x10);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58; // XNLX
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
    std::cout << "[OK] versal_detection" << std::endl;
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
    std::cout << "[OK] versal_gen2_detection" << std::endl;
}

void test_weak_pdi_rejected() {
    MemoryReader reader(1024);
    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x10);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58;

    auto img = parse_image(reader);
    assert(img.arch == Arch::Unknown);
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
    ph1->next_partition_header_offset = 0; // End of list
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::VersalGen1);
    assert(img.partitions.size() == 3); // PLM + PMC + Partition
    
    assert(img.partitions[0].name == "PLM");
    assert(img.partitions[0].load_address == 0xF0280000);
    assert(img.partitions[0].data_size == 0x2000);
    
    assert(img.partitions[1].name == "PMC_DATA");
    assert(img.partitions[1].load_address == 0x11223344);
    assert(img.partitions[1].data_size == 0x500);
    assert(img.partitions[1].data_offset == 0x1000 + 0x2100);
    
    assert(img.partitions[2].name == "PDI_PART_0");
    assert(img.partitions[2].load_address == 0xAAAAAAAABBBBBBBBULL);
    assert(img.partitions[2].exec_address == 0xCCCCCCCCDDDDDDDDULL);
    assert(img.partitions[2].data_size == 0x400);
    
    std::cout << "[OK] versal_partitions" << std::endl;
}

int main() {
    test_unpack_name();
    test_zynq7000_detection();
    test_zynqmp_detection();
    test_versal_detection();
    test_spartan_detection();
    test_versal_gen2_detection();
    test_weak_pdi_rejected();
    test_zynq7000_partitions();
    test_versal_partitions();
    
    std::cout << "All headless tests passed!" << std::endl;
    return 0;
}
