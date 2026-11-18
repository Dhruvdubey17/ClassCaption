#include "caption_window.h"

#include "classcaption/ppm.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace std;

namespace classcaption {
namespace {

// Verdana was drawn for reading on screens. ImGui's built-in font is a pixel font that blurs
// when enlarged, so it is only the fallback.
constexpr const char* kCaptionFont = "/System/Library/Fonts/Supplemental/Verdana.ttf";
constexpr float kDefaultFontSize = 32.0F;
constexpr float kMinFontSize = 16.0F;
constexpr float kMaxFontSize = 96.0F;
constexpr float kFontStep = 4.0F;
constexpr float kControlsFontSize = 16.0F;
// Redraw at most 30 times a second: captions change about once a second, and a laptop on battery
// through a lecture should not spend power on frames nobody needs.
constexpr double kFrameWait = 1.0 / 30.0;
constexpr auto kFrameSaveInterval = chrono::milliseconds(200);

struct Theme {
    ImVec4 background;
    ImVec4 text;
    ImVec4 draft; // still at least 4.5:1 contrast against the background (WCAG AA)
};
constexpr Theme kDark{.background = {0.0F, 0.0F, 0.0F, 1.0F},
                      .text = {1.0F, 1.0F, 1.0F, 1.0F},
                      .draft = {0.62F, 0.62F, 0.62F, 1.0F}};
constexpr Theme kLight{.background = {1.0F, 1.0F, 1.0F, 1.0F},
                       .text = {0.0F, 0.0F, 0.0F, 1.0F},
                       .draft = {0.35F, 0.35F, 0.35F, 1.0F}};

struct ViewSettings {
    float font_size = kDefaultFontSize;
    bool dark = true;
    bool always_on_top = false;
};

struct GlfwSession {
    GlfwSession() {
        if (glfwInit() == GLFW_FALSE) {
            throw runtime_error("could not start the window system");
        }
    }
    ~GlfwSession() { glfwTerminate(); }
    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
    GlfwSession(GlfwSession&&) = delete;
    GlfwSession& operator=(GlfwSession&&) = delete;
};

struct WindowDeleter {
    void operator()(GLFWwindow* window) const { glfwDestroyWindow(window); }
};

unique_ptr<GLFWwindow, WindowDeleter> open_window() {
    // macOS offers OpenGL 3.2 core profile, forward compatible, and nothing older for shaders.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    unique_ptr<GLFWwindow, WindowDeleter> window(
        glfwCreateWindow(1000, 320, "ClassCaption", nullptr, nullptr));
    if (!window) {
        throw runtime_error("could not open the caption window");
    }
    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(1);
    return window;
}

ImFont* load_caption_font() {
    ImGuiIO& io = ImGui::GetIO();
    if (filesystem::exists(kCaptionFont)) {
        return io.Fonts->AddFontFromFileTTF(kCaptionFont);
    }
    return io.Fonts->AddFontDefault();
}

// Cmd plus a key.
ImGuiKeyChord cmd(ImGuiKey key) {
    return ImGuiMod_Super | key;
}

void tooltip(const char* text) {
    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }
}

void wrapped_text(const string& text) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    ImGui::PopTextWrapPos();
}

void change_font_size(ViewSettings& view, float delta) {
    view.font_size = clamp(view.font_size + delta, kMinFontSize, kMaxFontSize);
}

void draw_controls(GLFWwindow* window, ViewSettings& view, bool finished) {
    const Theme& theme = view.dark ? kDark : kLight;
    ImGui::PushFont(nullptr, kControlsFontSize);
    // ImGui's own colors assume a dark background, so draw the controls in the theme's colors,
    // with a visible border on the checkbox in both themes.
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, theme.text);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme.background);
    ImGui::PushStyleColor(ImGuiCol_Border, theme.draft);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F);
    if (ImGui::Button("A-")) {
        change_font_size(view, -kFontStep);
    }
    tooltip("Smaller text (Cmd -)");
    ImGui::SameLine();
    if (ImGui::Button("A+")) {
        change_font_size(view, kFontStep);
    }
    tooltip("Larger text (Cmd +)");
    ImGui::SameLine();
    if (ImGui::Button(view.dark ? "Light background" : "Dark background")) {
        view.dark = !view.dark;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Always on top", &view.always_on_top)) {
        glfwSetWindowAttrib(window, GLFW_FLOATING, view.always_on_top ? GLFW_TRUE : GLFW_FALSE);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(finished ? "Finished" : "Listening");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
}

