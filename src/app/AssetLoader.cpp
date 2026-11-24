#include "app/AssetLoader.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgFilename.hpp"
#include "core/rw/DffParser.hpp"
#include "core/rw/TxdParser.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <fstream>

namespace imgstudio::app {

namespace {

core::Result<std::vector<std::uint8_t>> read_file_bytes_limited(const std::filesystem::path& p) {
    std::error_code ec;
    const std::uint64_t fsize = std::filesystem::file_size(p, ec);
    if (ec) {
        return core::Result<std::vector<std::uint8_t>>::failure("Failed to stat file.", ec.message());
    }
    if (fsize > 256ULL * 1024ULL * 1024ULL) {
        return core::Result<std::vector<std::uint8_t>>::failure("File exceeds 256 MiB memory safety limit.");
    }
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) {
        return core::Result<std::vector<std::uint8_t>>::failure("Failed to open file.", platform::to_utf8(p));
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(fsize));
    if (fsize > 0ULL) {
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(fsize));
        if (!in || static_cast<std::uint64_t>(in.gcount()) != fsize) {
            return core::Result<std::vector<std::uint8_t>>::failure("Failed to read file contents.");
        }
    }
    return core::Result<std::vector<std::uint8_t>>::success(std::move(out));
}

core::Result<std::vector<std::uint8_t>> read_archive_range(
    const std::filesystem::path& archive_path,
    std::uint64_t offset,
    std::uint64_t len) {
    if (len > 256ULL * 1024ULL * 1024ULL) {
        return core::Result<std::vector<std::uint8_t>>::failure("Entry payload exceeds 256 MiB limit.");
    }
    std::error_code ec;
    const std::uint64_t fsize = std::filesystem::file_size(archive_path, ec);
    if (ec) {
        return core::Result<std::vector<std::uint8_t>>::failure("Failed to query archive size.", ec.message());
    }
    if (offset > fsize || len > fsize - offset) {
        return core::Result<std::vector<std::uint8_t>>::failure("Entry payload bounds exceed archive size.");
    }

    std::ifstream in(archive_path, std::ios::binary);
    if (!in.is_open()) {
        return core::Result<std::vector<std::uint8_t>>::failure("Failed to open archive file.");
    }
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        return core::Result<std::vector<std::uint8_t>>::failure("Failed to seek entry inside archive.");
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(len));
    if (len > 0ULL) {
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(len));
        if (!in || static_cast<std::uint64_t>(in.gcount()) != len) {
            return core::Result<std::vector<std::uint8_t>>::failure("Failed to read entry payload from archive.");
        }
    }
    return core::Result<std::vector<std::uint8_t>>::success(std::move(out));
}

struct EntryByteSource {
    std::string name;
    bool uses_external = false;
    std::filesystem::path external_path;
    std::filesystem::path archive_path;
    std::uint64_t offset = 0;
    std::uint64_t len = 0;
};

core::Result<std::vector<std::uint8_t>> read_from_source(const EntryByteSource& src) {
    if (src.uses_external) {
        return read_file_bytes_limited(src.external_path);
    }
    return read_archive_range(src.archive_path, src.offset, src.len);
}

std::vector<EntryByteSource> collect_matching_txd_sources(
    const ArchiveDocument& doc,
    const std::string& dff_name) {
    std::vector<EntryByteSource> sources;
    const std::string stem = core::to_ascii_lower(
        platform::from_utf8(dff_name).stem().string());
    const std::vector<std::string> candidates = {
        stem + ".txd",
        "vehicle.txd",
        "generic.txd"
    };

    for (const auto& cand : candidates) {
        for (const auto& e : doc.entries()) {
            if (e.change_state == EntryChangeState::Deleted) continue;
            if (core::to_ascii_lower(e.name) == cand) {
                sources.push_back(EntryByteSource{
                    e.name,
                    e.uses_external_file,
                    e.external_path,
                    doc.path(),
                    e.original_offset_bytes,
                    e.uses_external_file ? e.external_size_bytes : e.original_effective_bytes
                });
                break;
            }
        }
    }
    return sources;
}

} // namespace

