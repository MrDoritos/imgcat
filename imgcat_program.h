#pragma once
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <cassert>
#include <functional>
#include <chrono>

#include "../console/advancedConsole.h"
#include "colorMap.h"

extern int errno;

void error_exit(std::string text, int err);
void errno_exit(std::string text);

namespace imgcat {
    struct cpix_t : public ColorMap::Cpix<> {
        cpix_t() {}
        cpix_t(const ColorMap::Cpix<> &c) : ColorMap::Cpix<>(c),alpha(0) {}
        color_t alpha;
    };

    struct pixel_t : public ColorMap::Pixel<color_t> {
        pixel_t() {}
        pixel_t(const ColorMap::Pixel<color_t> &p) : ColorMap::Pixel<color_t>(p),a(0) {}
        color_t a;
    };

    struct Size {
        Size():width(0),height(0) { }
        Size(int width, int height):width(width),height(height) { }
        Size(const Size *size):width(size->width),height(size->height) { }
        Size(const Size &size):Size(&size) { }
        int width;
        int height;
        inline int getLength() const {
            return width * height;
        }
        inline int get(int x, int y) const {
            return y * width + x;
        }
    };

    struct Screen : public Size {
        Screen() { }
        Screen(Size size):Size(size) { }
        float characterAR;
    };

    struct Texture : public Size {
        Texture() {
            data = nullptr;
        }
        ~Texture() {
            destroy();
        }

        void make(int width, int height, int bpp = 4) {
            this->width = width;
            this->height = height;
            this->bpp = bpp;

            int length = width * height * bpp;
            destroy();
            this->data = new unsigned char[length];
        }

        void set_data(void *data) {
            assert(data && "data is null");
            external_data = true;
            this->data = (unsigned char*)data;
        }

        void load_stbi(const unsigned char* data, int width, int height, int bpp) {
            assert(data && "data is null");
            make(width, height, bpp);
            memcpy(this->data, data, getLength() * bpp);
        }

        void destroy() {
            if (external_data)
                return;
            if (data)
                delete [] data;
            data = nullptr;
        }

        bool external_data;
        unsigned char *data;
        int bpp;

        inline bool isAvailable() const {
            return data && getLength();
        }

        inline pixel_t getPixel(float x, float y) const {
            int textureX = x * width;
            int textureY = y * height;

            return *reinterpret_cast<pixel_t*>(&data[textureY * (width * bpp) + (textureX * bpp)]);
        }
    };

    struct Buffer : public Size {
        Buffer():data(nullptr) { }
        Buffer(const Texture *tex):Buffer() {
            make(tex);
        }
        Buffer(const Texture &tex):Buffer(&tex) { }
        ~Buffer() {
            destroy();
        }

        void make(const Texture *tex) {
            destroy();
            *(Size*)this = *tex;
            data = new cpix_t[tex->getLength()];
        }

        void make(const Texture &tex) {
            make(&tex);
        }

        void destroy() {
            if (data)
                delete [] data;
            data = nullptr;
        }

        inline bool isAvailable() const {
            return data && getLength();
        }

        inline cpix_t getCpix(float x, float y) const {
            assert(data && "data is null");

            int bufferX = x * width;
            int bufferY = y * height;

            return data[bufferY * width + bufferX];
        }

        cpix_t *data;
    };

    struct State {
        int imageScaleInt;
        float imageScale;
        float posX, posY;
        float newW, newH;
        float next;
        bool doubleWidth, ascii, preSampleImage, showDebug, ditherFilter;
    };

    const int LINELEN = 256;
    const int BUFFERLEN = 65536;

    struct Program {
        Texture texture;
        Buffer buffer;
        Screen screen;
        State state;
        static Program *singleton;

        private:
            char buf[LINELEN];
            char *pbuf = &buf[0];

        public:

        using conversion_func = std::function<void(color_t,color_t,color_t,wchar_t*,color_t*)>;
        conversion_func converter;