void handle_shortcuts(ViewSettings& view) {
    if (ImGui::IsKeyChordPressed(cmd(ImGuiKey_Equal))) {
        change_font_size(view, kFontStep);
    }
    if (ImGui::IsKeyChordPressed(cmd(ImGuiKey_Minus))) {
        change_font_size(view, -kFontStep);
    }
}

void draw_captions(const CaptionLog::Snapshot& captions, ImFont* font, const ViewSettings& view) {
    const Theme& theme = view.dark ? kDark : kLight;
    ImGui::BeginChild("captions");
    // Decide before drawing, from last frame's position: new text grows scroll_max this frame.
    const bool following = keep_following(ImGui::GetScrollY(), ImGui::GetScrollMaxY());

    ImGui::PushFont(font, view.font_size);
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text);
    for (const string& line : captions.lines) {
        wrapped_text(line);
    }
    ImGui::PopStyleColor();
    if (!captions.draft.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.draft);
        wrapped_text(captions.draft);
        ImGui::PopStyleColor();
    }
    ImGui::PopFont();

    if (following) {
        ImGui::SetScrollHereY(1.0F);
    }
    ImGui::EndChild();
}

void draw_frame(GLFWwindow* window, const CaptionLog& log, ImFont* font, ViewSettings& view,
                bool finished) {
    const Theme& theme = view.dark ? kDark : kLight;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    handle_shortcuts(view);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme.background);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.background);
    ImGui::Begin("ClassCaption", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    draw_controls(window, view, finished);
    draw_captions(log.snapshot(), font, view);
    ImGui::End();
    ImGui::PopStyleColor(2);

    ImGui::Render();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(theme.background.x, theme.background.y, theme.background.z, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Reads the frame just drawn, before it is swapped to the screen. Reading back our own frame needs
// no screen recording permission, unlike a screenshot of the display.
void save_frame(GLFWwindow* window, const filesystem::path& path) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    ofstream(path, ios::binary) << ppm_image(width, height, rgba);
}

// Saves a numbered frame every kFrameSaveInterval while captions run, then a last one when the
// audio has ended.
class FrameRecorder {
  public:
    explicit FrameRecorder(filesystem::path dir) : dir_(std::move(dir)) {
        if (!dir_.empty()) {
            filesystem::create_directories(dir_);
        }
    }

    [[nodiscard]] bool enabled() const { return !dir_.empty(); }

    void maybe_save(GLFWwindow* window, bool force) {
        const auto now = chrono::steady_clock::now();
        if (!enabled() || (!force && now < next_save_)) {
            return;
        }
        save_frame(window, dir_ / format("frame-{:05}.ppm", ++count_));
        next_save_ = now + kFrameSaveInterval;
    }

  private:
    filesystem::path dir_;
    int count_ = 0;
    chrono::steady_clock::time_point next_save_;
};

} // namespace

void show_caption_window(const CaptionLog& log, const atomic<bool>& stop,
                         const atomic<bool>& finished, const filesystem::path& frames_dir) {
    const GlfwSession glfw;
    const auto window = open_window();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImFont* const font = load_caption_font();
    ViewSettings view;
    FrameRecorder recorder(frames_dir);
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE && !stop) {
        glfwWaitEventsTimeout(kFrameWait);
        // Read before drawing, so the last saved frame already shows every caption.
        const bool audio_done = finished;
        draw_frame(window.get(), log, font, view, audio_done);
        recorder.maybe_save(window.get(), audio_done);
        glfwSwapBuffers(window.get());
        if (audio_done && recorder.enabled()) {
            break;
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

} // namespace classcaption
