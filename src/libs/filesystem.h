#include "rapidjson/document.h"
#include <rapidjson/istreamwrapper.h>
#include <unordered_map>
#include <array>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

struct SongCacheEntry {
    std::filesystem::file_time_type mtime;
    std::array<std::string, 5> hashes;
    std::string title;
    std::string subtitle;
};

using SongCache = std::unordered_map<std::string, SongCacheEntry>;

static constexpr uint32_t CACHE_MAGIC   = 0x59444343; // "YDCC"
static constexpr uint32_t CACHE_VERSION = 2;

void save_hash_cache(const SongCache& cache,
                     const fs::path& cache_path);

std::pair<int, SongCache> load_hash_cache(const fs::path& cache_path);

void migrate_hash_cache(SongCache& cache, const fs::path& cache_path);

void extract_osz(const fs::path& osz_path);

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path);

rapidjson::Document read_json_file(fs::path file_path);

struct SongListEntry {
    std::string hash;
    std::string title;
    std::string subtitle;
};

std::vector<SongListEntry> read_song_list(const fs::path& path);

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries);
