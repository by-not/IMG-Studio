#pragma once

#include "app/ArchiveDocument.hpp"
#include "core/ImgTypes.hpp"
#include "core/rw/DffTypes.hpp"
#include "core/rw/TxdTypes.hpp"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace imgstudio::app {

struct TextureBindingInfo {
    std::string requested_name;
    bool resolved = false;
    std::string source_txd_name;
    const core::rw::TxdTexture* texture = nullptr;
};

struct LoadedDffSession {
    core::rw::DffModel model;
    std::vector<std::shared_ptr<const core::rw::TxdDictionary>> loaded_txds;
    std::unordered_map<std::string, TextureBindingInfo> resolved_textures; // lowercase key
    std::vector<std::string> resolution_notes;
    std::optional<std::filesystem::path> origin_archive_path;
};

struct AssetLoadCompletion {
    enum class Kind : std::uint8_t {
        DffLoaded = 0,
        TxdLoaded,
        AdditionalTxdLoaded
    };
    Kind kind = Kind::DffLoaded;
    bool ok = false;
    std::string message;
    std::string detail;
    std::optional<LoadedDffSession> dff_session;
    std::shared_ptr<const core::rw::TxdDictionary> txd_dict;
};

class AssetLoader {
public:
    ~AssetLoader();

    [[nodiscard]] static core::Result<std::vector<std::uint8_t>> read_document_entry_bytes(
        const ArchiveDocument& doc,
        std::uint64_t entry_id);

    [[nodiscard]] static core::Result<std::vector<std::uint8_t>> read_archive_entry_by_name(
        const std::filesystem::path& archive_path,
        std::string_view entry_name);

    [[nodiscard]] static core::Result<LoadedDffSession> load_dff_from_document_sync(
        const ArchiveDocument& doc,
        std::uint64_t entry_id,
        const std::vector<std::shared_ptr<const core::rw::TxdDictionary>>& extra_txds = {});

    [[nodiscard]] static core::Result<LoadedDffSession> load_dff_from_file_sync(
        const std::filesystem::path& dff_path,
        const std::vector<std::shared_ptr<const core::rw::TxdDictionary>>& extra_txds = {});

    [[nodiscard]] static core::Result<std::shared_ptr<const core::rw::TxdDictionary>>
    load_txd_from_document_sync(const ArchiveDocument& doc, std::uint64_t entry_id);

    [[nodiscard]] static core::Result<std::shared_ptr<const core::rw::TxdDictionary>>
    load_txd_from_file_sync(const std::filesystem::path& txd_path);

    static void resolve_dff_textures(LoadedDffSession& session);

    // Asynchronous loading helpers for UI responsiveness
    [[nodiscard]] bool is_busy() const noexcept { return busy_.load(std::memory_order_relaxed); }
    void start_load_dff_from_document(const ArchiveDocument& doc, std::uint64_t entry_id);
    void start_load_dff_from_file(const std::filesystem::path& path);
    void start_load_txd_from_document(const ArchiveDocument& doc, std::uint64_t entry_id, bool attach_to_dff = false);
    void start_load_txd_from_file(const std::filesystem::path& path, bool attach_to_dff = false);

    std::vector<AssetLoadCompletion> poll_completions();

private:
    void join_worker();
    void push_completion(AssetLoadCompletion comp);

    std::atomic<bool> busy_{false};
    std::mutex mutex_;
    std::vector<AssetLoadCompletion> completions_;
    std::thread worker_;
};

} // namespace imgstudio::app
