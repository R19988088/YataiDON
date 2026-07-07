#include "filesystem.h"
#include "miniz/miniz.h"
#include <array>
#include <fstream>
#include <spdlog/spdlog.h>
#include <unistd.h>

#ifdef __ANDROID__
    #include <SDL3/SDL.h>
#endif
#ifdef _WIN32
    #include <windows.h>
#endif
#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

#ifdef __ANDROID__
namespace {
fs::path get_android_pref_path() {
    char* pref_path = SDL_GetPrefPath("YataiDON", "YataiDON");
    if (!pref_path) {
        spdlog::warn("SDL_GetPrefPath failed: {}", SDL_GetError());
        return {};
    }

    fs::path path(pref_path);
    SDL_free(pref_path);
    return path;
}

bool switch_to_writable_directory(const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        spdlog::warn("Could not create {}: {}", dir.string(), ec.message());
        return false;
    }

    fs::current_path(dir, ec);
    if (ec) {
        spdlog::warn("Could not use {} as working directory: {}", dir.string(), ec.message());
        return false;
    }

    fs::path write_probe = dir / ".yataidon-write-test";
    {
        std::ofstream probe(write_probe, std::ios::binary | std::ios::trunc);
        if (!probe) {
            spdlog::warn("Working directory is not writable: {}", dir.string());
            return false;
        }
        probe << "ok";
    }
    fs::remove(write_probe, ec);

    spdlog::info("Working directory set to: {}", dir.string());
    return true;
}

bool copy_android_asset_to_file(const char* asset_path, const fs::path& destination) {
    SDL_IOStream* input = SDL_IOFromFile(asset_path, "rb");
    if (!input) {
        spdlog::warn("Could not open Android asset {}: {}", asset_path, SDL_GetError());
        return false;
    }

    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::warn("Could not create {} from Android asset", destination.string());
        SDL_CloseIO(input);
        return false;
    }

    std::array<char, 16 * 1024> buffer{};
    size_t bytes_read = 0;
    while ((bytes_read = SDL_ReadIO(input, buffer.data(), buffer.size())) > 0) {
        output.write(buffer.data(), static_cast<std::streamsize>(bytes_read));
        if (!output) {
            spdlog::warn("Could not write {}", destination.string());
            SDL_CloseIO(input);
            return false;
        }
    }

    SDL_CloseIO(input);
    return true;
}

void ensure_android_default_config() {
    std::error_code ec;
    if (fs::exists("dev-config.toml", ec) || fs::exists("config.toml", ec))
        return;

    if (copy_android_asset_to_file("config.toml", "config.toml")) {
        spdlog::info("Created default config.toml");
    } else {
        spdlog::error("config.toml is missing and the bundled default could not be copied");
    }
}
}
#endif

void set_working_directory_to_executable() {
#ifdef __ANDROID__
    std::vector<fs::path> candidates = { fs::path("/sdcard/YataiDON") };
    fs::path pref_path = get_android_pref_path();
    if (!pref_path.empty())
        candidates.push_back(pref_path);

    for (const fs::path& candidate : candidates) {
        if (switch_to_writable_directory(candidate)) {
            ensure_android_default_config();
            return;
        }
    }

    spdlog::error("Failed to find a writable Android working directory");
#elif __EMSCRIPTEN__
    spdlog::info("Emscripten: using virtual FS root as working directory");
#elif _WIN32
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::filesystem::path exe_path(buffer);
    std::filesystem::path exe_dir = exe_path.parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#elif __APPLE__
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) {
        spdlog::error("Failed to get executable path: buffer too small");
        return;
    }
    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) == nullptr) {
        spdlog::error("Failed to resolve executable path");
        return;
    }
    std::filesystem::path exe_dir = std::filesystem::path(resolved).parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        spdlog::error("Failed to get executable path");
        return;
    }
    buffer[len] = '\0';
    std::filesystem::path exe_dir = std::filesystem::path(buffer).parent_path();
    std::filesystem::current_path(exe_dir);
    spdlog::info("Working directory set to: {}", exe_dir.string());
#endif
}

void extract_osz(const fs::path& osz_path) {
    fs::path out_dir = osz_path.parent_path() / osz_path.stem();
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, osz_path.string().c_str(), 0)) {
        spdlog::error("extract_osz: failed to open {}", osz_path.string());
        return;
    }

    int num_files = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        fs::path out_file = out_dir / stat.m_filename;
        fs::create_directories(out_file.parent_path(), ec);

        if (!mz_zip_reader_extract_to_file(&zip, i, out_file.string().c_str(), 0))
            spdlog::warn("extract_osz: failed to extract {} from {}", stat.m_filename, osz_path.string());
    }

    mz_zip_reader_end(&zip);
    fs::remove(osz_path, ec);
    spdlog::info("extract_osz: extracted {} to {}", osz_path.string(), out_dir.string());
}

std::vector<fs::path> get_song_files(std::vector<fs::path> root_path) {
    std::vector<fs::path> songs;
    for (const fs::path& path : root_path) {
        // First pass: extract any .osz archives
        try {
            std::vector<fs::path> osz_files;
            for (const auto& entry : fs::recursive_directory_iterator(
                     path, fs::directory_options::skip_permission_denied | fs::directory_options::follow_directory_symlink)) {
                if (entry.path().extension() == ".osz")
                    osz_files.push_back(entry.path());
            }
            for (const auto& osz : osz_files)
                extract_osz(osz);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Error scanning for .osz files: {}", e.what());
        }

        // Second pass: collect .tja and .osu files
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     path, std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink)) {
                auto ext = entry.path().extension();
                if (ext == ".tja" || ext == ".osu" || ext == ".bin") {
                    songs.push_back(entry.path());
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            spdlog::error("Error scanning song directory: {}", e.what());
        }
    }
    return songs;
}

rapidjson::Document read_json_file(fs::path file_path) {
    if (!fs::exists(file_path)) {
        throw std::runtime_error("File not found: " + file_path.string());
    }

    if (file_path.extension() != ".json") {
        throw std::runtime_error("File is not a json file: " + file_path.string());
    }

    std::ifstream ifs(file_path);

    if (!ifs) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
    }
    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse " + file_path.string() + ": " + std::to_string(doc.GetParseError()));
    }

    return doc;
}

std::vector<SongListEntry> read_song_list(const fs::path& path) {
    std::vector<SongListEntry> entries;
    std::ifstream file(path);
    if (!file) return entries;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|'))
            fields.push_back(field);
        if (!line.empty() && line.back() == '|')
            fields.push_back("");

        if (fields.size() < 3) continue;

        std::string hash = fields[0];
        if (hash.size() >= 3 &&
            (unsigned char)hash[0] == 0xEF &&
            (unsigned char)hash[1] == 0xBB &&
            (unsigned char)hash[2] == 0xBF)
            hash = hash.substr(3);

        entries.push_back({std::move(hash), std::move(fields[1]), std::move(fields[2])});
    }
    return entries;
}

void write_song_list(const fs::path& path, const std::vector<SongListEntry>& entries) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto& e : entries)
        out << e.hash << "|" << e.title << "|" << e.subtitle << "\n";
}
