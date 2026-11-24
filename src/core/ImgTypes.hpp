#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace imgstudio::core {

inline constexpr std::uint64_t kSectorSize = 2048ULL;
inline constexpr std::uint64_t kHeaderSize = 8ULL;
inline constexpr std::uint64_t kDirectoryEntrySize = 32ULL;
inline constexpr std::size_t kMaxFilenameBytes = 24U;
inline constexpr std::size_t kMaxFilenameLength = 23U;
inline constexpr std::uint16_t kMaxEntrySectors = 65535U;
inline constexpr std::uint64_t kMaxEntryPayloadBytes =
    static_cast<std::uint64_t>(kMaxEntrySectors) * kSectorSize;
inline constexpr std::uint64_t kGtaSa2GiBThreshold = 2ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::array<std::uint8_t, 4> kVer2Magic = {'V', 'E', 'R', '2'};

enum class FileCategory : std::uint8_t {
    DffModel = 0,
    TxdTexture,
    ColCollision,
    IfpAnimation,
    IplPlacement,
    IdeDefinition,
    ScmScript,
    DataConfig,
    FxpEffects,
    CutCutscene,
    Other
};

enum class DiagnosticSeverity : std::uint8_t {
    Info = 0,
    Warning,
    Error
};

enum class DiagnosticCode : std::uint16_t {
    InvalidSignature = 1,
    TruncatedHeader,
    TruncatedDirectory,
    ArithmeticOverflow,
    EntryOutOfBounds,
    EntryOverlapsDirectory,
    EntryOverlapsPrevious,
    InvalidFilename,
    NonAsciiFilename,
    MissingNullTerminator,
    TrailingGarbageAfterNull,
    DuplicateEntryName,
    SuspiciousSizeMetadata,
    ZeroSizeEntry,
    LargeArchiveAbove2GiB,
    UnreadablePayload,
    IoFailure
};

struct ImgEntry {
    std::uint32_t index = 0;
    std::uint32_t offset_sectors = 0;
    std::uint16_t streaming_size = 0;
    std::uint16_t archive_size = 0;
    std::array<char, kMaxFilenameBytes> raw_name{};
    std::string name;
    std::string extension;
    FileCategory category = FileCategory::Other;
    bool has_null_terminator = true;
    bool has_trailing_garbage = false;

    [[nodiscard]] constexpr std::uint16_t effective_sectors() const noexcept {
        return archive_size != 0U ? archive_size : streaming_size;
    }

    [[nodiscard]] constexpr std::uint64_t offset_bytes() const noexcept {
        return static_cast<std::uint64_t>(offset_sectors) * kSectorSize;
    }

    [[nodiscard]] constexpr std::uint64_t effective_bytes() const noexcept {
        return static_cast<std::uint64_t>(effective_sectors()) * kSectorSize;
    }

    [[nodiscard]] constexpr std::uint64_t streaming_bytes() const noexcept {
        return static_cast<std::uint64_t>(streaming_size) * kSectorSize;
    }

    [[nodiscard]] constexpr std::uint64_t archive_bytes() const noexcept {
        return static_cast<std::uint64_t>(archive_size) * kSectorSize;
    }
};

struct ValidationDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    DiagnosticCode code = DiagnosticCode::InvalidSignature;
    std::optional<std::uint32_t> entry_index;
    std::string entry_name;
    std::string message;
};

struct ArchiveMetrics {
    std::uint64_t file_size_bytes = 0;
    std::uint32_t declared_entry_count = 0;
    std::uint32_t parsed_entry_count = 0;
    std::uint64_t directory_bytes = 0;
    std::uint64_t directory_padded_bytes = 0;
    std::uint64_t total_payload_bytes = 0;
    std::uint64_t wasted_gap_bytes = 0;
    std::uint32_t entries_with_nonzero_archive_size = 0;
    std::uint32_t duplicate_name_count = 0;
    bool is_fragmented = false;
};

struct ValidationReport {
    std::vector<ValidationDiagnostic> diagnostics;
    ArchiveMetrics metrics{};
    std::size_t error_count = 0;
    std::size_t warning_count = 0;
    std::size_t info_count = 0;

    [[nodiscard]] bool has_errors() const noexcept { return error_count > 0; }
    [[nodiscard]] bool has_warnings() const noexcept { return warning_count > 0; }
    [[nodiscard]] bool is_valid_structure() const noexcept { return error_count == 0; }

    void add(ValidationDiagnostic diag) {
        switch (diag.severity) {
            case DiagnosticSeverity::Info:
                ++info_count;
                break;
            case DiagnosticSeverity::Warning:
                ++warning_count;
                break;
            case DiagnosticSeverity::Error:
                ++error_count;
                break;
        }
        diagnostics.push_back(std::move(diag));
    }
};

struct ErrorInfo {
    std::string message;
    std::string detail;
};

template <typename T>
struct Result {
    std::optional<T> value;
    std::optional<ErrorInfo> error;

    [[nodiscard]] bool ok() const noexcept { return value.has_value() && !error.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    static Result<T> success(T val) {
        Result<T> r;
        r.value = std::move(val);
        return r;
    }

    static Result<T> failure(std::string msg, std::string det = {}) {
        Result<T> r;
        r.error = ErrorInfo{std::move(msg), std::move(det)};
        return r;
    }
};

template <>
struct Result<void> {
    bool succeeded = false;
    std::optional<ErrorInfo> error;

    [[nodiscard]] bool ok() const noexcept { return succeeded && !error.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    static Result<void> success() {
        Result<void> r;
        r.succeeded = true;
        return r;
    }

    static Result<void> failure(std::string msg, std::string det = {}) {
        Result<void> r;
        r.succeeded = false;
        r.error = ErrorInfo{std::move(msg), std::move(det)};
        return r;
    }
};

[[nodiscard]] constexpr std::string_view file_category_label(FileCategory cat) noexcept {
    switch (cat) {
        case FileCategory::DffModel: return "3D Model";
        case FileCategory::TxdTexture: return "Texture Dict";
        case FileCategory::ColCollision: return "Collision";
        case FileCategory::IfpAnimation: return "Animation";
        case FileCategory::IplPlacement: return "Item Placement";
        case FileCategory::IdeDefinition: return "Item Definition";
        case FileCategory::ScmScript: return "Script";
        case FileCategory::DataConfig: return "Data/Config";
        case FileCategory::FxpEffects: return "Effects";
        case FileCategory::CutCutscene: return "Cutscene";
        case FileCategory::Other: return "Other";
    }
    return "Other";
}

[[nodiscard]] constexpr std::string_view diagnostic_severity_label(DiagnosticSeverity sev) noexcept {
    switch (sev) {
        case DiagnosticSeverity::Info: return "Info";
        case DiagnosticSeverity::Warning: return "Warning";
        case DiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

} // namespace imgstudio::core
