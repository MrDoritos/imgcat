#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <stdio.h>
#include "colorMap.h"

#define COLORMAPPINGPALETTE

namespace ColorMappingPalette {

static unsigned char *palette_map = nullptr;
static wchar_t *palette_lut = nullptr;

static const int bitDepth = 4;
static const int trunDepth = 8 - bitDepth;
static const int compSize = 1 << (bitDepth);
static const int mapSize = (1 << (bitDepth * 3)) * 2;

struct pixel_v {
    unsigned char r, g, b, _sat8, _hue8;

    pixel_v() {}
    pixel_v(unsigned char r, unsigned char g, unsigned char b):r(r),g(g),b(b) {
        calc_sat();
        calc_hue();
    }

    inline void set_r(unsigned char r) {
        this->r = r;
    }

    inline void set_g(unsigned char g) {
        this->g = g;
    }

    inline void set_b(unsigned char b) {
        this->b = b;
    }

    inline unsigned char min() const {
        return ColorMap::Pixel<>::min(r,g,b);
    }

    inline unsigned char max() const {
        return ColorMap::Pixel<>::max(r,g,b);
    }

    inline void calc_sat() {
        this->_sat8 = max() - min();
    }

    inline void calc_hue() {
        float _r = r / 255.0f;
        float _g = g / 255.0f;
        float _b = b / 255.0f;
        auto _max = this->max();
        auto _min = this->min();
        float _c = (_max - _min) / 255.0f, segment = 0, shift = 0;

        if (_c == 0.0f) {
            _hue8 = 0;
            return;
        }

        if (r == _max) {
            segment = (_g - _b) / _c;
            shift = 0;
            if (segment < 0)
                shift = 360 / 60;
        }

        if (g == _max) {
            segment = (_b - _r) / _c;
            shift = 120 / 60;
        }

        if (b == _max) {
            segment = (_r - _g) / _c;
            shift = 240 / 60;
        }

        _hue8 = (segment + shift) * (255.0f / (3.14159f * 2.0f));
    }

    inline unsigned char sat8() const {
        return _sat8;
    }

    inline float hue() const {
        return _hue8 / 255.0f;
    }

    inline float sat() const {
        return _sat8 / 255.0f;
    }

    inline int distance(unsigned char r, unsigned char g, unsigned char b) const {
        return distance({r,g,b});
    }

    inline int distance(pixel_v v) const {
        return ColorMap::Pixel<>::distance(r, g, b, v.r, v.g, v.b);
    }
};

struct char_val {
    char_val() {}
    char_val(wchar_t ch, float v):character(ch),value(v) {}

    wchar_t character;
    float value;
};

struct pix_char : public pixel_v {
    pix_char() {}
    pix_char(wchar_t ch, unsigned char color, const pixel_v &c):pixel_v(c),color(color),character(ch) {}

    unsigned char color;
    wchar_t character;
};

struct palette_v {
    palette_v() {}
    palette_v(char_val ch, pix_char fg, pix_char bg)
    :ch(ch),fg(fg),bg(bg),fgpix(fg),bgpix(bg) {
        calc_lerp();
    }
    char_val ch;
    pix_char fg, bg;
    pixel_v lpix, fgpix, bgpix;

    inline void set_fg(const pix_char &fg) {
        this->fg = fg;
        this->fgpix = fg;
        calc_lerp();
    }

    inline void set_bg(const pix_char &bg) {
        this->bg = bg;
        this->bgpix = bg;
        calc_lerp();
    }

    inline void set_ch(const char_val &ch) {
        this->ch = ch;
        calc_lerp();
    }

    inline void calc_lerp() {
        lpix = pixel_v{r(),g(),b()};
    }

    inline unsigned char r() const {
        return ColorMap::lerp<float>(fg.r, bg.r, ch.value);
    }

    inline unsigned char g() const {
        return ColorMap::lerp<float>(fg.g, bg.g, ch.value);
    }

    inline unsigned char b() const {
        return ColorMap::lerp<float>(fg.b, bg.b, ch.value);
    }

    inline unsigned char color() const {
        return bg.color | (fg.color >> 4);
    }

