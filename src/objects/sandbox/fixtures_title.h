#include "../../scenes/sandbox.h"
#include "../title/warning_screen.h"
#include "../title/attract_camera.h"
#include "../title/bana_advert.h"
#include "../title/camera_cloud.h"
#include "texture_ids_generated.h"

struct WarningXFixture : public SandboxScreen::Fixture {
    std::optional<WarningX> active;

    WarningXFixture() { name = "WarningX"; screen = "title"; }

    uint32_t anchor_texture_id() override { return WARNING::X_RED; }

    void reset(double) override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override {
        if (active) { active->draw_bg(); active->draw_fg(); }
    }

    std::vector<std::string> debug_lines() override {
        return { std::string("Finished : ") + (active && active->is_finished() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct WarningBachiHitFixture : public SandboxScreen::Fixture {
    std::optional<WarningBachiHit> active;

    WarningBachiHitFixture() { name = "WarningBachiHit"; screen = "title"; }

    uint32_t anchor_texture_id() override { return WARNING::BACHI_HIT; }

    void reset(double) override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Finished : ") + (active && active->is_finished() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct AttractCameraFixture : public SandboxScreen::Fixture {
    std::optional<AttractCamera> active;

    AttractCameraFixture() { name = "AttractCamera"; screen = "title"; }

    uint32_t anchor_texture_id() override { return CAMERA::BACKGROUND; }

    void reset(double) override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(); }

    std::vector<std::string> debug_lines() override {
        return { std::string("Finished : ") + (active && active->is_finished() ? "yes" : "no") };
    }
    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct BanaAdvertisementFixture : public SandboxScreen::Fixture {
    std::optional<BanaAdvertisement> active;

    BanaAdvertisementFixture() { name = "BanaAdvertisement"; screen = "title"; }

    uint32_t anchor_texture_id() override { return CAMERA::BANA_ADVERT_BACKGROUND; }

    void reset(double) override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(0.0f, 0.0f); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};

struct CameraCloudFixture : public SandboxScreen::Fixture {
    std::optional<CameraCloud> active;

    CameraCloudFixture() { name = "CameraCloud"; screen = "title"; }

    uint32_t anchor_texture_id() override { return CAMERA::CAMERA_CLOUD; }

    void reset(double) override { active.emplace(); }
    void on_space(double) override { active.emplace(); }

    void update(double ms) override { if (active) active->update(ms); }
    void draw() override { if (active) active->draw(); }

    std::string controls() override { return "SPACE/L-CLICK: restart  R/R-CLICK: restart"; }
};
