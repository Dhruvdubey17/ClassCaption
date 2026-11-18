#include "classcaption/config.h"

#include <doctest.h>

using namespace std;
using namespace classcaption;

TEST_CASE("defaults run the live model on the Neural Engine") {
    const Config config;
    CHECK(config.backend == Backend::coreml);
    CHECK(config.live_model == "models/ggml-large-v3-turbo-q5_0.bin");
    CHECK(config.save_audio);
}

TEST_CASE("config text sets values and skips comments and blank lines") {
    const Config config = parse_config(R"(
        # switch to the GPU
        backend = metal

        live_model = models/ggml-medium.en.bin
        vad_threshold = 0.6
        step_ms = 750
        save_audio = false
    )");
    CHECK(config.backend == Backend::metal);
    CHECK(config.live_model == "models/ggml-medium.en.bin");
    CHECK(config.vad_threshold == doctest::Approx(0.6));
    CHECK(config.step_ms == 750);
    CHECK_FALSE(config.save_audio);
}

TEST_CASE("command line overrides win over the config file") {
    const Config from_file = parse_config("backend = metal\nstep_ms = 750\n");
    const vector<string> args = {"--backend", "cpu", "--step_ms=500"};
    const Config config = apply_args(from_file, args);
    CHECK(config.backend == Backend::cpu);
    CHECK(config.step_ms == 500);
}

TEST_CASE("mistakes fail with a message that names the problem") {
    CHECK_THROWS_WITH_AS(parse_config("bakend = cpu"), "line 1: unknown setting 'bakend'",
                         ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("\nbackend = gpu"),
                         "line 2: backend must be coreml, metal or cpu", ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("step_ms = fast"), "line 1: step_ms must be a whole number",
                         ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("step_ms = 0"), "line 1: step_ms must be above 0",
                         ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("vad_threshold = 1.5"),
                         "line 1: vad_threshold must be between 0 and 1", ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("save_audio = maybe"),
                         "line 1: save_audio must be true or false", ConfigError);
    CHECK_THROWS_WITH_AS(parse_config("backend"), "line 1: expected key = value", ConfigError);
    CHECK_THROWS_WITH_AS(apply_args(Config{}, vector<string>{"--backend"}),
                         "--backend needs a value", ConfigError);
}
