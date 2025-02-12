#include "colorMappingDitherFast.h"
#include "colorMappingCacheTest.h"
#include "colorMappingFast.h"
#include "colorMappingFaster.h"
#include "colorMappingDither.h"

#include <vector>
#include <string>

using func_colorMapping = std::function<void(unsigned char,unsigned char,unsigned char,wchar_t*,unsigned char*)>;

std::vector<std::pair<std::string, func_colorMapping>>
getMappingFuncs() {
    std::vector<std::pair<std::string, func_colorMapping>> funcs;

    #ifdef COLORMAPPINGDITHERFAST
    colorMappingDitherFastInit();
    funcs.push_back({"colorMappingDitherFast", colorMappingDitherFast});
    #endif

    #ifdef COLORMAPPINGCACHETEST
    colorMappingCacheTestInit();
    funcs.push_back({"colorMappingCacheTest", colorMappingCacheTest});
    #endif

    #ifdef COLORMAPPINGFAST
    colorMappingFastInit();
    funcs.push_back({"colorMappingFast", colorMappingFast});
    #endif

    #ifdef COLORMAPPINGFASTER
    colorMappingFasterInit();
    funcs.push_back({"colorMappingFaster", colorMappingFaster});
    #endif

    #ifdef COLORMAPPINGDITHER
    colorMappingDitherInit();
    funcs.push_back({"colorMappingDither", colorMappingDither});
    #endif

    if (funcs.size() < 1) {
        colormapper_init_table();
        funcs.push_back({"getDitherColored", getDitherColored});
    }

    return funcs;
}
