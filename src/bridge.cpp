#include <type_traits>
#include <pro.h>
#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <diskio.hpp>
#include <ida/loader.hpp>

// Forward declare the bridge
extern "C" void idax_loader_bridge_init(void** out_loader, void** out_input);

// Expose InputFile constructor
namespace ida::loader {
struct InputFileAccess {
    static InputFile wrap(linput_t* li) {
        InputFile f;
        f.handle_ = li;
        return f;
    }
};
}

int idaapi accept_file(qstring *fileformatname, qstring *processor, linput_t *li, const char *filename) {
    void* loader_ptr = nullptr;
    idax_loader_bridge_init(&loader_ptr, nullptr);
    if (!loader_ptr) {
        qeprintf("bridge: idax_loader_bridge_init failed\n");
        return 0;
    }
    
    auto* idax_loader = static_cast<ida::loader::Loader*>(loader_ptr);
    auto file = ida::loader::InputFileAccess::wrap(li);
    
    auto res = idax_loader->accept(file);
    if (res && res.value().has_value()) {
        auto val = res.value().value();
        *fileformatname = val.format_name.c_str();
        if (!val.processor_name.empty()) {
            *processor = val.processor_name.c_str();
        }
        return val.priority > 0 ? val.priority : ACCEPT_FIRST;
    }
    return 0;
}

void idaapi load_file(linput_t *li, ushort neflag, const char *fileformatname) {
    qeprintf("bridge: load_file called\n");
    void* loader_ptr = nullptr;
    idax_loader_bridge_init(&loader_ptr, nullptr);
    if (!loader_ptr) return;
    
    auto* idax_loader = static_cast<ida::loader::Loader*>(loader_ptr);
    auto file = ida::loader::InputFileAccess::wrap(li);
    
    // Call load
    idax_loader->load(file, fileformatname);
    qeprintf("bridge: load_file finished\n");
}

idaman loader_t ida_module_data LDSC;
loader_t LDSC = {
    IDP_INTERFACE_VERSION,
    0,
    accept_file,
    load_file,
    nullptr, // save_file
    nullptr, // move_segm
    nullptr  // process_archive
};
// append to bridge to just test
