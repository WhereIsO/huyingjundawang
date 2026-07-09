#include "BackendAdapter.h"
#include "RealBackend.h"

namespace pbackup::ui {

std::unique_ptr<BackendAdapter> createBackend() {
    return createRealBackend();
}

} // namespace pbackup::ui