        void runPresampler() {
            char buf[LINELEN];
            char *pbuf = &buf[0];

            assert(texture.isAvailable());

            buffer.make(texture);

            for (int x = 0; x < buffer.width; x++) {
                snprintf(pbuf, LINELEN, "Presampling image... (%.0f%%)   ", x * 100.0f / (float) buffer.width);
                console::write(0,0,pbuf);                

                for (int y = 0; y < buffer.height; y++) {
                    buffer.data[buffer.get(x,y)] = sampleImage(x / (float) buffer.width, y / (float) buffer.height);
                }
            }
        }

        void processArgs(int argc, char** argv) {
            for (int i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "-p"))
                    state.preSampleImage = true;
            }
        }

        inline cpix_t conversionWrapper(pixel_t pixel) const {
            cpix_t ret;
            assert(converter && "conversion function not assigned");
            if (state.ditherFilter)
                pixel = ColorMap::Dither::get_pixel(pixel);
            converter(pixel.r, pixel.g, pixel.b, &ret.character, &ret.color);
            return ret;
        }

        inline cpix_t sampleImage(float textureX, float textureY) const {
            return conversionWrapper(texture.getPixel(textureX, textureY));
        }

        inline cpix_t sampleImage(int screenX, int screenY) const {
            float relX = screenX / (float) screen.width;
            float relY = screenY / (float) screen.height;
            
            return sampleImage(relX, relY);
        }

        inline cpix_t sampleImage(float textureX, float textureY, bool preSampled) const {
            if (preSampled && buffer.isAvailable()) {
                return buffer.getCpix(textureX, textureY);
            } else {
                return sampleImage(textureX, textureY);
            }
        }

        void setCharacterAR() {
            screen.characterAR = 1.0f;
            if (!adv::doubleSize)
                screen.characterAR = 2.0f;

            int w = screen.width, h = screen.height, r = screen.characterAR;
            int tw = texture.width, th = texture.height;

            state.newW = w;
            state.newH = h;

            if ((w * r) / h > (float) tw / th)
                state.newW = (tw * h) / (float) th;
            else    
                state.newH = (th * w) / (float) tw;
        }

        void drawImage() {
            std::lock_guard<std::mutex> lk(adv::buffers);

            float nW = state.newW * state.imageScale * screen.characterAR;
            float nH = state.newH * state.imageScale;

            float sampleOffsetX = -(screen.width/nW)*0.5f + state.posX;
            float sampleOffsetY = -(screen.height/nH)*0.5f + state.posY;

            for (int x = 0; x < screen.width; x++) {
                for (int y = 0; y < screen.height; y++) {
                    float sampleX = x / nW + sampleOffsetX;
                    float sampleY = y / nH + sampleOffsetY;

                    if (sampleX >= 1. || sampleY >= 1. || sampleX < 0. || sampleY < 0.)
                        continue;

                    cpix_t cpix = sampleImage(sampleX, sampleY, state.preSampleImage);

                    int offset = y * screen.width + x;
                    adv::fb[offset] = cpix.character;
                    adv::cb[offset] = cpix.color;
                }
            }

            adv::modify = true;
        }

        void drawDebug() {
            if (state.showDebug) {
                snprintf(pbuf, LINELEN, "s: %.2f si: %i sn: %.2f xn: %.4f yn: %.4f n: %.4f w: %i h: %i", state.imageScale, state.imageScaleInt, 1.0f / state.imageScale, state.posX, state.posY, state.next, screen.width, screen.height);
                console::write(0,0,pbuf,FBLACK|BWHITE);
            }
        }

        void draw() {
            consoleUpdateScreen();
            adv::clear();
            drawImage();
            adv::draw();
            drawDebug();
        }

        void keyboard(int key) {
            float imageScaleI = 0.4f / state.imageScale;

            switch (key) {
			case VK_RIGHT:
			case 'd':
			case 'D':
					state.posX += imageScaleI;
				break;
			case VK_LEFT:
			case 'a':
			case 'A':
					state.posX -= imageScaleI;
				break;
			case VK_DOWN:
			case 's':
			case 'S':
					state.posY += imageScaleI;
				break;
			case VK_UP:
			case 'w':
			case 'W':
					state.posY -= imageScaleI;
				break;
			case 'x':
			case 'X':
			case '.':
				state.imageScaleInt++;
				if (state.imageScaleInt < 1) {
					state.imageScale = abs(state.imageScaleInt - 2);
				} else {
					state.imageScale = 1.0f / float(state.imageScaleInt);
				}
				
				if (state.imageScaleInt > 0) {
					state.next = 0.25f * (1.0f / abs(state.imageScaleInt + 1));
					//cur = 0.5f * (1.0f / fabs(imageScale));
				}
				else
					if (state.imageScaleInt + 1 != 0)
						state.next = (1/32.0f) * (1.0f / abs(state.imageScaleInt + 1));
					else
						state.next = (1/32.0f);
				break;
			case 'z':
			case 'Z': //Working
			case ',':
				state.imageScaleInt--;
				if (state.imageScaleInt < 1)
					state.imageScale = abs(state.imageScaleInt - 2);
				else					
					state.imageScale = 1.0f / float(state.imageScaleInt);
				break;
            case '0':
                reset();
                break;
            case '1':
                adv::setDoubleWidth(state.doubleWidth = !state.doubleWidth);
                consoleUpdateScreen();
                break;
            case '2':
                adv::setAscii(state.ascii = !state.ascii);
                setCharacterAR();
                break;
            case '3':
                state.ditherFilter = !state.ditherFilter;
                break;
            case 'l':
                state.showDebug = !state.showDebug;
                break;
            }
        }

        void consoleUpdateScreen() {
            adv::isNewSize();
            screen.width = adv::width;
            screen.height = adv::height;
        }

        void consoleInit() {
            consoleUpdateScreen();

            init();
        }

        void init() {
            state.preSampleImage = false;
            state.showDebug = false;

            reset();

            adv::setThreadState(false);
            adv::setThreadSafety(false);
        }

        void reset() {
            state.imageScaleInt = 1;
            state.imageScale = 1.0f;

            state.posX = 0.5f;
            state.posY = 0.5f;

            state.doubleWidth = false;
            state.ascii = true;
            state.ditherFilter = false;

            setCharacterAR();
        }

        void destroy() {
            adv::_advancedConsoleDestruct();
            console::cons.~constructor();
        }

        ~Program() {
            destroy();
            singleton = nullptr;
        }

        Program() {
            singleton = this;
        }
    };

    Program *Program::singleton = nullptr;

    struct GreedyScheduler {
        GreedyScheduler() {
            if (skip)
                return;

            if (sched_getparam(0, &param))
                errno_exit("sched_getparam");

            param.sched_priority = 20;
            if (sched_setscheduler(0, SCHED_FIFO, &param)) {
                if (errno == EPERM)
                    skip = true;
                else
                    errno_exit("sched_setscheduler fifo");
            }
        }
        ~GreedyScheduler() {
            if (skip)
                return;

            param.sched_priority = 0;
            if (sched_setscheduler(0, SCHED_OTHER, &param))
                errno_exit("sched_setscheduler other");
        }
        sched_param param;
        static bool skip;
    };

    bool GreedyScheduler::skip = false;
}

void error_exit(std::string text, int ret=-1) {
    if (imgcat::Program::singleton)
        imgcat::Program::singleton->~Program();
    std::cout << text << std::endl;
    exit(ret);
}

void errno_exit(std::string text) {
    int errsv = errno;
    if (imgcat::Program::singleton)
        imgcat::Program::singleton->~Program();
    std::cout << text << " (" << strerror(errsv) << ") " << std::endl;
    exit(errno);
}