#include "chara_3d.h"
#include "../../libs/global_data.h"
#include "raylib.h"
#include <cmath>

extern "C" void rlEnableDepthTest(void);
extern "C" void rlDisableDepthTest(void);
extern "C" void rlDrawRenderBatchActive(void);
extern "C" void glClear(unsigned int mask);
extern "C" void rlDisableBackfaceCulling(void);
extern "C" void rlEnableBackfaceCulling(void);
extern "C" void glCullFace(unsigned int mode);
static constexpr unsigned int GL_FRONT = 0x0404;
static constexpr unsigned int GL_BACK  = 0x0405;
static constexpr unsigned int GL_DEPTH_BUFFER_BIT = 0x00000100;


static ray::Matrix rotation_xyz(float ax, float ay, float az) {
    float cx = cosf(-ax), sx = sinf(-ax);
    float cy = cosf(-ay), sy = sinf(-ay);
    float cz = cosf(-az), sz = sinf(-az);
    ray::Matrix r = {};
    r.m0 = cz*cy;  r.m1 = (cz*sy*sx) - (sz*cx);  r.m2 = (cz*sy*cx) + (sz*sx);
    r.m4 = sz*cy;  r.m5 = (sz*sy*sx) + (cz*cx);   r.m6 = (sz*sy*cx) - (cz*sx);
    r.m8 = -sy;    r.m9 = cy*sx;                   r.m10 = cy*cx;
    r.m15 = 1.0f;
    return r;
}

static void reindex_animations(ray::Model& model, ray::Model& glb_model,
                               ray::ModelAnimation* anims, int anim_count) {
    std::unordered_map<std::string, int> glb_bone_idx;
    for (int i = 0; i < glb_model.skeleton.boneCount; i++)
        glb_bone_idx[glb_model.skeleton.bones[i].name] = i;

    int n = model.skeleton.boneCount;

    for (int a = 0; a < anim_count; a++) {
        auto& anim = anims[a];
        ray::ModelAnimPose* new_poses =
            (ray::ModelAnimPose*)std::malloc(anim.keyframeCount * sizeof(ray::ModelAnimPose));

        for (int f = 0; f < anim.keyframeCount; f++) {
            new_poses[f] = (ray::Transform*)std::malloc(n * sizeof(ray::Transform));
            for (int b = 0; b < n; b++) {
                auto it = glb_bone_idx.find(model.skeleton.bones[b].name);
                if (it != glb_bone_idx.end() && it->second < anim.boneCount)
                    new_poses[f][b] = anim.keyframePoses[f][it->second];
                else
                    new_poses[f][b] = model.skeleton.bindPose[b];
            }
            std::free(anim.keyframePoses[f]);
        }
        std::free(anim.keyframePoses);
        anim.keyframePoses = new_poses;
        anim.boneCount = n;
    }
}

Chara3D::Chara3D(std::string& model_name) {
    outline_shader = ray::LoadShader("shader/outline.vs", "shader/outline.fs");
    fs::path root_path = fs::path("Skins") / global_data.config->paths.skin / "Models";
    fs::path model_path = root_path / (model_name + ".gltf");
    fs::path anim_path = root_path / "animations.glb";
    model = ray::LoadModel(model_path.string().c_str());
    ray::Model glb_model = ray::LoadModel(anim_path.string().c_str());
    anims = ray::LoadModelAnimations(anim_path.string().c_str(), &anim_count);
    reindex_animations(model, glb_model, anims, anim_count);
    ray::UnloadModel(glb_model);
    fs::path texture_path = "Skins/PyTaikoGreen/Models/faces/0/4.png";
    set_face_texture(texture_path);
    set_body_colors({104, 191, 192, 255}, {249, 240, 225, 255});
    set_face_colors({249, 71, 40, 255}, {249, 240, 225, 255});
}

Chara3D::~Chara3D() {
    ray::UnloadModelAnimations(anims, anim_count);
    ray::UnloadModel(model);
    ray::UnloadShader(outline_shader);
}

void Chara3D::set_texture(fs::path& texture_path, int material_index) {
    ray::Texture2D old = model.materials[material_index].maps[ray::MATERIAL_MAP_DIFFUSE].texture;
    if (old.id != 0) ray::UnloadTexture(old);
    ray::Texture2D tex = ray::LoadTexture(texture_path.string().c_str());
    int map_type = ray::MATERIAL_MAP_DIFFUSE;
    ray::SetMaterialTexture(&model.materials[material_index], map_type, tex);
}

void Chara3D::set_body_texture(fs::path& texture_path) {
    set_texture(texture_path, model.materialCount - 3);
}

void Chara3D::set_face_rim_texture(fs::path& texture_path) {
    set_texture(texture_path, model.materialCount - 2);
}

void Chara3D::set_face_texture(fs::path& texture_path) {
    set_texture(texture_path, model.materialCount - 1);
}