AssetLoader::~AssetLoader() {
    join_worker();
}

void AssetLoader::join_worker() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void AssetLoader::push_completion(AssetLoadCompletion comp) {
    std::lock_guard lock(mutex_);
    completions_.push_back(std::move(comp));
    busy_.store(false, std::memory_order_relaxed);
}

std::vector<AssetLoadCompletion> AssetLoader::poll_completions() {
    std::lock_guard lock(mutex_);
    std::vector<AssetLoadCompletion> out;
    out.swap(completions_);
    return out;
}

core::Result<std::vector<std::uint8_t>> AssetLoader::read_document_entry_bytes(
    const ArchiveDocument& doc,
    std::uint64_t entry_id) {
    const auto* e = doc.find_by_id(entry_id);
    if (!e || e->change_state == EntryChangeState::Deleted) {
        return core::Result<std::vector<std::uint8_t>>::failure("Entry not found or marked deleted.");
    }
    if (e->uses_external_file) {
        return read_file_bytes_limited(e->external_path);
    }
    return read_archive_range(doc.path(), e->original_offset_bytes, e->original_effective_bytes);
}

core::Result<std::vector<std::uint8_t>> AssetLoader::read_archive_entry_by_name(
    const std::filesystem::path& archive_path,
    std::string_view entry_name) {
    auto arch_res = core::ImgArchive::open(archive_path);
    if (!arch_res.ok()) {
        return core::Result<std::vector<std::uint8_t>>::failure(arch_res.error->message, arch_res.error->detail);
    }
    const auto idx_opt = arch_res.value->find_index_case_insensitive(entry_name);
    if (!idx_opt.has_value()) {
        return core::Result<std::vector<std::uint8_t>>::failure("Entry not found in IMG archive.", std::string(entry_name));
    }
    const auto& e = arch_res.value->entries()[*idx_opt];
    return read_archive_range(archive_path, e.offset_bytes(), e.effective_bytes());
}

void AssetLoader::resolve_dff_textures(LoadedDffSession& session) {
    session.resolved_textures.clear();
    session.resolution_notes.clear();

    for (const auto& tex_name : session.model.referenced_textures) {
        const std::string key = core::to_ascii_lower(tex_name);
        TextureBindingInfo info;
        info.requested_name = tex_name;

        for (const auto& dict : session.loaded_txds) {
            if (!dict) continue;
            for (const auto& t : dict->textures) {
                if (core::to_ascii_lower(t.name) == key) {
                    if (t.supported && !t.mipmaps.empty()) {
                        info.resolved = true;
                        info.source_txd_name = dict->source_name;
                        info.texture = &t;
                        break;
                    } else {
                        session.resolution_notes.push_back(
                            "Texture '" + tex_name + "' found in " + dict->source_name +
                            " but format is unsupported: " + t.unsupported_reason);
                    }
                }
            }
            if (info.resolved) {
                break;
            }
        }
        session.resolved_textures.emplace(key, std::move(info));
    }
}

core::Result<LoadedDffSession> AssetLoader::load_dff_from_document_sync(
    const ArchiveDocument& doc,
    std::uint64_t entry_id,
    const std::vector<std::shared_ptr<const core::rw::TxdDictionary>>& extra_txds) {
    const auto* e = doc.find_by_id(entry_id);
    if (!e || e->change_state == EntryChangeState::Deleted) {
        return core::Result<LoadedDffSession>::failure("DFF entry not found or deleted.");
    }
    auto bytes_res = read_document_entry_bytes(doc, entry_id);
    if (!bytes_res.ok()) {
        return core::Result<LoadedDffSession>::failure(bytes_res.error->message, bytes_res.error->detail);
    }
    auto model_res = core::rw::DffParser::parse_bytes(*bytes_res.value, e->name);
    if (!model_res.ok()) {
        return core::Result<LoadedDffSession>::failure(model_res.error->message, model_res.error->detail);
    }

    LoadedDffSession session;
    session.model = std::move(*model_res.value);
    session.origin_archive_path = doc.path();
    session.loaded_txds = extra_txds;

    for (const auto& src : collect_matching_txd_sources(doc, e->name)) {
        auto txd_bytes = read_from_source(src);
        if (txd_bytes.ok()) {
            auto parsed = core::rw::TxdParser::parse_bytes(*txd_bytes.value, src.name);
            if (parsed.ok()) {
                session.loaded_txds.push_back(
                    std::make_shared<const core::rw::TxdDictionary>(std::move(*parsed.value)));
            }
        }
    }

    resolve_dff_textures(session);
    return core::Result<LoadedDffSession>::success(std::move(session));
}

