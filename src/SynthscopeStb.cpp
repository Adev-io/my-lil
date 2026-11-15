// SynthscopeStb.cpp: PNG decoding for the cockpit's frame sheets, with the SDK's stb_image compiled static into this file
// so it can never clash with the copy inside Rack.
// Author: Ari Klopfer / A_Development. MIT
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <stb_image.h>
#pragma GCC diagnostic pop
#include "Synthscope.hpp"

bool decodePng (const std::vector<uint8_t>& bytes, ss::ArtSheet& out)
{
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory (bytes.data(), (int) bytes.size(), &w, &h, &n, 4);
    if (! px) return false;
    out.w = w;
    out.h = h;
    out.rgba.assign (px, px + (size_t) w * h * 4);
    stbi_image_free (px);
    return out.ok();
}
