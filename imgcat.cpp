#include "imgcat_program.h"

//#include "colorMappingDither.h" //4s
//#include "colorMappingFaster.h" //0.2s
//#include "colorMappingCacheTest.h"
//#include "colorMappingDitherFast.h"
#include "colorMappingCombined.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LINELEN 256
#define BUFFERLEN 65536

#ifdef __linux__
int wmain(int argc, char** argv) {
#else
int wmain(int argc, wchar_t** argv) {
#endif
    imgcat::Program program;
	bool useArgs = argc > 1;
	bool isPipeInput = !isatty(fileno(stdin));
    unsigned char *stbi_image_data;
	FILE* fp;
	char buf[LINELEN];
	char *pbuf = &buf[0];

	if (!useArgs && !isPipeInput)
		error_exit("Pipe or filename required");

	if (isPipeInput) {
		std::vector<unsigned char> data;
		unsigned char line[BUFFERLEN];
		int count;

		// Open the pipe as binary input
		fp = freopen(NULL, "rb", stdin);
		#if defined __WIN32
		_setmode(_fileno(stdin), _O_BINARY);
		#endif

		// Copy piped input into vector
		while ((count = fread(line, 1, BUFFERLEN, stdin)) > 0)
			std::copy(&line[0], &line[count], std::back_inserter(data));
		
		stbi_image_data = stbi_load_from_memory(data.data(), (int)data.size(), &program.texture.width, &program.texture.height, &program.texture.bpp, 4);

		// Open stdin again as the text input from the tty or console
		#ifdef __WIN32
		fp = freopen("CONIN$", "r", stdin);
		console::inHandle = GetStdHandle(STD_INPUT_HANDLE);
		SetConsoleMode(console::inHandle, ENABLE_WINDOW_INPUT);
		#elif defined __linux__
		fp = freopen("/dev/tty", "r", stdin);
		#endif
	} else {
		// Convert wide to char on windows
		#ifndef __linux__
		char farg[BUFFERLEN];
		wcstombs(&farg[0], argv[1], BUFFERLEN);
		#else
		char *farg = argv[1];
		#endif
		
		stbi_image_data = stbi_load(&farg[0], &program.texture.width, &program.texture.height, &program.texture.bpp, 4);
	}

    program.texture.bpp = 4;
    program.texture.load_stbi(stbi_image_data, program.texture.width, program.texture.height, program.texture.bpp);
    stbi_image_free(stbi_image_data);

	if (!program.texture.isAvailable())
		error_exit("Failed to load image");
	
    program.consoleInit();

    int mapping_funcs_index = 0;
    auto mapping_funcs = getMappingFuncs();

    program.converter = mapping_funcs.at(mapping_funcs_index).second;

	if (useArgs)
        program.processArgs(argc, argv);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b")) {
            int y = 1;
            std::vector<double> samples;
            program.buffer.make(program.texture);
            console::cons.~constructor();
            do {
                using hrc = std::chrono::high_resolution_clock;
                using tp = std::chrono::time_point<hrc>;

                tp start, end;

                start = hrc::now();

                //program.runPresampler();
                int width = program.buffer.width;
                int height = program.buffer.height;
                imgcat::cpix_t cpix;
                imgcat::pixel_t pixel;
                for (int x = 0; x < width; x++) {
                    float xf = x / (float) width;
                    for (int y = 0; y < height; y++) {
                        float yf = y / (float) height;
                        //program.buffer.data[y * width + x] =
                        //    program.sampleImage(x / (float) program.buffer.width, y / (float) program.buffer.height);
                        pixel = program.texture.getPixel(xf, yf);
                        getDitherColored(pixel.r, pixel.g, pixel.b, &cpix.character, &cpix.color);
                        program.buffer.data[y * width + x] = cpix;
                    }
                }

                end = hrc::now();

                auto duration = std::chrono::duration<double, std::milli>(end - start);
                double value = duration.count();

                samples.push_back(value);

                double average = 0, max = value, min = value;
                for (auto _v : samples) { 
                    average += _v;
                    if (_v > max)
                        max = _v;
                    if (_v < min)
                        min = _v;
                }
                average /= samples.size();
                int iterations = program.buffer.getLength();

                snprintf(pbuf, LINELEN, "(%li) duration: %f millis, avg: %f, min: %f, max: %f (%f ns for %i iterations), mapper: %s", samples.size(), value, average, min, max, (value / iterations) * 1000000.0f, iterations, mapping_funcs.at(mapping_funcs_index).first.c_str());

                //console::write(0,y++,pbuf);
                puts(pbuf);
            } while (!HASKEY(console::readKey(), VK_ESCAPE));
            return 0;
        }
    }

    if (program.state.preSampleImage)
        program.runPresampler();

	int key = 0;
	while (true) {
        program.keyboard(key);

        program.draw();

        if (HASKEY(key, VK_ESCAPE) || HASKEY(key, 'q'))
            break;

        if (HASKEY(key, 'p')) {
            if (++mapping_funcs_index + 1 > mapping_funcs.size())
                mapping_funcs_index = 0;
            program.converter = mapping_funcs.at(mapping_funcs_index).second;
            if (program.state.preSampleImage)
                program.runPresampler();
            program.draw();
            console::write(0, program.state.showDebug, mapping_funcs.at(mapping_funcs_index).first.c_str());
        }

        key = console::readKey();
    }

    return 0;
}