core::Result<LoadedDffSession> AssetLoader::load_dff_from_file_sync(
    const std::filesystem::path& dff_path,
    const std::vector<std::shared_ptr<const core::rw::TxdDictionary>>& extra_txds) {
    auto model_res = core::rw::DffParser::parse_file(dff_path);
    if (!model_res.ok()) {
        return core::Result<LoadedDffSession>::failure(model_res.error->message, model_res.error->detail);
    }

    LoadedDffSession session;
    session.model = std::move(*model_res.value);
    session.loaded_txds = extra_txds;

    // Auto-look for sibling <stem>.txd next to external DFF
    std::error_code ec;
    const auto sibling_txd = dff_path.parent_path() / (dff_path.stem().string() + ".txd");
    if (std::filesystem::exists(sibling_txd, ec) && std::filesystem::is_regular_file(sibling_txd, ec)) {
        auto parsed = core::rw::TxdParser::parse_file(sibling_txd);
        if (parsed.ok()) {
            session.loaded_txds.push_back(
                std::make_shared<const core::rw::TxdDictionary>(std::move(*parsed.value)));
        }
    }

    resolve_dff_textures(session);
    return core::Result<LoadedDffSession>::success(std::move(session));
}

core::Result<std::shared_ptr<const core::rw::TxdDictionary>> AssetLoader::load_txd_from_document_sync(
    const ArchiveDocument& doc,
    std::uint64_t entry_id) {
    const auto* e = doc.find_by_id(entry_id);
    if (!e || e->change_state == EntryChangeState::Deleted) {
        return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::failure("TXD entry not found or deleted.");
    }
    auto bytes_res = read_document_entry_bytes(doc, entry_id);
    if (!bytes_res.ok()) {
        return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::failure(
            bytes_res.error->message, bytes_res.error->detail);
    }
    auto parsed = core::rw::TxdParser::parse_bytes(*bytes_res.value, e->name);
    if (!parsed.ok()) {
        return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::failure(
            parsed.error->message, parsed.error->detail);
    }
    return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::success(
        std::make_shared<const core::rw::TxdDictionary>(std::move(*parsed.value)));
}

core::Result<std::shared_ptr<const core::rw::TxdDictionary>> AssetLoader::load_txd_from_file_sync(
    const std::filesystem::path& txd_path) {
    auto parsed = core::rw::TxdParser::parse_file(txd_path);
    if (!parsed.ok()) {
        return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::failure(
            parsed.error->message, parsed.error->detail);
    }
    return core::Result<std::shared_ptr<const core::rw::TxdDictionary>>::success(
        std::make_shared<const core::rw::TxdDictionary>(std::move(*parsed.value)));
}