    inline int distance(const palette_v &v) const {
        return distance(v.lpix);
    }

    inline int distance(const unsigned char &vr, const unsigned char &vg, const unsigned char &vb) const {
        return distance({vr, vg, vb});
    }

    inline int distance(const pixel_v &pv) const {
        // An unsaturated pv color and saturated pfg+pbg should not be close to each other
        if (pv.sat() < (fgpix.sat() + bgpix.sat()) * 0.1f)
            return 10000;

        //if (pv.sat() < 0.25 && pv.sat() < (pfg.sat() + pbg.sat()) * 0.25f)
        //    return 1000.0;

        return lpix.distance(pv);
    }
};

struct palette_loader {
    std::vector<char_val> charv_table;
    std::vector<pix_char> color_table;
    std::vector<palette_v> palette_table;

    int load_file(const char *filename) {
        FILE *fp = fopen(filename, "r");

        if (!fp) {
            fprintf(stderr, "Failed to open palette file\n");
            return -1;
        }

        const int LINELEN = 256;
        char buf[LINELEN];
        char *pbuf = &buf[0];

        while ((fgets(pbuf, LINELEN, fp)) != nullptr) {
            char t, _c;
            int c, ch, co, r, g, b;
            float v;

            if (sscanf(pbuf, "%c", &t) != 1) {
                continue;
            }

            if (t == 'p') {
                if (sscanf(pbuf, "%c,%i,%i,%i,%i,%i,%i", &t, &c, &ch, &co, &r, &g, &b) != 7) {
                    continue;
                }
    
                pix_char pc;
                pc.color = co;
                pc.r = r;
                pc.g = g;
                pc.b = b;
                color_table.push_back(pc);
            }

            if (t == 'c') {
                if (sscanf(pbuf, "%c,%i,%c,%f", &t, &ch, &_c, &v) != 4) {
                    continue;
                }
    
                char_val cv;
                cv.character = (wchar_t)ch;
                cv.value = v;
                charv_table.push_back(cv);
            }
        }

        fclose(fp);

        if (charv_table.size() < 1 || color_table.size() < 1) {
            fprintf(stderr, "Not enough parameters to generate a palette\n");
            return -1;
        }

        generate_tables();

        return 0;
    }

    void load_builtin() {
        char_val chv[] = {
            {9618,  0.50f},
            {9617,  0.75f},
            {32,    1.00f}
        };
    
        pix_char pxc[] = {
            {0, 0,   {0  , 0  , 0  }},
            {0, 64,  {127, 0  , 0  }},
            {0, 128, {198, 198, 198}},
            {0, 192, {204, 0  , 0  }},
            {0, 16,  {0  , 0  , 127}},
            {0, 80,  {127, 0  , 127}},
            {0, 144, {0  , 0  , 204}},
            {0, 208, {204, 0  , 204}},
            {0, 32,  {0  , 127, 0  }},
            {0, 96,  {0  , 127, 127}},
            {0, 160, {0  , 204, 0  }},
            {0, 224, {0  , 204, 204}},
            {0, 48,  {127, 127, 0  }},
            {0, 112, {127, 127, 127}},
            {0, 176, {204, 204, 0  }},
            {0, 240, {255, 255, 255}}
        };

        for (int i = 0; i < sizeof chv / sizeof chv[0]; i++)
            charv_table.push_back(chv[i]);

        for (int i = 0; i < sizeof pxc / sizeof pxc[0]; i++)
            color_table.push_back(pxc[i]);

        generate_tables();
    }

    void generate_tables() {
        int colorCount = color_table.size();
        int charvCount = charv_table.size();

        palette_table.reserve(colorCount * colorCount * charvCount);

        for (int li = 0; li < charvCount; li++) {
            auto &c = charv_table[li];
            
            for (int fg = 0; fg < colorCount; fg++) {
                for (int bg = 0; bg < colorCount; bg++) {
                    pix_char pfg = color_table[fg];
                    pix_char pbg = color_table[bg];

                    if (pfg.color == pbg.color && c.value < 1.0)
                        continue;

                    if (pfg.color == pbg.color && pfg.color == 0xF0)
                        continue;

                    if (pfg.color != pbg.color && c.value >= 1.0)
                        continue;

                    palette_table.push_back({c,pfg,pbg});
                }
            }
        }
    }

