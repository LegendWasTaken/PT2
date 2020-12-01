//
// Created by legend on 12/2/20.
//

#pragma once

#include <cstdint>

#include <pt2/vec3.h>

namespace pt2
{
    class Image
    {
    public:
        Image() = delete;

        Image(int width, int height, const uint8_t *data);

        [[nodiscard]] auto width() const noexcept -> uint32_t;

        [[nodiscard]] auto height() const noexcept -> uint32_t;

        [[nodiscard]] auto get(float u, float v) const noexcept -> uint32_t;

        [[nodiscard]] auto get(uint32_t x, uint32_t y) const noexcept -> uint32_t;

    private:
        uint32_t              _width;
        uint32_t              _height;
        std::vector<uint32_t> _data;
    };

    struct SceneRenderDetail
    {
        uint8_t  max_bounces  = 5;
        uint8_t  thread_count = 1;
        uint16_t spp          = 4;
        uint16_t width        = 512;
        uint16_t height       = 512;
        struct
        {
            Vec3 origin;
            Vec3 lookat;
        } camera;
    };
}    // namespace pt2
