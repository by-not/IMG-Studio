#pragma once

#include "core/ImgTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgstudio::app {

enum class BackupKind : std::uint8_t {
    Automatic = 0,
    Manual
};

struct BackupRecord {
    std::string id;
    std::filesystem::path original_archive_path;
    std::filesystem::path backup_file_path;
    std::string timestamp_utc;
    std::uint64_t size_bytes = 0;
    std::string sha256_hex;
    BackupKind kind = BackupKind::Automatic;
    std::string reason;
    bool file_exists = false;
};

class BackupManager {
public:
    explicit BackupManager(std::filesystem::path backup_dir);

    void set_backup_directory(std::filesystem::path backup_dir);
    [[nodiscard]] const std::filesystem::path& backup_directory() const noexcept { return backup_dir_; }

    [[nodiscard]] core::Result<BackupRecord> create_backup(
        const std::filesystem::path& archive_path,
        BackupKind kind,
        const std::string& reason = {});

    [[nodiscard]] std::vector<BackupRecord> list_backups(
        const std::filesystem::path& filter_archive = {}) const;

    [[nodiscard]] core::Result<void> restore_backup(
        const BackupRecord& record,
        const std::filesystem::path& target_path_override = {});

    [[nodiscard]] core::Result<void> delete_backup(const std::string& backup_id);

    [[nodiscard]] static std::string recovery_instructions(const BackupRecord& record);
    [[nodiscard]] static std::string compute_file_sha256(const std::filesystem::path& file_path);

private:
    [[nodiscard]] std::filesystem::path index_path() const;
    [[nodiscard]] std::vector<BackupRecord> load_index() const;
    [[nodiscard]] core::Result<void> save_index(const std::vector<BackupRecord>& records) const;

    std::filesystem::path backup_dir_;
};

} // namespace imgstudio::app
