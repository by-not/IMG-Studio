#include "app/BackupManager.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgArchive.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace imgstudio::app {

namespace {

std::tm utc_tm_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val{};
#ifdef _WIN32
    gmtime_s(&tm_val, &tt);
#else
    gmtime_r(&tt, &tm_val);
#endif
    return tm_val;
}

std::string format_timestamp_compact(const std::tm& tm_val) {
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string format_timestamp_iso(const std::tm& tm_val) {
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string sanitize_stem(std::string stem) {
    for (char& ch : stem) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '-' || ch == '_';
        if (!ok) {
            ch = '_';
        }
    }
    return stem.empty() ? std::string("archive") : stem;
}

} // namespace

BackupManager::BackupManager(std::filesystem::path backup_dir)
    : backup_dir_(std::move(backup_dir)) {}

void BackupManager::set_backup_directory(std::filesystem::path backup_dir) {
    backup_dir_ = std::move(backup_dir);
}

std::filesystem::path BackupManager::index_path() const {
    return backup_dir_ / "backups_index.json";
}

std::string BackupManager::compute_file_sha256(const std::filesystem::path& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    core::Sha256 sha;
    std::array<std::uint8_t, 65536> buf{};
    while (in) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize n = in.gcount();
        if (n > 0) {
            sha.update(std::span<const std::uint8_t>(buf.data(), static_cast<std::size_t>(n)));
        }
    }
    return sha.finalize_hex();
}

std::vector<BackupRecord> BackupManager::load_index() const {
    std::vector<BackupRecord> out;
    const auto idx = index_path();
    std::error_code ec;
    if (!std::filesystem::exists(idx, ec)) {
        return out;
    }

    std::ifstream in(idx);
    if (!in.is_open()) {
        return out;
    }

    try {
        nlohmann::json j;
        in >> j;
        if (!j.is_array()) {
            return out;
        }
        for (const auto& item : j) {
            if (!item.is_object()) {
                continue;
            }
            BackupRecord rec;
            rec.id = item.value("id", "");
            rec.original_archive_path = platform::from_utf8(item.value("original_archive_path", ""));
            rec.backup_file_path = platform::from_utf8(item.value("backup_file_path", ""));
            rec.timestamp_utc = item.value("timestamp_utc", "");
            rec.size_bytes = item.value("size_bytes", std::uint64_t{0});
            rec.sha256_hex = item.value("sha256_hex", "");
            rec.kind = (item.value("kind", 0) == 1) ? BackupKind::Manual : BackupKind::Automatic;
            rec.reason = item.value("reason", "");
            rec.file_exists = std::filesystem::exists(rec.backup_file_path, ec);
            if (!rec.id.empty() && !rec.backup_file_path.empty()) {
                out.push_back(std::move(rec));
            }
        }
    } catch (...) {
        return {};
    }
    return out;
}

core::Result<void> BackupManager::save_index(const std::vector<BackupRecord>& records) const {
    std::error_code ec;
    std::filesystem::create_directories(backup_dir_, ec);
    if (ec) {
        return core::Result<void>::failure("Failed to create backup directory.", ec.message());
    }

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& rec : records) {
        arr.push_back({
            {"id", rec.id},
            {"original_archive_path", platform::to_utf8(rec.original_archive_path)},
            {"backup_file_path", platform::to_utf8(rec.backup_file_path)},
            {"timestamp_utc", rec.timestamp_utc},
            {"size_bytes", rec.size_bytes},
            {"sha256_hex", rec.sha256_hex},
            {"kind", static_cast<int>(rec.kind)},
            {"reason", rec.reason}
        });
    }

    const auto idx = index_path();
    const auto tmp = platform::make_unique_temp_path(idx, ".json.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return core::Result<void>::failure("Failed to open temporary backup index.");
        }
        out << arr.dump(2) << '\n';
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(tmp, ec);
            return core::Result<void>::failure("Failed to flush backup index.");
        }
    }

    auto rep = platform::atomic_replace_file(tmp, idx);
    if (!rep.ok()) {
        std::filesystem::remove(tmp, ec);
    }
    return rep;
}

