#include <ida/idax.hpp>
#include <ida/loader.hpp>
#include <ida/segment.hpp>
#include <ida/name.hpp>
#include <ida/entry.hpp>
#include <ida/ui.hpp>

#include "parser.hpp"

using namespace ida;

class IdaReader : public xilinx::Reader {
    ida::loader::InputFile& file;
public:
    explicit IdaReader(ida::loader::InputFile& f) : file(f) {}
    bool read_bytes(uint64_t offset, void* buffer, size_t size) override {
        auto res = file.read_bytes_at(offset, size);
        if (!res || res->size() < size) return false;
        std::memcpy(buffer, res->data(), size);
        return true;
    }
};

class XilinxBootLoader : public ida::loader::Loader {
public:
    ida::Result<std::optional<ida::loader::AcceptResult>> accept(ida::loader::InputFile& file) override {
        IdaReader reader(file);
        auto img = xilinx::parse_image(reader);
        
        if (img.arch != xilinx::Arch::Unknown) {
            return ida::loader::AcceptResult{
                .format_name = img.format_name,
                .processor_name = img.processor_name
            };
        }
        return std::nullopt;
    }

    ida::Status load(ida::loader::InputFile& file, std::string_view format_name) override {
        IdaReader reader(file);
        
        auto logger = [](const std::string& msg) {
            ida::ui::message(msg);
        };
        
        auto img = xilinx::parse_image(reader, logger);

        ida::loader::set_processor(img.processor_name);

        if (img.bootloader_size > 0 && img.bootloader_load_address != 0xFFFFFFFF) {
            ida::segment::create(
                static_cast<ida::Address>(img.bootloader_load_address),
                static_cast<ida::Address>(img.bootloader_load_address + img.bootloader_size),
                "FSBL", "CODE", ida::segment::Type::Code
            );
            ida::loader::file_to_database(file.handle(), img.bootloader_offset, static_cast<ida::Address>(img.bootloader_load_address), img.bootloader_size, true);
            if (img.bootloader_exec_address != 0 && img.bootloader_exec_address != 0xFFFFFFFF) {
                ida::entry::add(1, static_cast<ida::Address>(img.bootloader_exec_address), "fsbl_entry");
            }
        }

        uint32_t count = 2;
        for (const auto& part : img.partitions) {
            ida::ui::message(" - " + part.name + ": Load=0x" + std::to_string(part.load_address) + 
                             " Size=0x" + std::to_string(part.data_size) + "\n");
                             
            if (part.data_size > 0 && part.load_address != 0xFFFFFFFF) {
                ida::segment::create(
                    static_cast<ida::Address>(part.load_address),
                    static_cast<ida::Address>(part.load_address + part.data_size),
                    part.name, "CODE", ida::segment::Type::Code
                );
                ida::loader::file_to_database(file.handle(), part.data_offset, static_cast<ida::Address>(part.load_address), part.data_size, true);
                if (part.exec_address != 0 && part.exec_address != 0xFFFFFFFF) {
                    ida::entry::add(count, static_cast<ida::Address>(part.exec_address), part.name + "_entry");
                }
            }
            count++;
        }

        ida::loader::create_filename_comment();
        return ida::ok();
    }
};

IDAX_LOADER(XilinxBootLoader)
