#ifndef TOOLBOX_ENGINE_HPP
#define TOOLBOX_ENGINE_HPP

#include "sbpt_generated_includes.hpp"

namespace tbx_engine {

const std::string config_value_slow_move = "slow_move";
const std::string config_value_fast_move = "fast_move";
const std::string config_value_forward = "forward";
const std::string config_value_left = "left";
const std::string config_value_back = "back";
const std::string config_value_right = "right";
const std::string config_value_up = "up";
const std::string config_value_down = "down";

const std::unordered_map<std::string, EKey> movement_value_str_to_default_key = {
    {config_value_slow_move, EKey::LEFT_CONTROL},
    {config_value_fast_move, EKey::TAB},
    {config_value_forward, EKey::w},
    {config_value_left, EKey::a},
    {config_value_back, EKey::s},
    {config_value_right, EKey::d},
    {config_value_up, EKey::SPACE},
    {config_value_down, EKey::LEFT_SHIFT}};

std::optional<EKey> get_input_key_from_config_if_valid(InputState &input_state, Configuration &configuration,
                                                       const std::string &section_key);

EKey get_input_key_from_config_or_default_value(InputState &input_state, Configuration &configuration,
                                                const std::string &section_key);

void config_x_input_state_x_fps_camera_processing(FPSCamera &fps_camera, InputState &input_state,
                                                  Configuration &configuration, double dt);

void register_input_graphics_sound_config_handlers(Configuration &configuration, FPSCamera &fps_camera,
                                                   FixedFrequencyLoop &ffl);

void potentially_switch_between_menu_and_3d_view(InputState &input_state,
                                                 InputGraphicsSoundMenu &input_graphics_sound_menu,
                                                 FPSCamera &fps_camera, Window &window);

AllGLFWLambdaCallbacks create_default_glcm_for_input_and_camera(InputState &input_state, FPSCamera &fps_camera,
                                                                Window &window, ShaderCache &shader_cache);

#include <string>
#include <sstream>
#include <utility>

std::optional<std::pair<int, int>> extract_width_height_from_resolution(const std::string &resolution);

const std::vector<std::string> on_off_options = {"on", "off"};

// NOTE: I don't think the following have to be lambdas.

bool parse_on_off_to_bool(const std::string &user_option);
bool get_user_on_off_value_or_default(Configuration &configuration, const std::string &section_name,
                                      const std::string &key_name);
int parse_int_or_default(const std::string &text, int default_value);

}; // namespace tbx_engine

class ToolboxEngineCore {
    ConsoleLogger console_logger;
    Configuration configuration;
};

class ToolboxEngine {
  private:
    const std::string default_config_file_path = "assets/config/user_cfg.ini";
    std::string wh;
    const std::pair<int, int> default_resolution = {1280, 720};

  public:
    Configuration configuration;

    std::pair<int, int> requested_resolution;

    ConsoleLogger logger;
    Window window;

    GLFWLambdaCallbackManager glfw_lambda_callback_manager;
    InputState input_state;

    std::vector<ShaderType> requested_shaders;

    ShaderCache shader_cache;
    Batcher batcher;
    FixedFrequencyLoop main_loop;
    InputGraphicsSoundMenu input_graphics_sound_menu;
    FPSCamera fps_camera;

    std::unordered_map<SoundType, std::string> sound_type_to_file;
    SoundSystem sound_system;

    ToolboxEngine(const std::string &program_name, std::vector<ShaderType> requested_shaders,
                  std::unordered_map<SoundType, std::string> sound_type_to_file)
        : configuration(default_config_file_path), requested_shaders(requested_shaders),
          requested_resolution(tbx_engine::extract_width_height_from_resolution(
                                   configuration.get_value("graphics", "resolution").value_or("1280x720"))
                                   .value_or(default_resolution)),
          window(requested_resolution.first, requested_resolution.second, program_name,
                 tbx_engine::get_user_on_off_value_or_default(configuration, "graphics", "fullscreen"), false, false),
          sound_type_to_file(sound_type_to_file), sound_system(100, sound_type_to_file),
          shader_cache(requested_shaders), batcher(shader_cache),
          input_graphics_sound_menu(window, input_state, batcher, sound_system, configuration),
          glfw_lambda_callback_manager(window.glfw_window),
          main_loop(
              tbx_engine::parse_int_or_default(configuration.get_value("graphics", "max_fps").value_or("60"), 60)) {
        auto all_callbacks =
            tbx_engine::create_default_glcm_for_input_and_camera(input_state, fps_camera, window, shader_cache);
        glfw_lambda_callback_manager.set_all_callbacks(all_callbacks);
        glfw_lambda_callback_manager.register_all_callbacks_with_glfw();

        fps_camera.freeze_camera();
        tbx_engine::register_input_graphics_sound_config_handlers(configuration, fps_camera, main_loop);
        configuration.apply_config_logic();
    }
};

#endif // TOOLBOX_ENGINE_HPP
