#include "toolbox_engine.hpp"
#include <charconv>
#include <system_error>  // for std::errc

namespace tbx_engine {

std::optional<EKey> get_input_key_from_config_if_valid(InputState &input_state, Configuration &configuration,
                                                       const std::string &section_key) {
    auto key_value = configuration.get_value("input", section_key);
    if (key_value.has_value()) {
        auto key_value_str = key_value.value();
        if (input_state.is_valid_key_string(key_value_str)) {
            return input_state.key_str_to_key_enum.at(key_value_str);
        }
    }
    return std::nullopt;
}

EKey get_input_key_from_config_or_default_value(InputState &input_state, Configuration &configuration,
                                                const std::string &section_key) {
    auto opt_val = get_input_key_from_config_if_valid(input_state, configuration, section_key);
    return opt_val.value_or(movement_value_str_to_default_key.at(section_key));
}

void config_x_input_state_x_fps_camera_processing(FPSCamera &fps_camera, InputState &input_state,
                                                  Configuration &configuration, double dt) {
    fps_camera.process_input(
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_slow_move)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_fast_move)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_forward)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_left)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_back)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_right)),
        input_state.is_pressed(get_input_key_from_config_or_default_value(input_state, configuration, config_value_up)),
        input_state.is_pressed(
            get_input_key_from_config_or_default_value(input_state, configuration, config_value_down)),
        dt);
}

void register_input_graphics_sound_config_handlers(Configuration &configuration, FPSCamera &fps_camera,
                                                   FixedFrequencyLoop &ffl) {

    configuration.register_config_handler("input", "mouse_sensitivity", [&](const std::string value) {
        float requested_sens;
        try {
            requested_sens = std::stof(value);
            fps_camera.change_active_sensitivity(requested_sens);
        } catch (const std::exception &) {
            std::cout << "sensivity is invalid" << std::endl;
        }
    });

    configuration.register_config_handler("graphics", "field_of_view", [&](const std::string value) {
        float fov, default_fov = 90;
        try {
            fov = std::stof(value);
            fps_camera.fov = fov;
        } catch (const std::exception &) {
            std::cout << "fov is invalid" << std::endl;
        }
    });

    configuration.register_config_handler("graphics", "max_fps", [&](const std::string value) {
        int max_fps;
        try {
            ffl.rate_limiter_enabled = true;
            max_fps = std::stoi(value);
        } catch (const std::exception &) {
            if (value == "inf") {
                ffl.rate_limiter_enabled = false;
            } else {
                std::cout << "max fps value couldn't be converted to an integer." << std::endl;
            }
        }
        ffl.update_rate_hz = max_fps;
        std::cout << "just set the update rate on the main tick to " << max_fps << std::endl;
    });
}

void potentially_switch_between_menu_and_3d_view(InputState &input_state,
                                                 InputGraphicsSoundMenu &input_graphics_sound_menu,
                                                 FPSCamera &fps_camera, Window &window) {
    if (input_state.is_just_pressed(EKey::ESCAPE)) {
        input_graphics_sound_menu.enabled = !input_graphics_sound_menu.enabled;
        if (input_graphics_sound_menu.enabled) {
            fps_camera.freeze_camera();
            window.enable_cursor();
        } else {
            fps_camera.unfreeze_camera();
            window.disable_cursor();
        }
    }
}

AllGLFWLambdaCallbacks create_default_glcm_for_input_and_camera(InputState &input_state, FPSCamera &fps_camera,
                                                                Window &window, ShaderCache &shader_cache) {
    std::function<void(unsigned int)> char_callback = [](unsigned int codepoint) {};
    std::function<void(int, int, int, int)> key_callback = [&](int key, int scancode, int action, int mods) {
        input_state.glfw_key_callback(key, scancode, action, mods);
    };
    std::function<void(double, double)> mouse_pos_callback = [&](double xpos, double ypos) {
        fps_camera.mouse_callback(xpos, ypos);
        input_state.glfw_cursor_pos_callback(xpos, ypos);
    };
    std::function<void(int, int, int)> mouse_button_callback = [&](int button, int action, int mods) {
        input_state.glfw_mouse_button_callback(button, action, mods);
    };

    std::function<void(double, double)> scroll_callback = [&](double x_offset, double y_offset) {};
    std::function<void(int, int)> frame_buffer_size_callback = [&](int width, int height) {
        // this gets called whenever the window changes size, because the framebuffer automatically
        // changes size, that is all done in glfw's context, then we need to update opengl's size.
        glViewport(0, 0, width, height);
        window.width_px = width;
        window.height_px = height;

        shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
                                 glm::vec2(height / (float)width, 1));
    };

    return {char_callback,         key_callback,    mouse_pos_callback,
            mouse_button_callback, scroll_callback, frame_buffer_size_callback};
}

std::optional<std::pair<int, int>> extract_width_height_from_resolution(const std::string &resolution) {
    std::istringstream iss(resolution);
    int width = 0, height = 0;
    char delimiter = 'x';

    if (!(iss >> width))
        return std::nullopt;

    if (iss.peek() == delimiter)
        iss.ignore();

    if (!(iss >> height))
        return std::nullopt;

    std::pair<int, int> val = {width, height};
    return val;
}

bool parse_on_off_to_bool(const std::string &user_option) {
    if (user_option == "on") {
        return true;
    } else { // user option is false or an invalid string in either case return false
        return false;
    }
};

bool get_user_on_off_value_or_default(Configuration &configuration, const std::string &section_name,
                                      const std::string &key_name) {
    std::optional<std::string> opt_val = configuration.get_value(section_name, key_name);
    bool val = false;
    if (opt_val) {
        val = parse_on_off_to_bool(*opt_val);
    }
    return val;
}


int parse_int_or_default(const std::string &text, int default_value) {
    int result;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (ec == std::errc()) {
        return result;
    } else {
        return default_value;
    }
}

} // namespace tbx_engine