void AssetLoader::start_load_dff_from_document(const ArchiveDocument& doc, std::uint64_t entry_id) {
    const auto* e = doc.find_by_id(entry_id);
    if (!e || e->change_state == EntryChangeState::Deleted || busy_.exchange(true)) {
        return;
    }
    join_worker();

    const EntryByteSource dff_src{
        e->name, e->uses_external_file, e->external_path, doc.path(),
        e->original_offset_bytes,
        e->uses_external_file ? e->external_size_bytes : e->original_effective_bytes
    };
    const auto txd_sources = collect_matching_txd_sources(doc, e->name);
    const auto archive_path = doc.path();

    worker_ = std::thread([this, dff_src, txd_sources, archive_path]() {
        AssetLoadCompletion comp;
        comp.kind = AssetLoadCompletion::Kind::DffLoaded;
        auto bytes = read_from_source(dff_src);
        if (!bytes.ok()) {
            comp.ok = false;
            comp.message = bytes.error->message;
            comp.detail = bytes.error->detail;
            push_completion(std::move(comp));
            return;
        }
        auto model_res = core::rw::DffParser::parse_bytes(*bytes.value, dff_src.name);
        if (!model_res.ok()) {
            comp.ok = false;
            comp.message = model_res.error->message;
            comp.detail = model_res.error->detail;
            push_completion(std::move(comp));
            return;
        }
        LoadedDffSession session;
        session.model = std::move(*model_res.value);
        session.origin_archive_path = archive_path;
        for (const auto& ts : txd_sources) {
            auto tb = read_from_source(ts);
            if (tb.ok()) {
                auto tp = core::rw::TxdParser::parse_bytes(*tb.value, ts.name);
                if (tp.ok()) {
                    session.loaded_txds.push_back(
                        std::make_shared<const core::rw::TxdDictionary>(std::move(*tp.value)));
                }
            }
        }
        resolve_dff_textures(session);
        comp.ok = true;
        comp.message = "Loaded DFF: " + dff_src.name;
        comp.dff_session = std::move(session);
        push_completion(std::move(comp));
    });
}

void AssetLoader::start_load_dff_from_file(const std::filesystem::path& path) {
    if (busy_.exchange(true)) return;
    join_worker();
    worker_ = std::thread([this, path]() {
        AssetLoadCompletion comp;
        comp.kind = AssetLoadCompletion::Kind::DffLoaded;
        auto res = load_dff_from_file_sync(path);
        if (res.ok()) {
            comp.ok = true;
            comp.message = "Loaded DFF: " + platform::to_utf8(path.filename());
            comp.dff_session = std::move(*res.value);
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void AssetLoader::start_load_txd_from_document(
    const ArchiveDocument& doc,
    std::uint64_t entry_id,
    bool attach_to_dff) {
    const auto* e = doc.find_by_id(entry_id);
    if (!e || e->change_state == EntryChangeState::Deleted || busy_.exchange(true)) {
        return;
    }
    join_worker();
    const EntryByteSource src{
        e->name, e->uses_external_file, e->external_path, doc.path(),
        e->original_offset_bytes,
        e->uses_external_file ? e->external_size_bytes : e->original_effective_bytes
    };
    worker_ = std::thread([this, src, attach_to_dff]() {
        AssetLoadCompletion comp;
        comp.kind = attach_to_dff ? AssetLoadCompletion::Kind::AdditionalTxdLoaded
                                  : AssetLoadCompletion::Kind::TxdLoaded;
        auto bytes = read_from_source(src);
        if (!bytes.ok()) {
            comp.ok = false;
            comp.message = bytes.error->message;
            comp.detail = bytes.error->detail;
            push_completion(std::move(comp));
            return;
        }
        auto parsed = core::rw::TxdParser::parse_bytes(*bytes.value, src.name);
        if (parsed.ok()) {
            comp.ok = true;
            comp.message = "Loaded TXD: " + src.name;
            comp.txd_dict = std::make_shared<const core::rw::TxdDictionary>(std::move(*parsed.value));
        } else {
            comp.ok = false;
            comp.message = parsed.error->message;
            comp.detail = parsed.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void AssetLoader::start_load_txd_from_file(const std::filesystem::path& path, bool attach_to_dff) {
    if (busy_.exchange(true)) return;
    join_worker();
    worker_ = std::thread([this, path, attach_to_dff]() {
        AssetLoadCompletion comp;
        comp.kind = attach_to_dff ? AssetLoadCompletion::Kind::AdditionalTxdLoaded
                                  : AssetLoadCompletion::Kind::TxdLoaded;
        auto res = load_txd_from_file_sync(path);
        if (res.ok()) {
            comp.ok = true;
            comp.message = "Loaded TXD: " + platform::to_utf8(path.filename());
            comp.txd_dict = std::move(*res.value);
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

} // namespace imgstudio::app
