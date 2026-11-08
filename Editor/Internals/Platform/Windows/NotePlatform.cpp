#include "Editor/Internals/Platform/NotePlatform.h"
#include <Windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <stdexcept>
#include <system_error>

namespace {
void Fail(const char* message) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), message);
}
} // namespace
void RenameNote(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (!MoveFileW(from.c_str(), to.c_str())) Fail("Could not rename note");
}
void RecycleNote(const std::filesystem::path& path) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const auto check = [](HRESULT result) {
        if (FAILED(result))
            throw std::system_error(static_cast<int>(result), std::system_category(),
                                    "Could not send item to the Recycle Bin");
    };
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) check(initialized);
    try {
        Microsoft::WRL::ComPtr<IFileOperation> operation;
        check(CoCreateInstance(__uuidof(FileOperation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&operation)));
        // Explicit recycling: never silently fall back to permanent deletion.
        check(operation->SetOperationFlags(FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD | FOF_NO_UI | FOFX_EARLYFAILURE));
        Microsoft::WRL::ComPtr<IShellItem> item;
        check(SHCreateItemFromParsingName(std::filesystem::absolute(path).c_str(), nullptr, IID_PPV_ARGS(&item)));
        check(operation->DeleteItem(item.Get(), nullptr));
        check(operation->PerformOperations());
        BOOL aborted = FALSE;
        check(operation->GetAnyOperationsAborted(&aborted));
        if (aborted) throw std::runtime_error("The Recycle Bin operation was cancelled");
    } catch (...) {
        if (SUCCEEDED(initialized)) CoUninitialize();
        throw;
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}
