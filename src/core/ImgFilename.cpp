#include "core/ImgFilename.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace imgstudio::core {

namespace {

[[nodiscard]] constexpr bool is_forbidden_filename_char(unsigned char ch) noexcept {
    switch (ch) {
        case '/':
        case '\\':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] std::string to_ascii_upper(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char raw_ch : input) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        if (ch >= 'a' && ch <= 'z') {
            out.push_back(static_cast<char>(ch - ('a' - 'A')));
        } else {
            out.push_back(raw_ch);
        }
    }
    return out;
}

} // namespace

std::string to_ascii_lower(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char raw_ch : input) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        if (ch >= 'A' && ch <= 'Z') {
            out.push_back(static_cast<char>(ch + ('a' - 'A')));
        } else {
            out.push_back(raw_ch);
        }
    }
    return out;
}

std::string extract_extension_lower(std::string_view filename) {
    const std::size_t dot = filename.rfind('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1U >= filename.size()) {
        return {};
    }
    return to_ascii_lower(filename.substr(dot));
}

FileCategory classify_extension(std::string_view ext_lower) noexcept {
    if (ext_lower == ".dff") return FileCategory::DffModel;
    if (ext_lower == ".txd") return FileCategory::TxdTexture;
    if (ext_lower == ".col") return FileCategory::ColCollision;
    if (ext_lower == ".ifp") return FileCategory::IfpAnimation;
    if (ext_lower == ".ipl") return FileCategory::IplPlacement;
    if (ext_lower == ".ide") return FileCategory::IdeDefinition;
    if (ext_lower == ".scm") return FileCategory::ScmScript;
    if (ext_lower == ".dat" || ext_lower == ".cfg") return FileCategory::DataConfig;
    if (ext_lower == ".fxp") return FileCategory::FxpEffects;
    if (ext_lower == ".cut") return FileCategory::CutCutscene;
    return FileCategory::Other;
}

bool is_windows_reserved_filename(std::string_view filename) {
    if (filename.empty()) {
        return false;
    }
    const std::size_t dot = filename.find('.');
    std::string_view stem = (dot == std::string_view::npos) ? filename : filename.substr(0, dot);
    while (!stem.empty() && stem.back() == ' ') {
        stem.remove_suffix(1);
    }
    const std::string upper = to_ascii_upper(stem);
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") {
        return true;
    }
    if (upper.size() == 4 &&
        (upper.starts_with("COM") || upper.starts_with("LPT")) &&
        upper[3] >= '1' && upper[3] <= '9') {
        return true;
    }
    return false;
}

FilenameValidationResult validate_entry_filename(std::string_view name) {
    FilenameValidationResult res{};
    if (name.empty()) {
        res.reason = "Filename cannot be empty.";
        return res;
    }
    if (name.size() > kMaxFilenameLength) {
        res.reason = "Filename exceeds 23-byte VER2 null-terminated limit.";
        return res;
    }
    if (name == "." || name == "..") {
        res.safe_for_windows_export = false;
        res.reason = "Filename cannot be '.' or '..'.";
        return res;
    }
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        res.safe_for_windows_export = false;
        res.reason = "Filename cannot start/end with whitespace or end with a dot.";
        return res;
    }

    for (char raw_ch : name) {
        const auto ch = static_cast<unsigned char>(raw_ch);
        if (ch < 0x20U || ch == 0x7FU) {
            res.reason = "Filename contains control characters.";
            return res;
        }
        if (ch >= 0x80U) {
            res.ascii_only = false;
            res.reason = "Filename contains non-ASCII characters (VER2 requires 7-bit ASCII).";
            return res;
        }
        if (is_forbidden_filename_char(ch)) {
            res.safe_for_windows_export = false;
            res.reason = "Filename contains path or Windows-forbidden characters.";
            return res;
        }
    }

    if (is_windows_reserved_filename(name)) {
        res.safe_for_windows_export = false;
        res.reason = "Filename matches a reserved Windows device name.";
        return res;
    }

    res.valid = true;
    return res;
}

std::array<char, kMaxFilenameBytes> encode_raw_filename(std::string_view name) {
    std::array<char, kMaxFilenameBytes> raw{};
    const std::size_t len = std::min(name.size(), kMaxFilenameLength);
    for (std::size_t i = 0; i < len; ++i) {
        raw[i] = name[i];
    }
    raw[len] = '\0';
    return raw;
}

DecodedRawFilename decode_raw_filename(const std::array<char, kMaxFilenameBytes>& raw) {
    DecodedRawFilename decoded;
    std::size_t null_pos = kMaxFilenameBytes;
    for (std::size_t i = 0; i < kMaxFilenameBytes; ++i) {
        if (raw[i] == '\0') {
            null_pos = i;
            break;
        }
    }

    if (null_pos == kMaxFilenameBytes) {
        decoded.has_null_terminator = false;
        decoded.name.assign(raw.data(), kMaxFilenameBytes);
    } else {
        decoded.has_null_terminator = true;
        decoded.name.assign(raw.data(), null_pos);
        for (std::size_t i = null_pos + 1U; i < kMaxFilenameBytes; ++i) {
            if (raw[i] != '\0') {
                decoded.has_trailing_garbage = true;
                break;
            }
        }
    }

    for (char raw_ch : decoded.name) {
        if (static_cast<unsigned char>(raw_ch) >= 0x80U) {
            decoded.ascii_only = false;
            break;
        }
    }

    return decoded;
}

Result<std::filesystem::path> build_safe_export_path(
    const std::filesystem::path& target_dir,
    std::string_view entry_name,
    bool sanitize_unsafe) {
    if (entry_name.empty()) {
        return Result<std::filesystem::path>::failure("Cannot export an entry with an empty filename.");
    }

    std::string clean_name;
    clean_name.reserve(entry_name.size());

    const auto validation = validate_entry_filename(entry_name);
    if (!validation.valid) {
        if (!sanitize_unsafe) {
            return Result<std::filesystem::path>::failure(
                "Unsafe export filename rejected.",
                std::string(entry_name) + ": " + validation.reason);
        }
        for (char raw_ch : entry_name) {
            const auto ch = static_cast<unsigned char>(raw_ch);
            if (ch < 0x20U || ch == 0x7FU || ch >= 0x80U || is_forbidden_filename_char(ch)) {
                clean_name.push_back('_');
            } else {
                clean_name.push_back(raw_ch);
            }
        }
        while (!clean_name.empty() && (clean_name.back() == ' ' || clean_name.back() == '.')) {
            clean_name.pop_back();
        }
        if (clean_name.empty() || clean_name == "." || clean_name == "..") {
            clean_name = "unnamed_entry";
        }
        if (is_windows_reserved_filename(clean_name)) {
            clean_name = "_" + clean_name;
        }
    } else {
        clean_name.assign(entry_name);
    }

    std::error_code ec;
    const std::filesystem::path abs_dir = std::filesystem::weakly_canonical(target_dir, ec);
    const std::filesystem::path base = ec ? target_dir.lexically_normal() : abs_dir;
    const std::u8string u8_clean(reinterpret_cast<const char8_t*>(clean_name.data()), clean_name.size());
    const std::filesystem::path joined = (base / std::filesystem::path(u8_clean)).lexically_normal();

    if (joined.parent_path() != base) {
        const auto u8_joined = joined.u8string();
        return Result<std::filesystem::path>::failure(
            "Export path escapes target directory.",
            std::string(reinterpret_cast<const char*>(u8_joined.data()), u8_joined.size()));
    }

    return Result<std::filesystem::path>::success(joined);
}

} // namespace imgstudio::core
