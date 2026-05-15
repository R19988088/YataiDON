#include "webcam.h"
#include <SDL3/SDL_camera.h>
#include <spdlog/spdlog.h>

WebCamera::~WebCamera() {
    close();
}

bool WebCamera::open(int device_index) {
    if (m_camera) close();

    static bool camera_init_done = false;
    if (!camera_init_done) {
        if (!SDL_InitSubSystem(SDL_INIT_CAMERA)) {
            spdlog::error("WebCamera: failed to init SDL camera subsystem: {}", SDL_GetError());
            return false;
        }
        camera_init_done = true;
    }

    int count = 0;
    SDL_CameraID* ids = SDL_GetCameras(&count);
    if (!ids || count == 0) {
        spdlog::warn("WebCamera: no camera devices found");
        SDL_free(ids);
        return false;
    }
    if (device_index >= count) {
        spdlog::warn("WebCamera: device index {} out of range ({} found)", device_index, count);
        SDL_free(ids);
        return false;
    }

    SDL_Camera* cam = SDL_OpenCamera(ids[device_index], nullptr);
    SDL_free(ids);

    if (!cam) {
        spdlog::error("WebCamera: failed to open device {}: {}", device_index, SDL_GetError());
        return false;
    }

    m_camera = cam;
    spdlog::info("WebCamera: opened device {}", device_index);
    return true;
}

void WebCamera::close() {
    if (m_texture.has_value()) {
        ray::UnloadTexture(m_texture.value());
        m_texture.reset();
    }
    if (m_camera) {
        SDL_CloseCamera(static_cast<SDL_Camera*>(m_camera));
        m_camera = nullptr;
    }
    m_width  = 0;
    m_height = 0;
}

void WebCamera::update() {
    if (!m_camera) return;

    // 0 = pending, -1 = denied
    int perm = SDL_GetCameraPermissionState(static_cast<SDL_Camera*>(m_camera));
    if (perm == -1) {
        spdlog::warn("WebCamera: permission denied");
        return;
    }
    if (perm == 0) return;

    Uint64 timestamp = 0;
    SDL_Surface* frame = SDL_AcquireCameraFrame(static_cast<SDL_Camera*>(m_camera), &timestamp);
    if (!frame) return;

    SDL_Surface* rgba = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGBA32);
    SDL_ReleaseCameraFrame(static_cast<SDL_Camera*>(m_camera), frame);

    if (!rgba) return;

    if (!m_texture.has_value()) {
        m_width  = rgba->w;
        m_height = rgba->h;

        ray::Image img{};
        img.data    = rgba->pixels;
        img.width   = m_width;
        img.height  = m_height;
        img.mipmaps = 1;
        img.format  = ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        ray::Texture2D tex = ray::LoadTextureFromImage(img);
        ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_BILINEAR);
        m_texture = tex;
    } else {
        ray::UpdateTexture(m_texture.value(), rgba->pixels);
    }

    SDL_DestroySurface(rgba);
}
