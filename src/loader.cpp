#include <ida/idax.hpp>
#include <ida/loader.hpp>
#include <ida/segment.hpp>
#include <ida/database.hpp>
#include <ida/name.hpp>
#include <ida/entry.hpp>
#include <ida/ui.hpp>

#include "parser.hpp"

using namespace ida;

// Resolve the segment bitness (16/32/64) for a given processor family and ARM bitness hint.
static int resolve_segment_bitness(xilinx::ProcessorFamily family,
                                   xilinx::ArmBitnessHint arm_hint) {
    if (family == xilinx::ProcessorFamily::Arm) {
        if (arm_hint == xilinx::ArmBitnessHint::AArch64) return 64;
        return 32; // AArch32 or Unknown defaults to 32-bit for ARM
    }
    // MicroBlaze and other families are 32-bit
    return 32;
}

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
        
        if (img.arch != xilinx::Arch::Unknown && img.load_supported) {
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

        if (img.arch == xilinx::Arch::Unknown) {
            return std::unexpected(ida::Error::unsupported("Unsupported or unrecognized Xilinx image."));
        }
        if (!img.load_supported) {
            std::string msg = "Detected " + img.format_name + " but safe loading is disabled for this family.";
            if (!img.warnings.empty()) {
                msg += " " + img.warnings.front();
            }
            return std::unexpected(ida::Error::unsupported(msg));
        }

        for (const auto& security_warning : img.security_warnings) {
            ida::ui::message("SECURITY WARNING: " + security_warning + "\n");
        }

        auto proc_status = ida::loader::set_processor(img.processor_name);
        if (!proc_status) {
            return std::unexpected(proc_status.error());
        }

        // Set database-level address bitness based on the image's processor
        // selection. This must happen after set_processor() because the ARM
        // processor module defaults to 64-bit, but Zynq-7000 is AArch32.
        // Without this, IDA loads the 64-bit decompiler even for 32-bit ARM code.
        int db_bitness = resolve_segment_bitness(
            img.processor_selection.family,
            img.processor_selection.arm_bitness_hint);
        ida::database::set_address_bitness(db_bitness);

        if (img.bootloader_size > 0 && img.bootloader_load_address != 0xFFFFFFFF) {
            ida::segment::create(
                static_cast<ida::Address>(img.bootloader_load_address),
                static_cast<ida::Address>(img.bootloader_load_address + img.bootloader_size),
                "FSBL", "CODE", ida::segment::Type::Code
            );
            int fsbl_bits = resolve_segment_bitness(
                img.processor_selection.family,
                img.processor_selection.arm_bitness_hint);
            ida::segment::set_bitness(
                static_cast<ida::Address>(img.bootloader_load_address), fsbl_bits);
            ida::loader::file_to_database(file.handle(), img.bootloader_offset, static_cast<ida::Address>(img.bootloader_load_address), img.bootloader_size, true);
            // Note: exec address 0x0 is valid for Zynq7000 (OCM base).
            if (img.bootloader_exec_address != 0xFFFFFFFF) {
                ida::entry::add(1, static_cast<ida::Address>(img.bootloader_exec_address), "fsbl_entry");
            }
        }

        uint32_t count = 2;
        for (const auto& part : img.partitions) {
            if (part.is_bootloader_partition) {
                count++;
                continue;
            }

            const bool auth_blob_overlaps_payload =
                xilinx::partition_payload_overlaps_auth_certificate(part);
            if (auth_blob_overlaps_payload) {
                ida::ui::message("SECURITY WARNING: Skipping partition '" + part.name +
                                 "' because payload offset points at authentication-certificate metadata.\n");
                count++;
                continue;
            }

            const bool encrypted_payload = part.is_encrypted;

            ida::ui::message(" - " + part.name + ": Load=0x" + std::to_string(part.load_address) + 
                             " Size=0x" + std::to_string(part.data_size) + "\n");
                              
            if (part.data_size > 0 && part.load_address == 0xFFFFFFFFULL) {
                ida::ui::message("WARNING: Skipping partition '" + part.name +
                                 "' due invalid load address 0xFFFFFFFF.\n");
                count++;
                continue;
            }

            // Skip PL/configuration partitions with load_address=0 (no valid CPU address).
            if (part.data_size > 0 && part.load_address == 0 &&
                (part.destination_device == xilinx::DestinationDevice::PL ||
                 xilinx::is_configuration_partition_type_external(part.partition_type))) {
                ida::ui::message("INFO: Skipping PL/configuration partition '" + part.name +
                                 "' (no CPU load address).\n");
                count++;
                continue;
            }

            // Skip partitions that would overlap with the FSBL segment already mapped from BH.
            if (img.bootloader_size > 0 && part.data_size > 0 &&
                part.load_address == img.bootloader_load_address &&
                part.data_offset == img.bootloader_offset) {
                ida::ui::message("INFO: Skipping partition '" + part.name +
                                 "' (duplicate of FSBL bootloader segment).\n");
                count++;
                continue;
            }

            if (part.data_size > 0 && part.load_address != 0xFFFFFFFFULL) {
                const bool map_as_code = xilinx::partition_should_map_as_code(part);
                const bool executable_cpu_partition = xilinx::partition_is_executable_cpu(part);
                const bool has_exec_address = part.exec_address != 0 && part.exec_address != 0xFFFFFFFFULL;
                const char* segment_class = map_as_code ? "CODE" : "DATA";
                const auto segment_type = map_as_code ? ida::segment::Type::Code : ida::segment::Type::Data;
                ida::segment::create(
                    static_cast<ida::Address>(part.load_address),
                    static_cast<ida::Address>(part.load_address + part.data_size),
                    part.name, segment_class, segment_type
                );
                int part_bits = resolve_segment_bitness(
                    part.processor_family, part.arm_bitness_hint);
                ida::segment::set_bitness(
                    static_cast<ida::Address>(part.load_address), part_bits);
                ida::loader::file_to_database(file.handle(), part.data_offset, static_cast<ida::Address>(part.load_address), part.data_size, true);
                if (!executable_cpu_partition && has_exec_address) {
                    ida::ui::message("WARNING: Not creating entry point for non-executable partition '" +
                                     part.name + "'.\n");
                } else if (encrypted_payload && has_exec_address) {
                    ida::ui::message("SECURITY WARNING: Not creating entry point for encrypted partition '" +
                                     part.name + "'.\n");
                } else if (executable_cpu_partition) {
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
