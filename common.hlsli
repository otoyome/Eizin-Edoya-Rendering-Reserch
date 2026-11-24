#ifndef __COMMON_HLSLI__
#define __COMMON_HLSLI__

float2 convert_ndcspaceposition_xy_to_uv(float2 ndc)
{
    return float2(ndc.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f));
}

float2 convert_uv_to_ndcspaceposition_xy(float2 uv)
{
    return float2(uv.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f));
}

static const int dither_pattern_16 = 0;
static const int dither_pattern_64 = 1;
float calculate_alpha_dithering(float2 screen_position, int dither_pattern_index = dither_pattern_64)
{
    float dither;
    int sx = (int) screen_position.x;
    int sy = (int) screen_position.y;
    int x, y;
    switch(dither_pattern_index)
    {
        case dither_pattern_16:
        {
            static const int dither_pattern[16] =
            {
                 0,  8,  2, 10,
                12,  4, 14,  6,
                 3, 11,  1,  9,
                15,  7, 13,  5
            };
            x = sx % 4;
            y = sy % 4;
            dither = (float) dither_pattern[x + y * 4] / 16.0f;
            break;
        }
        default:
        case dither_pattern_64:
        {
            static const int dither_pattern[64] =
            {
                 0, 48, 12, 60,  3, 51, 15, 63,
                32, 16, 44, 28, 35, 19, 47, 31,
                 8, 56,  4, 52, 11, 59,  7, 55,
                40, 24, 36, 20, 43, 27, 39, 23,
                 2, 50, 14, 62,  1, 49, 13, 61,
                34, 18, 46, 30, 33, 17, 45, 29,
                10, 58,  6, 54,  9, 57,  5, 53,
                42, 26, 38, 22, 41, 25, 37, 21
            };
            x = sx % 8;
            y = sy % 8;
            dither = (float) dither_pattern[x + y * 8] / 64.0f;
            break;
        }
    }
    return dither;
}

float random(float2 seeds)
{
    return frac(sin(dot(seeds, float2(12.9898, 78.233))) * 43758.5453);
}

float block_noise(float2 seeds)
{
    return random(floor(seeds));
}

#endif  //  __COMMON_HLSLI__
