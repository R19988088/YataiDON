#include "settings.h"
#include "../libs/audio.h"
#include "../libs/input.h"
#include "../libs/filesystem.h"
#include "../libs/scores.h"

void save_config(const Config& config);

namespace {
using Allocator = rapidjson::Document::AllocatorType;

void set_localized_text(rapidjson::Value& object,
                        const char* en_text,
                        Allocator& allocator) {
    object.SetObject();
    object.AddMember("en", rapidjson::Value(en_text, allocator), allocator);
}

bool move_option(rapidjson::Document& tmpl,
                 const char* from_category,
                 const char* option_name,
                 rapidjson::Value& target_options,
                 Allocator& allocator) {
    if (!tmpl.HasMember(from_category) || !tmpl[from_category].IsObject())
        return false;
    rapidjson::Value& category = tmpl[from_category];
    if (!category.HasMember("options") || !category["options"].IsObject())
        return false;

    rapidjson::Value& options = category["options"];
    auto option_it = options.FindMember(option_name);
    if (option_it == options.MemberEnd())
        return false;

    rapidjson::Value key(option_it->name, allocator);
    rapidjson::Value value(option_it->value, allocator);
    target_options.AddMember(key, value, allocator);
    options.RemoveMember(option_it);
    return true;
}

void add_keybind_option(rapidjson::Document& tmpl,
                        const char* option_name,
                        const char* path,
                        const char* label,
                        const char* description) {
    if (!tmpl.HasMember("keybinds") || !tmpl["keybinds"].IsObject())
        return;
    rapidjson::Value& keybinds = tmpl["keybinds"];
    if (!keybinds.HasMember("options") || !keybinds["options"].IsObject())
        return;
    rapidjson::Value& options = keybinds["options"];
    if (options.HasMember(option_name))
        return;

    auto& allocator = tmpl.GetAllocator();
    rapidjson::Value option(rapidjson::kObjectType);
    rapidjson::Value name(rapidjson::kObjectType);
    rapidjson::Value desc(rapidjson::kObjectType);
    set_localized_text(name, label, allocator);
    set_localized_text(desc, description, allocator);

    option.AddMember("path", rapidjson::Value(path, allocator), allocator);
    option.AddMember("name", name, allocator);
    option.AddMember("description", desc, allocator);
    option.AddMember("type", rapidjson::Value("keybind", allocator), allocator);
    option.AddMember("values", rapidjson::Value(rapidjson::kObjectType), allocator);
    options.AddMember(rapidjson::Value(option_name, allocator), option, allocator);
}

void add_screen_option(rapidjson::Value& target_options,
                       const char* option_name,
                       const char* label,
                       const char* description,
                       const char* screen,
                       Allocator& allocator) {
    rapidjson::Value option(rapidjson::kObjectType);
    rapidjson::Value name(rapidjson::kObjectType);
    rapidjson::Value desc(rapidjson::kObjectType);
    set_localized_text(name, label, allocator);
    set_localized_text(desc, description, allocator);

    option.AddMember("name", name, allocator);
    option.AddMember("description", desc, allocator);
    option.AddMember("type", rapidjson::Value("screen", allocator), allocator);
    option.AddMember("path", rapidjson::Value("", allocator), allocator);
    option.AddMember("screen", rapidjson::Value(screen, allocator), allocator);
    option.AddMember("values", rapidjson::Value(rapidjson::kObjectType), allocator);
    target_options.AddMember(rapidjson::Value(option_name, allocator), option, allocator);
}

void add_skin_settings_category(rapidjson::Document& tmpl) {
    add_keybind_option(tmpl,
                       "settings_key",
                       "keys/settings_key",
                       "Settings Key",
                       "Key to open settings.");

    if (tmpl.HasMember("skin_settings"))
        return;

    auto& allocator = tmpl.GetAllocator();
    rapidjson::Value category(rapidjson::kObjectType);
    rapidjson::Value name(rapidjson::kObjectType);
    rapidjson::Value options(rapidjson::kObjectType);

    set_localized_text(name, "Skin Settings", allocator);
    move_option(tmpl, "general", "nijiiro_notes", options, allocator);
    move_option(tmpl, "general", "fake_online", options, allocator);
    add_screen_option(options,
                      "entry_costume",
                      "3D Don",
                      "Choose the 3D Don costume from the entry screen.",
                      "entry",
                      allocator);
    add_screen_option(options,
                      "skin_viewer",
                      "Skin Viewer",
                      "Inspect skin graphics and texture data.",
                      "skin_viewer",
                      allocator);

    category.AddMember("name", name, allocator);
    category.AddMember("options", options, allocator);
    tmpl.AddMember(rapidjson::Value("skin_settings", allocator), category, allocator);
}
}

void SettingsScreen::on_screen_start() {
    Screen::on_screen_start();

    fs::path skin_dir = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    fs::path tmpl_path = skin_dir / "settings_template.json";

    try {
        rapidjson::Document tmpl = read_json_file(tmpl_path);
        add_skin_settings_category(tmpl);
        box_manager = std::make_unique<SettingsBoxManager>(tmpl);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load settings template: {}", e.what());
        screen_init = false;
        return;
    }
    indicator   = Indicator(Indicator::State::SELECT);
    coin_overlay   = CoinOverlay();
    allnet_indicator = AllNetIcon();

    audio.play_sound("bgm", VolumePreset::MUSIC);
    screen_init = true;
}

Screens SettingsScreen::on_screen_end(Screens next_screen) {
    save_config(*global_data.config);
    scores_manager.save_player_data(scores_manager.player_1_data);
    scores_manager.save_player_data(scores_manager.player_2_data);
    spdlog::info("Settings saved");

    audio.close_audio_device();
    fs::path sounds_path = fs::path("Skins") / global_data.config->paths.skin / "Sounds";
    audio.init_audio_device(sounds_path, global_data.config->audio, global_data.config->volume);

    box_manager.reset();

    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SettingsScreen::handle_input() {
    if (ray::IsKeyPressed(ray::KEY_F3)) {
        return on_screen_end(Screens::INPUT_CALI);
    }
    if (ray::IsKeyPressed(ray::KEY_F2)) {
        return on_screen_end(Screens::SKIN_VIEWER);
    }
    if (box_manager->is_capturing_input()) {
        return std::nullopt;
    }
    if (is_l_kat_pressed() || check_key_pressed(ray::KEY_LEFT) || check_key_pressed(ray::KEY_UP)) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_left();
    } else if (is_r_kat_pressed() || check_key_pressed(ray::KEY_RIGHT) || check_key_pressed(ray::KEY_DOWN)) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_right();
    } else if (is_l_don_pressed() || is_r_don_pressed() || check_key_pressed(ray::KEY_ENTER)) {
        audio.play_sound("don", VolumePreset::SOUND);
        bool result = box_manager->select_box();
        if (result) {
            return on_screen_end(Screens::ENTRY);
        }
    }
    return std::nullopt;
}

std::optional<Screens> SettingsScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    indicator.update(current_time);
    box_manager->update(current_time);
    if (auto screen = box_manager->pending_screen_change())
        return on_screen_end(*screen);
    return handle_input();
}

void SettingsScreen::draw() {
    tex.draw_texture(BACKGROUND::BACKGROUND);
    box_manager->draw();
    tex.draw_texture(BACKGROUND::FOOTER);
    indicator.draw(tex.skin_config[SC::SONG_SELECT_INDICATOR].x,
                   tex.skin_config[SC::SONG_SELECT_INDICATOR].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
