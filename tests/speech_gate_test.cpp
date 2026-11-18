#include "classcaption/speech_gate.h"

#include <doctest.h>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

// Feeds each probability and records the gate's answer after each one.
vector<bool> run(SpeechGate& gate, const vector<float>& probabilities) {
    vector<bool> open;
    open.reserve(probabilities.size());
    for (const float p : probabilities) {
        open.push_back(gate.update(p));
    }
    return open;
}

} // namespace

TEST_CASE("the gate opens on the first frame above the threshold") {
    SpeechGate gate(0.5F, 2);
    CHECK(run(gate, {0.1F, 0.2F, 0.6F}) == vector<bool>{false, false, true});
}

TEST_CASE("short pauses inside speech keep the gate open") {
    SpeechGate gate(0.5F, 2);
    CHECK(run(gate, {0.9F, 0.1F, 0.1F, 0.9F}) == vector<bool>{true, true, true, true});
}

TEST_CASE("the gate closes once silence outlasts the hangover") {
    SpeechGate gate(0.5F, 2);
    CHECK(run(gate, {0.9F, 0.1F, 0.1F, 0.1F, 0.1F}) ==
          vector<bool>{true, true, true, false, false});
}

TEST_CASE("speech that dips just under the threshold still counts as speech") {
    // Once open, the gate uses a lower bar, so a quiet syllable does not start the hangover.
    SpeechGate gate(0.5F, 1);
    CHECK(run(gate, {0.9F, 0.4F, 0.4F, 0.4F}) == vector<bool>{true, true, true, true});
}

TEST_CASE("noise just under the threshold never opens the gate") {
    SpeechGate gate(0.5F, 2);
    CHECK(run(gate, {0.4F, 0.45F, 0.49F}) == vector<bool>{false, false, false});
}