core::Result<BackupRecord> BackupManager::create_backup(
    const std::filesystem::path& archive_path,
    BackupKind kind,
    const std::string& reason) {
    std::error_code ec;
    if (!std::filesystem::exists(archive_path, ec) || !std::filesystem::is_regular_file(archive_path, ec)) {
        return core::Result<BackupRecord>::failure(
            "Source archive does not exist for backup.",
            platform::to_utf8(archive_path));
    }

    std::filesystem::create_directories(backup_dir_, ec);
    if (ec) {
        return core::Result<BackupRecord>::failure(
            "Failed to create backup directory.",
            ec.message());
    }

    const std::tm tm_val = utc_tm_now();
    const std::string compact = format_timestamp_compact(tm_val);
    const std::string iso = format_timestamp_iso(tm_val);
    const std::string stem = sanitize_stem(archive_path.stem().string());
    const std::string kind_str = (kind == BackupKind::Automatic) ? "auto" : "manual";

    std::string filename = stem + "_" + compact + "_" + kind_str + ".img";
    std::filesystem::path dest = backup_dir_ / filename;
    int suffix = 1;
    while (std::filesystem::exists(dest, ec)) {
        filename = stem + "_" + compact + "_" + kind_str + "_" + std::to_string(suffix++) + ".img";
        dest = backup_dir_ / filename;
    }

    auto copy_res = platform::copy_file_verified(archive_path, dest);
    if (!copy_res.ok()) {
        return core::Result<BackupRecord>::failure(
            "Failed to copy archive to backup location.",
            copy_res.error->message + (copy_res.error->detail.empty() ? "" : " (" + copy_res.error->detail + ")"));
    }

    const std::string src_hash = compute_file_sha256(archive_path);
    const std::string dst_hash = compute_file_sha256(dest);
    if (src_hash.empty() || src_hash != dst_hash) {
        std::filesystem::remove(dest, ec);
        return core::Result<BackupRecord>::failure(
            "Backup SHA-256 verification failed after copy.");
    }

    BackupRecord rec;
    rec.id = filename;
    rec.original_archive_path = std::filesystem::weakly_canonical(archive_path, ec);
    if (ec) {
        rec.original_archive_path = archive_path;
    }
    rec.backup_file_path = dest;
    rec.timestamp_utc = iso;
    rec.size_bytes = std::filesystem::file_size(dest, ec);
    rec.sha256_hex = dst_hash;
    rec.kind = kind;
    rec.reason = reason.empty() ? (kind == BackupKind::Automatic ? "Pre-commit automatic backup" : "Manual backup") : reason;
    rec.file_exists = true;

    auto records = load_index();
    records.insert(records.begin(), rec);
    auto save_res = save_index(records);
    if (!save_res.ok()) {
        return core::Result<BackupRecord>::failure(
            "Backup file was created, but updating backup index failed.",
            save_res.error->message);
    }

    return core::Result<BackupRecord>::success(std::move(rec));
}

std::vector<BackupRecord> BackupManager::list_backups(
    const std::filesystem::path& filter_archive) const {
    auto records = load_index();
    if (filter_archive.empty()) {
        return records;
    }

    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical(filter_archive, ec);
    const std::string target = platform::to_utf8(ec ? filter_archive : canon);

    std::vector<BackupRecord> filtered;
    for (const auto& r : records) {
        if (platform::to_utf8(r.original_archive_path) == target) {
            filtered.push_back(r);
        }
    }
    return filtered;
}

core::Result<void> BackupManager::restore_backup(
    const BackupRecord& record,
    const std::filesystem::path& target_path_override) {
    std::error_code ec;
    if (!std::filesystem::exists(record.backup_file_path, ec)) {
        return core::Result<void>::failure(
            "Backup file is missing on disk.",
            platform::to_utf8(record.backup_file_path));
    }

    if (!record.sha256_hex.empty()) {
        const std::string actual_hash = compute_file_sha256(record.backup_file_path);
        if (actual_hash != record.sha256_hex) {
            return core::Result<void>::failure(
                "Backup SHA-256 checksum does not match recorded hash; restore aborted.",
                platform::to_utf8(record.backup_file_path));
        }
    }

    auto open_res = core::ImgArchive::open(record.backup_file_path);
    if (!open_res.ok() || !open_res.value->validation_report().is_valid_structure()) {
        return core::Result<void>::failure(
            "Backup archive failed structural VER2 validation; restore aborted.",
            platform::to_utf8(record.backup_file_path));
    }

    const std::filesystem::path dest = target_path_override.empty()
        ? record.original_archive_path
        : target_path_override;
    if (dest.empty()) {
        return core::Result<void>::failure("No target path specified for backup restore.");
    }

    const std::filesystem::path tmp = platform::make_unique_temp_path(dest, ".restoretmp");
    auto copy_res = platform::copy_file_verified(record.backup_file_path, tmp);
    if (!copy_res.ok()) {
        std::filesystem::remove(tmp, ec);
        return copy_res;
    }

    auto rep = platform::atomic_replace_file(tmp, dest);
    if (!rep.ok()) {
        std::filesystem::remove(tmp, ec);
    }
    return rep;
}

core::Result<void> BackupManager::delete_backup(const std::string& backup_id) {
    auto records = load_index();
    std::error_code ec;
    bool found = false;
    for (auto it = records.begin(); it != records.end(); ++it) {
        if (it->id == backup_id) {
            std::filesystem::remove(it->backup_file_path, ec);
            records.erase(it);
            found = true;
            break;
        }
    }
    if (!found) {
        return core::Result<void>::failure("Backup record not found.", backup_id);
    }
    return save_index(records);
}

std::string BackupManager::recovery_instructions(const BackupRecord& record) {
    std::ostringstream oss;
    oss << "Manual Recovery Instructions:\n"
        << "1. Close GTA San Andreas or any modding tool using the target IMG.\n"
        << "2. Locate backup file:\n   " << platform::to_utf8(record.backup_file_path) << "\n"
        << "3. Copy it over the original archive:\n   " << platform::to_utf8(record.original_archive_path) << "\n"
        << "4. Verify SHA-256 if needed: " << record.sha256_hex;
    return oss.str();
}

} // namespace imgstudio::app
