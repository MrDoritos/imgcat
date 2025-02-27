#include <algorithm>
#include <numeric>
#include <tuple>

#include "imgcat_program.h"

#include "colorMappingCombined.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LINELEN 256
#define BUFFERLEN 65536

int run_benchmark(imgcat::Program &program) {
    auto gs = imgcat::GreedyScheduler();
    
    const auto &mappers = ColorMap::all();
    
    std::unordered_map<std::string,std::vector<double>> samples;

    program.buffer.make(program.texture);
    console::cons.~constructor();

    do {
        for (auto *mapper : mappers) {
            using hrc = std::chrono::high_resolution_clock;
            using tp = std::chrono::time_point<hrc>;

            tp start, end;

            auto func = mapper->get_function();
            auto name = mapper->get_name();
            auto &vec = samples[name];

            start = hrc::now();

            int width = program.buffer.width;
            int height = program.buffer.height;
            imgcat::cpix_t cpix;
            imgcat::pixel_t pixel;
            for (int x = 0; x < width; x++) {
                float xf = x / (float) width;
                for (int y = 0; y < height; y++) {
                    float yf = y / (float) height;
                    pixel = program.texture.getPixel(xf, yf);
                    func(pixel.r, pixel.g, pixel.b, &cpix.character, &cpix.color);
                    program.buffer.data[y * width + x] = cpix;
                }
            }

            end = hrc::now();

            auto duration = std::chrono::duration<double, std::milli>(end - start);
            double value = duration.count();

            vec.push_back(value);

            double average = 0, max = value, min = value;

            max = *std::max_element(vec.begin(), vec.end());
            min = *std::min_element(vec.begin(), vec.end());
            average = std::accumulate(vec.begin(), vec.end(), 0.0) / double(vec.size());
            int iterations = program.buffer.getLength();

            printf("(%li) mapper: %s, duration: %f ms, min: %f, max: %f, avg: %f (%f ns for %i iterations)\n", 
                vec.size(), 
                name.c_str(), 
                value, 
                min, 
                max, 
                average, 
                (value / iterations) * 1000000.0f, 
                iterations);
        }
    } while (!HASKEY(console::readKey(), VK_ESCAPE));

    printf("%li samples of %i iterations for %li programs\n", (*samples.begin()).second.size(), program.buffer.getLength(), samples.size());
    printf("Program\r\t\tMin ms\r\t\t\t\tMax ms\r\t\t\t\t\t\tAvg ms\r\t\t\t\t\t\t\t\tns/iter\n");

    std::vector<std::tuple<double,std::string,std::vector<double>&,double,double,double>> sample_data;

    for (auto &pair : samples) {
        auto &vec = pair.second;
        auto &name = pair.first;

        double max = *std::max_element(vec.begin(), vec.end());
        double min = *std::min_element(vec.begin(), vec.end());
        double average = std::accumulate(vec.begin(), vec.end(), 0.0) / double(vec.size());
        double nanos = (average / double(program.buffer.getLength())) * 1.0e6;

        sample_data.push_back({nanos,name,vec,min,max,average});
    }

    std::sort(sample_data.begin(), sample_data.end());

    for (auto &sample : sample_data)
        printf("%s\r\t\t%f\r\t\t\t\t%f\r\t\t\t\t\t\t%f\r\t\t\t\t\t\t\t\t%f\n", std::get<1>(sample).c_str(), std::get<3>(sample), std::get<4>(sample), std::get<5>(sample), std::get<0>(sample));

    return 0;
}

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
    ColorMap::init();
    
    program.converter = ColorMap::current()->get_function();

	if (useArgs)
        program.processArgs(argc, argv);

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b"))
            return run_benchmark(program);
    }

    if (program.state.preSampleImage)
        program.runPresampler();

	int key = 0;
	while (true) {
        program.keyboard(key);

        program.draw();

        if (HASKEY(key, VK_ESCAPE) || HASKEY(key, 'q'))
            break;

        if (HASKEY(key, 'p'))
            ColorMap::next();
        
        if (HASKEY(key, 'P'))
            ColorMap::prev();

        if (HASKEY(key, 'p') || HASKEY(key, 'P')) {
            program.converter = ColorMap::current()->get_function();

            if (program.state.preSampleImage)
                program.runPresampler();

            program.draw();
            console::write(0, program.state.showDebug, ColorMap::current()->get_name().c_str());
        }

        key = console::readKey();
    }

    return 0;
}