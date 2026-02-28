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
    assert(img.format_name == "Xilinx Zynq UltraScale+ Boot Image");
    std::cout << "[OK] zynqmp_detection" << std::endl;
}

void test_versal_detection() {
    MemoryReader reader(1024);
    uint32_t* words = reinterpret_cast<uint32_t*>(reader.data.data() + 0x10);
    words[0] = 0xAA995566;
    words[1] = 0x584C4E58; // XNLX
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::PDI);
    assert(img.format_name == "Xilinx Versal/Spartan PDI");
    std::cout << "[OK] versal_detection" << std::endl;
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
    MemoryReader reader(4096);
    auto* bh = reinterpret_cast<versal::BootHeader*>(reader.data.data());
    bh->width_detection_word = 0xAA995566;
    bh->header_signature = 0x584C4E58;
    bh->plm_source_offset = 0x1000;
    bh->plm_length = 0x2000;
    bh->pmc_data_load_address = 0x11223344;
    bh->pmc_data_length = 0x500;
    bh->total_plm_length = 0x2100;
    bh->meta_header_offset = 0x100 / 4; // Word offset
    
    auto* iht = reinterpret_cast<versal::ImageHeaderTable*>(reader.data.data() + 0x100);
    iht->partition_header_offset = 0x200 / 4;
    
    auto* ph1 = reinterpret_cast<versal::PartitionHeader*>(reader.data.data() + 0x200);
    ph1->unencrypted_data_word_length = 0x400 / 4;
    ph1->destination_load_address_lo = 0xBBBBBBBB;
    ph1->destination_load_address_hi = 0xAAAAAAAA;
    ph1->destination_execution_address_lo = 0xDDDDDDDD;
    ph1->destination_execution_address_hi = 0xCCCCCCCC;
    ph1->actual_partition_word_offset = 0x800 / 4;
    ph1->next_partition_header_offset = 0; // End of list
    
    auto img = parse_image(reader);
    assert(img.arch == Arch::PDI);
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
    test_zynq7000_partitions();
    test_versal_partitions();
    
    std::cout << "All headless tests passed!" << std::endl;
    return 0;
}