static ray::Texture2D recolor_texture(ray::Image& source,
                                       ray::Color from_a, ray::Color to_a,
                                       ray::Color from_b, ray::Color to_b) {
    ray::Image img = ray::ImageCopy(source);
    ray::ImageFormat(&img, ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    unsigned char* pixels = (unsigned char*)img.data;
    int total = img.width * img.height;

    for (int i = 0; i < total; i++) {
        float r = pixels[i * 4 + 0] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        float b = pixels[i * 4 + 2] / 255.0f;
        float a = pixels[i * 4 + 3] / 255.0f;

        float brightness = r + g + b;
        if (brightness < 0.1f) continue;

        auto similarity = [](float r, float g, float b, ray::Color c) {
            float cr = c.r / 255.0f, cg = c.g / 255.0f, cb = c.b / 255.0f;
            float len = sqrtf(cr*cr + cg*cg + cb*cb);
            if (len == 0) return 0.0f;
            return (r*cr + g*cg + b*cb) / len;
        };

        float wa = similarity(r, g, b, from_a);
        float wb = similarity(r, g, b, from_b);
        float total_w = wa + wb;
        if (total_w == 0) continue;
        wa /= total_w;
        wb /= total_w;

        float intensity = brightness / 3.0f;

        float nr = (wa * (to_a.r / 255.0f) + wb * (to_b.r / 255.0f)) * intensity * 3.0f;
        float ng = (wa * (to_a.g / 255.0f) + wb * (to_b.g / 255.0f)) * intensity * 3.0f;
        float nb = (wa * (to_a.b / 255.0f) + wb * (to_b.b / 255.0f)) * intensity * 3.0f;

        pixels[i * 4 + 0] = (unsigned char)(fminf(nr, 1.0f) * 255.0f);
        pixels[i * 4 + 1] = (unsigned char)(fminf(ng, 1.0f) * 255.0f);
        pixels[i * 4 + 2] = (unsigned char)(fminf(nb, 1.0f) * 255.0f);
        pixels[i * 4 + 3] = (unsigned char)(a * 255.0f);
    }

    ray::Texture2D result = ray::LoadTextureFromImage(img);
    ray::UnloadImage(img);
    return result;
}

void Chara3D::set_body_colors(ray::Color body, ray::Color rim) {
    auto& map = model.materials[model.materialCount - 3].maps[ray::MATERIAL_MAP_DIFFUSE];
    ray::Image orig_body_img = ray::LoadImageFromTexture(map.texture);
    ray::Texture2D new_tex = recolor_texture(orig_body_img,
        {255, 0, 0, 255}, body,    // red -> body color
        {0, 0, 255, 255}, rim);    // blue -> rim color
    ray::UnloadTexture(map.texture);
    map.texture = new_tex;
}

void Chara3D::set_face_colors(ray::Color face, ray::Color rim) {
    auto& map = model.materials[model.materialCount - 2].maps[ray::MATERIAL_MAP_DIFFUSE];
    ray::Image orig_face_img = ray::LoadImageFromTexture(map.texture);
    ray::Texture2D new_tex = recolor_texture(orig_face_img,
        {0, 255, 0, 255}, face,    // green -> face color
        {0, 0, 255, 255}, rim);    // blue -> rim color
    ray::UnloadTexture(map.texture);
    map.texture = new_tex;
}

void Chara3D::set_anim(int idx) {
    if (idx >= 0 && idx < anim_count) {
        if (idx == 20 || idx == 21) {
            is_looping = true;
        } else if (get_anim_name(idx).find("loop") == std::string::npos) {
            is_looping = false;
            prev_anim_idx = anim_index;
        }
        anim_index = idx;
        anim_frame = 0;
        last_frame_ms = 0;
    }
}

std::string Chara3D::get_anim_name(int idx) {
    if (idx >= 0 && idx < anim_count) {
        return anims[idx].name;
    }
    return "";
}

void Chara3D::set_bpm(float bpm) {
    this->bpm = bpm;
}

int Chara3D::get_anim_count() const {
    return anim_count;
}

void Chara3D::update(double current_ms) {
    if (anim_count > 0) {
        double ms_per_beat = 60000.0 / bpm;
        if (anim_index == 20 || anim_index == 21) ms_per_beat *= 3;
        double ms_per_frame = ms_per_beat / anims[anim_index].keyframeCount;
        if (current_ms - last_frame_ms >= ms_per_frame) {
            int loop_frames = anims[anim_index].keyframeCount - 1;
            anim_frame = (anim_frame + 1) % loop_frames;
            ray::UpdateModelAnimation(model, anims[anim_index], anim_frame);
            last_frame_ms = current_ms;

            if (!is_looping && anim_frame == loop_frames - 1) {
                set_anim(prev_anim_idx);
                is_looping = true;
            }
        }
    }
}

void Chara3D::draw(float x, float y) {
    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BUFFER_BIT);
    rlEnableDepthTest();

    ray::Matrix saved = model.transform;
    ray::Matrix transform = rotation_xyz(rot_x * DEG2RAD, rot_y * DEG2RAD, rot_z * DEG2RAD);
    model.transform = transform;

    ray::Shader saved_shaders[model.materialCount];
    for (int i = 0; i < model.materialCount; i++) {
        saved_shaders[i] = model.materials[i].shader;
        model.materials[i].shader = outline_shader;
    }

    int thickness_loc = ray::GetShaderLocation(outline_shader, "outlineThickness");
    ray::SetShaderValue(outline_shader, thickness_loc, &outline_thickness, ray::SHADER_UNIFORM_FLOAT);

    int screen_size_loc = ray::GetShaderLocation(outline_shader, "screenSize");
    float screen_size[2] = { (float)ray::GetScreenWidth(), (float)ray::GetScreenHeight() };
    ray::SetShaderValue(outline_shader, screen_size_loc, screen_size, ray::SHADER_UNIFORM_VEC2);

    rlDisableDepthTest();
    glCullFace(GL_FRONT);
    ray::DrawModel(model, {x, y, 400.0f}, scale, ray::WHITE);
    glCullFace(GL_BACK);

    for (int i = 0; i < model.materialCount; i++)
        model.materials[i].shader = saved_shaders[i];

    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BUFFER_BIT);
    rlEnableDepthTest();
    ray::DrawModel(model, {x, y, 400.0f}, scale, ray::WHITE);

    model.transform = saved;
    rlDisableDepthTest();
}
