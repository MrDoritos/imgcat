#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

#include <portaudio.h>

#include "imgcat_program.h"

//#include "colorMappingDitherFast.h"
//#include "colorMappingCacheTest.h"
//#include "colorMappingFast.h"
#include "colorMappingCombined.h"

int wmain(int argc, char** argv) {
    imgcat::Program program;
    bool useArgs = argc > 1;
    bool isPipeInput = !isatty(fileno(stdin));
    FILE* fp;
    char buf[imgcat::LINELEN];
    char *pbuf = &buf[0];

    if (!useArgs && !isPipeInput)
        error_exit("Pipe or filename required");

    cv::VideoCapture vr_state;

    if (isPipeInput) {
        vr_state = cv::VideoCapture("/dev/stdin");
        if (!vr_state.isOpened())
            error_exit("Could not open pipe input");
    } else {
        vr_state = cv::VideoCapture(argv[1]);
        if (!vr_state.isOpened())
            error_exit("Could not open video file");
    }

    program.texture.make(
        vr_state.get(cv::CAP_PROP_FRAME_WIDTH),
        vr_state.get(cv::CAP_PROP_FRAME_HEIGHT)
    );

    if (!program.texture.isAvailable())
        error_exit("Failed to create framebuffer");

    program.consoleInit();

    int conversion_func_index = 0;
    auto conversion_funcs = getMappingFuncs();

    program.converter = conversion_funcs.at(0).second;

    if (useArgs)
        program.processArgs(argc, argv);

    float frame_rate = vr_state.get(cv::CAP_PROP_FPS);
    float frame_duration = 1000.0f / frame_rate;
    float frame_total_count = vr_state.get(cv::CAP_PROP_FRAME_COUNT);
    vr_state.set(cv::CAP_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY);
    float total_time = 0.0f;
    int frame_count = 0;
    int frame_drop = 0;
    int frame_half_drop = 0;
    color_t debugColor = FWHITE|BBLACK;
    adv::setFPS(frame_rate);
    int fit_frame_index = 0;
    std::vector<std::string> fit_frames = {
        "actual",
        "stretch",
        "fit"
    };

    std::string message;

    cv::Mat frame;
    cv::Mat tex_frame(program.texture.height, program.texture.width, 4);

    int key = 0;

    using hrc = std::chrono::system_clock;
    using tp = std::chrono::time_point<hrc>;
    using dur = std::chrono::duration<float, std::micro>;
    using durl = std::chrono::duration<long, std::micro>;

    tp start, now, prev_tp, message_tp;
    std::vector<std::pair<std::string, tp>> debugProfile;
    start = hrc::now();

    auto gs = imgcat::GreedyScheduler();

    //adv::setThreadState(true);
    //adv::setThreadSafety(true);

    while (true) {
        program.keyboard(key);
        if (program.state.showDebug)
            debugProfile.push_back({"keyboard", hrc::now()});

        //adv::waitForReadyFrame();
        //console::sleep(33);
        
        if (HASKEY(key, VK_ESCAPE) || HASKEY(key, 'q'))
            break;

        if (HASKEY(key, ' ')) {
            tp _pause = hrc::now();
            while (!HASKEY(console::readKey(), ' '));
            dur _pausedur = hrc::now() - _pause;
            start += durl((long)_pausedur.count());
            now += durl((long)_pausedur.count());
        }

        float _skip_t = 5000.0f;
        float _skip_framec = _skip_t / frame_duration;
        float _pos_t = frame_count * frame_duration;

        if (HASKEY(key, 'b')) {
            frame_count += _skip_framec;
            /*
            if (frame_count > frame_total_count) {
                int _tmp = frame_total_count - 1;
                int _diff = _tmp - frame_count;
                frame_count = _tmp;
                _skip_t = _diff * frame_duration;
            }*/
            start -= durl((long)(_skip_t * 1000.0));
            vr_state.set(cv::CAP_PROP_POS_MSEC, _pos_t + _skip_t);
        }
        if (HASKEY(key, 'v')) {
            frame_count -= _skip_framec;
            /*
            if (frame_count < 0) {
                int _tmp = 0;
                int _diff = _tmp - frame_count;
                frame_count = _tmp;
                _skip_t = _diff * frame_duration;
            }*/
            start += durl((long)(_skip_t * 1000.0));
            vr_state.set(cv::CAP_PROP_POS_MSEC, _pos_t - _skip_t);
        }
        if (HASKEY(key, 'f')) {
            fit_frame_index++;
            if (fit_frame_index + 1 > fit_frames.size())
                fit_frame_index = 0;
            program.reset();
            message_tp = hrc::now();
            int tw = program.texture.width;
            int th = program.texture.height;
            int w = program.screen.width;
            switch (fit_frame_index) {
                case 1:
                    program.state.newH = program.screen.height;
                    program.state.newW = program.screen.width;
                    break;
                case 2:
                    program.state.newW = program.screen.width;
                    program.state.newH = (th * w) / (float) tw;
                    break;
            }
            message = fit_frames.at(fit_frame_index);
        }
        if (HASKEY(key, 'p')) {
            conversion_func_index++;
            if (conversion_func_index + 1 > conversion_funcs.size())
                conversion_func_index = 0;
            program.converter = conversion_funcs.at(conversion_func_index).second;
            message = conversion_funcs.at(conversion_func_index).first;
            message_tp = hrc::now();
        }

        program.draw();
        //program.consoleUpdateScreen();
        //adv::clear();
        //program.drawImage();
        //program.drawDebug();

        if (program.state.showDebug)
            debugProfile.push_back({"draw", hrc::now()});

        if (!vr_state.grab()) {
            if (program.state.showDebug)
                console::readKey();
            error_exit("Could not load next video frame");
        }
        if (program.state.showDebug)
            debugProfile.push_back({"grab", hrc::now()});

        int64_t pts;

        frame_count++;
        prev_tp = now;
        now = hrc::now();
        dur ft(frame_duration * 1000.0f);
        dur frame_clock = frame_count * ft;
        dur elapsed_duration = now - start;
        dur laglead = frame_clock - elapsed_duration;
        dur total_duration = frame_total_count * ft;
        total_time = frame_count * frame_duration;
        float laglead_framecount = laglead / ft;
        dur delta_time = ft - laglead;

        if (program.state.showDebug) {
            int y = 2;
            std::string sched = !imgcat::GreedyScheduler::skip ? "true" : "false";
            snprintf(pbuf, imgcat::LINELEN, "frame %i/%i drop %i/%i fps %.2f ft %.2fms vt %.2f rt %.2f tt %.2f laglead %.2fms (%.2f frames) width %i height %i sched_fifo %s last_message %s", 
                frame_count, (int)frame_total_count, frame_drop, frame_half_drop, frame_rate, delta_time.count() / 1000.0f, total_time / 1000.0f, 
                elapsed_duration.count() / 1000000.0f, total_duration.count() / 1000000.0f,
                laglead.count() / 1000.0f, laglead_framecount,
                program.texture.width, program.texture.height,
                sched.c_str(), message.c_str());
            console::write(0, y++, pbuf, debugColor);

            tp i_prev = prev_tp;
            tp totaltp = now + durl((long)dur(now - i_prev).count());

            debugProfile.push_back({"total", totaltp});
            for (auto tp : debugProfile) {
                dur diff = tp.second - i_prev;
                i_prev = tp.second;
                snprintf(pbuf, imgcat::LINELEN, "%.2fms %s", diff.count() / 1000.0f, tp.first.c_str());
                console::write(0, y++, pbuf, FBLACK|BWHITE);
            }

            debugProfile.clear();
        } else {
            srand(0);
        }

        if (now - message_tp < durl(2000000)) {
            console::write((program.screen.width / 2) - (message.size() / 2), program.screen.height / 2, message.c_str(), FWHITE|BBLACK);
        }

        if (program.state.showDebug)
            debugProfile.push_back({"debug", hrc::now()});

        //if (frame_total_count < frame_count + 2) {
        if  (total_duration < frame_clock + dur(250000)) {
            //end of stream
            //break;
            vr_state.set(cv::CAP_PROP_POS_FRAMES, 0);
            frame_count = 0;
            start = hrc::now();
        }

        dur drop_duration = dur(-ft * 0.5);
        if (elapsed_duration < frame_clock) {
            debugColor = FBLACK|BWHITE;
            if (elapsed_duration > frame_clock - drop_duration)//debug if leading by drop_duration
                debugColor = FWHITE|BGREEN;
            //reading frames too rapidly, make main thread sleep
            std::this_thread::sleep_for(laglead);
            if (program.state.showDebug)
                debugProfile.push_back({"on time sleep", hrc::now()});
        } else {
            //reading frames too slowly, drop frames intentionally if lagging by drop_duration
            //if (laglead_framecount < -4) { // 4 frames
            if (laglead < drop_duration) { // 100ms
                debugColor = FBLACK|BRED;
                frame_drop++;
                frame_count++;
                if (!vr_state.grab())
                    error_exit("Could not load video frame while dropping frames");
                if (program.state.showDebug)
                    debugProfile.push_back({"mandatory drop", hrc::now()});
            } else {
                debugColor = FWHITE|BBLUE;
                //sleep for a reduced period to catch up (sleep reduces flickering)
                frame_half_drop++;
                //as lag time approaches drop_duration, sleep less
                dur half_lag = ft / (laglead / drop_duration);
                std::this_thread::sleep_for(ft * 0.1f);
                if (program.state.showDebug)
                    debugProfile.push_back({"catch up sleep", hrc::now()});
            }
        }

        if (program.state.showDebug)
            debugProfile.push_back({"dropping", hrc::now()});

        if (!vr_state.retrieve(frame))
            error_exit("Failed to decode frame");

        if (program.state.showDebug)
            debugProfile.push_back({"decode", hrc::now()});

        cv::cvtColor(frame, tex_frame, cv::COLOR_BGR2RGBA);
        program.texture.set_data(tex_frame.data);

        if (program.state.showDebug)
            debugProfile.push_back({"color conversion", hrc::now()});

        key = console::readKeyAsync();
    }    

    return 0;
}