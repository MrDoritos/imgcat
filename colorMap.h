#pragma once
#include <vector>
#include <string>
#include <functional>

namespace ColorMap {
    using color_t = unsigned char;
    using char_t = wchar_t;

    template<typename T>
    constexpr inline T lerp(T v0, T v1, T fact) {
        return (1 - fact) * v0 + fact * v1;
    }
    
    template<typename T>
    constexpr inline T max(T a, T b) {
        return a > b ? a : b;
    }
    
    template<typename T>
    constexpr inline T min(T a, T b) {
        return a < b ? a : b;
    }

    template<typename c_type = color_t>
    struct Pixel {
        Pixel() {}
        Pixel(const color_t &r, const color_t &g, const color_t &b):r(r),g(g),b(b) {}
        color_t r, g, b;

        template<typename RESULT = int, typename CAST = int>
        constexpr inline RESULT distance(const Pixel &v) const {
            return distance<RESULT, CAST>(v.r, v.g, v.b);
        }

        template<typename RESULT = int, typename INPUT = int>
        constexpr inline RESULT distance(const INPUT rv, const INPUT gv, const INPUT bv) {
            return distance<RESULT, INPUT>(r, g, b, rv, gv, bv);
        }

        template<typename RESULT = int, typename INPUT = int>
        static constexpr inline RESULT distance(const INPUT r0, const INPUT g0, const INPUT b0, const INPUT r1, const INPUT g1, const INPUT b1) {
            return (r1 - r0) * (r1 - r0) +
                   (g1 - g0) * (g1 - g0) +
                   (b1 - b0) * (b1 - b0);
        }

        inline unsigned char min() const {
            return min(r,g,b);
        }
    
        inline unsigned char max() const {
            return max(r,g,b);
        }

        template<typename RESULT = color_t, typename INPUT = color_t>
        static constexpr inline RESULT min(const INPUT r, const INPUT g, const INPUT b) {
            return ColorMap::min(ColorMap::min(r,g),b);
        }

        template<typename RESULT = color_t, typename INPUT = color_t>
        static constexpr inline RESULT max(const INPUT r, const INPUT g, const INPUT b) {
            return ColorMap::max(ColorMap::max(r,g),b);
        }
    };

    template<typename ch_type = char_t, typename c_type = color_t>
    struct Cpix {
        Cpix() {}
        Cpix(const ch_type &character, const c_type &color):character(character),color(color) {}
        c_type color;
        ch_type character;
    };

    using colorMappingFunction = std::function<void(unsigned char,unsigned char,unsigned char,wchar_t*,unsigned char*)>;

    template<typename cpix_type = Cpix<>, typename pixel_type = Pixel<>>
    struct Mapper {
        std::string mapper_name;
        colorMappingFunction mapper_function;

        Mapper(const std::string &name = "");
        ~Mapper();

        void base_init() {
            this->mapper_function = this->get_function();
            this->init();
        }

        void base_destroy() {
            this->destroy();
        }

        virtual colorMappingFunction get_function() = 0;
        virtual void destroy() {}
        virtual void init() {}

        virtual std::string get_name() {
            return mapper_name;
        }

        virtual void set_function(colorMappingFunction function) {
            mapper_function = function;
        }

        constexpr inline cpix_type get(const pixel_type &pix) const {
            cpix_type ret;
            mapper_function(pix.r, pix.g, pix.b, &ret.character, &ret.color);
            return ret;
        }
    };

    static std::vector<Mapper<>*> mappers;
    static int mappers_index = 0;

    static Mapper<>* next() {
        if (++mappers_index + 1 > mappers.size())
            mappers_index = 0;

        return mappers.at(mappers_index);
    }

    static Mapper<>* prev() {
        if (--mappers_index < 0)
            mappers_index = mappers.size() - 1;

        return mappers.at(mappers_index);
    }

    static Mapper<>* current() {
        return mappers.at(mappers_index);
    }

    static const std::vector<Mapper<>*>& all() {
        return mappers;
    }

    static inline Cpix<> get(const Pixel<> &pix) {
        return current()->get(pix);
    }

    static inline std::string get_name() {
        return current()->get_name();
    }

    static inline colorMappingFunction get_function() {
        return current()->get_function();
    }

    static void init() {
        for (auto *mapper : mappers)
            mapper->base_init();
    }

    static void destroy() {
        for (auto *mapper : mappers)
            mapper->destroy();
    }

    template<>
    Mapper<>::Mapper(const std::string &name) : mapper_name(name) {
        mappers.push_back(this);
    }

    template<>
    Mapper<>::~Mapper() {
        mappers.erase(std::remove(mappers.begin(), mappers.end(), this));
    }

    struct Dither {
        using rand_t = char;
        static rand_t ex_seed;

        static inline rand_t sample_normal() {
            ex_seed = (5 * ex_seed + 127);
            return (((ex_seed >> ((ex_seed & 0b1100) >> 2)) >> 4) & 0xFE) + 1;
        }

        static inline void set_srand(rand_t v) {
            ex_seed = v;
        }

        static void set_mapper(const Mapper<> *mapper) {

        }

        static inline Cpix<> get(const Pixel<> &pix_in) {
            return ColorMap::get(get_pixel(pix_in));
        }

        static inline Pixel<> get_pixel(const Pixel<> &pix_in) {
            char r = sample_normal();
            return {
                color_t((pix_in.r & 0xF0) + 7 + r),
                color_t((pix_in.g & 0xF0) + 7 + r),
                color_t((pix_in.b & 0xF0) + 7 + r)
            };
        }
    };

    Dither::rand_t Dither::ex_seed;
}