// SPDX-License-Identifier: MIT
#include "audit.hpp"
#include "catalog.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <winver.h>
#include <softpub.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <limits>

namespace shiny::libraries {
namespace {
constexpr std::uint64_t maxFile = 128ull * 1024 * 1024;
constexpr std::uint64_t maxAudit = 512ull * 1024 * 1024;
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Algorithm {
    BCRYPT_ALG_HANDLE value = nullptr;
    Algorithm() {
        if (BCryptOpenAlgorithmProvider(&value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("sha256-unavailable");
    }
    ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
    Algorithm(const Algorithm&) = delete;
    Algorithm& operator=(const Algorithm&) = delete;
};
std::string asciiName(const std::filesystem::path& path) {
    auto name = path.filename().wstring();
    std::string result;
    for (wchar_t c : name) {
        if (c > 127) throw std::runtime_error("unrecognized-library-name");
        result += static_cast<char>(c);
    }
    return result;
}
// Reject UNC/device/relative/ADS paths and reparse components. No elevation, no drive scan.
void localPath(const std::filesystem::path& path, bool directory) {
    const auto s = path.wstring();
    if (s.size() < 3 || s.size() > 30000 || !path.is_absolute()
        || !((s[0] >= L'A' && s[0] <= L'Z') || (s[0] >= L'a' && s[0] <= L'z'))
        || s[1] != L':' || (s[2] != L'\\' && s[2] != L'/')
        || s.find(L':', 2) != std::wstring::npos)
        throw std::runtime_error("absolute-local-drive-path-required");
    if (GetDriveTypeW(path.root_path().c_str()) == DRIVE_REMOTE)
        throw std::runtime_error("network-drive-not-supported");
    std::filesystem::path part = path.root_path();
    for (const auto& item : path.relative_path()) {
        const auto text = item.wstring();
        if (text.empty()) continue;
        if (text == L"." || text == L".." || text.back() == L'.' || text.back() == L' '
            || text.find_first_of(L"*?<>|\"") != std::wstring::npos)
            throw std::runtime_error("ambiguous-path-not-supported");
        part /= item;
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const auto error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                throw std::runtime_error("missing");
            throw std::runtime_error("path-unreadable");
        }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("reparse-point-rejected");
    }
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("path-unreadable");
    if (bool(attributes & FILE_ATTRIBUTE_DIRECTORY) != directory)
        throw std::runtime_error(directory ? "directory-required" : "regular-file-required");
}
std::string hash(const std::vector<std::uint8_t>& bytes) {
    Algorithm algorithm;
    std::array<UCHAR, 32> digest{};
    if (BCryptHash(algorithm.value, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
        static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size())) < 0)
        throw std::runtime_error("sha256-failed");
    std::ostringstream result;
    for (auto c : digest) result << std::hex << std::setw(2) << std::setfill('0') << unsigned(c);
    return result.str();
}
std::string version(const std::filesystem::path& file) {
    DWORD unused = 0;
    auto size = GetFileVersionInfoSizeExW(FILE_VER_GET_NEUTRAL, file.c_str(), &unused);
    if (!size || size > 1024 * 1024) return "unavailable";
    std::vector<std::uint8_t> resource(size);
    if (!GetFileVersionInfoExW(FILE_VER_GET_NEUTRAL, file.c_str(), 0, size, resource.data())) return "unavailable";
    VS_FIXEDFILEINFO* fixed = nullptr; UINT length = 0;
    if (!VerQueryValueW(resource.data(), L"\\", reinterpret_cast<void**>(&fixed), &length)
        || !fixed || length < sizeof(*fixed) || fixed->dwSignature != 0xfeef04bd) return "unavailable";
    return std::to_string(HIWORD(fixed->dwFileVersionMS)) + "." + std::to_string(LOWORD(fixed->dwFileVersionMS))
        + "." + std::to_string(HIWORD(fixed->dwFileVersionLS)) + "." + std::to_string(LOWORD(fixed->dwFileVersionLS));
}
// Cache-only chain check: NEVER represented as publisher approval or proof of licensing.
std::string signature(const std::filesystem::path& file, HANDLE handle) {
    WINTRUST_FILE_INFO info{}; info.cbStruct = sizeof(info); info.pcwszFilePath = file.c_str(); info.hFile = handle;
    WINTRUST_DATA data{}; data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN; data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &info; data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    auto status = WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE), &policy, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE), &policy, &data);
    if (status == ERROR_SUCCESS) return "valid-cached-chain-not-publisher-approved";
    if (status == TRUST_E_NOSIGNATURE) return "unsigned";
    std::ostringstream result;
    result << "unverified-0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<DWORD>(status);
    return result.str();
}
struct Record {
    const Slot& slot;
    std::string status = "missing", architecture = "unknown", sha256, fileVersion = "unavailable", authenticode = "not-checked";
    std::uint64_t size = 0;
    bool peMetadata = false;
    explicit Record(const Slot& s) : slot(s) {}
    std::string json() const {
        std::ostringstream out;
        out << "{\"slot\":" << jsonString(slot.path) << ",\"family\":" << jsonString(familyName(slot.family))
            << ",\"status\":" << jsonString(status) << ",\"architecture\":" << jsonString(architecture)
            << ",\"size\":" << size << ",\"sha256\":" << jsonString(sha256)
            << ",\"fileVersionUntrusted\":" << jsonString(fileVersion) << ",\"authenticode\":" << jsonString(authenticode)
            << ",\"peMetadataParsed\":" << (peMetadata ? "true" : "false")
            << ",\"activationAllowed\":false,\"swapAllowed\":false,\"reason\":" << jsonString(blockedReason(slot.family)) << "}";
        return out.str();
    }
};
Record inspect(const Slot& slot, const std::filesystem::path& path, std::uint64_t& budget) {
    Record result(slot);
    try {
        localPath(path, false);
        Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (file.value == INVALID_HANDLE_VALUE) throw std::runtime_error("unreadable-or-write-locked");
        BY_HANDLE_FILE_INFORMATION info{};
        if (GetFileType(file.value) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.value, &info))
            throw std::runtime_error("regular-file-required");
        if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
            throw std::runtime_error("reparse-point-or-directory-rejected");
        if (info.nNumberOfLinks != 1) throw std::runtime_error("hardlink-rejected");
        result.size = (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (!result.size || result.size > maxFile) throw std::runtime_error("file-size-limit");
        if (result.size > budget) throw std::runtime_error("audit-size-budget");
        budget -= result.size;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(result.size));
        DWORD read = 0;
        if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr)
            || read != bytes.size()) throw std::runtime_error("incomplete-read");
        result.sha256 = hash(bytes);
        const auto pe = inspectPe(bytes);
        result.architecture = pe.architecture;
        result.peMetadata = true;
        if (pe.dll != slot.dll) throw std::runtime_error("wrong-image-kind");
        result.fileVersion = version(path);
        result.authenticode = signature(path, file.value);
        result.status = pe.architecture == "x64" ? "inspected-not-approved" : "architecture-mismatch";
    } catch (const std::filesystem::filesystem_error&) {
        result.status = "filesystem-error";
    } catch (const std::runtime_error& error) {
        // Only application-defined codes: never include OS paths or media information.
        result.status = error.what();
    }
    return result;
}
std::string report(const std::vector<Record>& records, bool inventory) {
    bool layout = inventory;
    for (const auto& r : records) if (r.slot.required && r.status != "inspected-not-approved") layout = false;
    std::ostringstream out;
    out << "{\"schemaVersion\":1,\"mode\":\"read-only-audit\",\"scanScope\":"
        << jsonString(inventory ? "fixed-vlc-relative-slots-no-recursion" : "explicit-candidate")
        << ",\"vlcLayoutDetected\":" << (layout ? "true" : "false")
        << ",\"runtimeCompatibility\":\"not-established\",\"signaturePolicy\":\"offline-cache-only-no-publisher-approval\""
        << ",\"dlssBackendAvailable\":false,\"swapSupported\":false,\"activationSupported\":false"
        << ",\"vsr\":\"driver-managed-not-a-dlss-dll-swap\",\"nr\":\"separate-model-data-workflow\",\"files\":[";
    bool first = true;
    for (const auto& r : records) { if (!first) out << ','; first = false; out << r.json(); }
    return out.str() + "]}\n";
}
}
std::string auditVlc(const std::filesystem::path& directory) {
    localPath(directory, true);
    std::uint64_t budget = maxAudit;
    std::vector<Record> records;
    for (const auto& slot : slots) records.push_back(inspect(slot, directory / std::filesystem::path(slot.path), budget));
    return report(records, true);
}
std::string auditCandidate(const std::filesystem::path& file) {
    const auto* slot = candidateSlot(asciiName(file));
    if (!slot) throw std::runtime_error("unrecognized-library-name");
    std::uint64_t budget = maxAudit;
    return report({inspect(*slot, file, budget)}, false);
}
}