    void generate_lut(unsigned char *&lut, wchar_t *&cht) {
        int charvCount = charv_table.size();
        int paletteCount = palette_table.size();

        cht = new wchar_t[charvCount];
        lut = new unsigned char[mapSize];

        for (int i = 0; i < charvCount; i++)
            cht[i] = charv_table[i].character;

        auto &lut0 = palette_table[0];

        for (int r = 0; r < compSize; r++) {
            for (int g = 0; g < compSize; g++) {
                for (int b = 0; b < compSize; b++) {
                    const int pr = (r << trunDepth) + int((1 << trunDepth) * 0.5);
                    const int pg = (g << trunDepth) + int((1 << trunDepth) * 0.5);
                    const int pb = (b << trunDepth) + int((1 << trunDepth) * 0.5);

                    const int index = ((r << (bitDepth * 2)) | (g << (bitDepth)) | (b)) << 1;
                    const pixel_v ppix(pr,pg,pb);

                    int nearest_value = lut0.distance(ppix);
                    auto &near = lut0;
                    unsigned char li;

                    for (int i = 1; i < paletteCount; i++) {
                        int value = palette_table[i].distance(ppix);
                        if (value < nearest_value) {
                            nearest_value = value;
                            near = palette_table[i];
                        }
                    }

                    lut[index + 1] = near.color();

                    for (int i = 0; i < charvCount; i++) {
                        if (near.ch.character == charv_table[i].character) {
                            lut[index] = (unsigned char)i;
                            break;
                        }
                    }
                }
            }
        }
    }
};

void colormapper_init_table() {
    palette_loader palette;
    if (palette.load_file("palette.csv"))
        palette.load_builtin();

    palette.generate_lut(palette_map, palette_lut);
}

inline void getDitherColored(unsigned char r, unsigned char g, unsigned char b, wchar_t *character, unsigned char *color) {
    const int tr = r >> trunDepth, tg = g >> trunDepth, tb = b >> trunDepth;
	const int inm = ((tr << (bitDepth * 2)) | (tg << bitDepth) | (tb)) << 1;
	const unsigned char *index = &palette_map[inm];
	
	*character = palette_lut[*index];
	*color = *(index+1);
}

struct Palette : public ColorMap::Mapper<> {
    Palette():ColorMap::Mapper<>("Palette") {}

    ColorMap::colorMappingFunction get_function() override {
        return ColorMappingPalette::getDitherColored;
    }

    void init() override {
        ColorMappingPalette::colormapper_init_table();
    }
};

struct PaletteSlow : public ColorMap::Mapper<> {
    PaletteSlow():ColorMap::Mapper<>("PaletteSlow") {}

    static palette_loader palette;

    ColorMap::colorMappingFunction get_function() override {
        return getDitherColored;
    }

    static void getDitherColored(unsigned char r, unsigned char g, unsigned char b, wchar_t *character, unsigned char *color) {
        auto &table = palette.palette_table;
        auto table_size = table.size();
        auto ppix = pixel_v(r,g,b);

        auto &near = table[0];
        int nearest_value = near.distance(ppix);

        for (int i = 1; i < table_size; i++) {
            int value = table[i].distance(ppix);
            if (value < nearest_value) {
                nearest_value = value;
                near = table[i];
            }
        }

        *color = near.color();
        *character = near.ch.character;
    }

    void init() override {
        if (palette.load_file("palette.csv"))
            palette.load_builtin();
    }
};

palette_loader PaletteSlow::palette;

}

auto colorMapPalette = ColorMappingPalette::Palette();
auto colorMapPaletteSlow = ColorMappingPalette::PaletteSlow();
auto colorMappingPalette = ColorMappingPalette::getDitherColored;
auto colorMappingPaletteInit = ColorMappingPalette::colormapper_init_table;
#undef getDitherColored
#undef colormapper_init_table
#define getDitherColored colorMappingPalette
#define colormapper_init_table colorMappingPaletteInit