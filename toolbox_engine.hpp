#ifndef TOOLBOX_ENGINE_HPP
#define TOOLBOX_ENGINE_HPP

#include "sbpt_generated_includes.hpp"

#include <GLFW/glfw3.h>
#include <string>
#include <sstream>
#include <utility>

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

void register_input_graphics_sound_config_handlers(Configuration &configuration, FPSCamera &fps_camera,
                                                   FixedFrequencyLoop &ffl);

void potentially_switch_between_menu_and_3d_view(InputState &input_state,
                                                 InputGraphicsSoundMenu &input_graphics_sound_menu,
                                                 FPSCamera &fps_camera, Window &window);

AllGLFWLambdaCallbacks create_default_glcm_for_input_and_camera(GLFWInputAdapter &glfw_input_adapter,
                                                                FPSCamera &fps_camera, Window &window,
                                                                ShaderCache &shader_cache);

std::optional<std::pair<int, int>> extract_width_height_from_resolution(const std::string &resolution);

const std::vector<std::string> on_off_options = {"on", "off"};

// NOTE: I don't think the following have to be lambdas.

bool parse_on_off_to_bool(const std::string &user_option);
bool get_user_on_off_value_or_default(Configuration &configuration, const std::string &section_name,
                                      const std::string &key_name);
int parse_int_or_default(const std::string &text, int default_value);

}; // namespace tbx_engine

// class ToolboxEngineCore {
//     Logger console_logger;
//     Configuration configuration;
// };

/**
 * @pre you have to have generated the batcher for the absolute_position_with_colored_vertex shader, this is probably
 * the simplest shader that allows you to express objects with color so I don't find this to be a huge dependency
 *
 * This is a 3d interactive experience that has a 3d component and a 2d menu component
 *
 */
class ToolboxEngine {
  private:
    const std::string default_config_file_path = "assets/config/user_cfg.ini";
    std::string wh;
    const std::pair<int, int> default_resolution = {1280, 720};

  public:
    Configuration configuration;

    bool window_should_close() { return glfwWindowShouldClose(window.glfw_window); }

    /// a wrapper around the main loop start function so we can inject engine specfic logic around what the user wants
    /// to do.
    void start(const std::function<void(double)> &rate_limited_func,
               const std::function<bool()> &termination_condition_func,
               std::optional<std::function<void(IterationStats)>> loop_stats_function = std::nullopt) {
        main_loop.wait_strategy = FixedFrequencyLoop::WaitStrategy::busy_wait;
        main_loop.start(

            [&](double dt) {
                window.start_of_tick_glfw_logic();
                rate_limited_func(dt);
                window.end_of_tick_glfw_logic();
                input_state.process();
            },

            termination_condition_func, loop_stats_function);
    };

    std::pair<int, int> requested_resolution;

    Logger logger{"toolbox_engine"};
    Window window;

    GLFWLambdaCallbackManager glfw_lambda_callback_manager;
    InputState input_state;
    GLFWInputAdapter glfw_input_adapter{input_state};

    std::vector<ShaderType> requested_shaders;

    ShaderCache shader_cache;
    Batcher batcher;

    FixedFrequencyLoop main_loop;

  public:
    InputGraphicsSoundMenu input_graphics_sound_menu;
    bool &igs_menu_active;
    // NOTE: this starts frozen so you have to unfreeze it to look around
    FPSCamera fps_camera;

    std::unordered_map<SoundType, std::string> sound_type_to_file;
    SoundSystem sound_system;

    UIRenderSuiteImpl ui_render_suite;

    // startfold support for pausing updates from callbacks

    ToolboxEngine(const std::string &program_name, std::vector<ShaderType> requested_shaders,
                  std::unordered_map<SoundType, std::string> sound_type_to_file)
        : configuration(default_config_file_path), requested_shaders(requested_shaders),
          requested_resolution(tbx_engine::extract_width_height_from_resolution(
                                   configuration.get_value("graphics", "resolution").value_or("1280x720"))
                                   .value_or(default_resolution)),
          window(requested_resolution.first, requested_resolution.second, program_name,
                 tbx_engine::get_user_on_off_value_or_default(configuration, "graphics", "fullscreen"), false, false),
          fps_camera(window.width_px, window.height_px), sound_type_to_file(sound_type_to_file),
          sound_system(100, sound_type_to_file), shader_cache(requested_shaders), batcher(shader_cache),
          input_graphics_sound_menu(window, input_state, batcher, sound_system, configuration),
          igs_menu_active(input_graphics_sound_menu.enabled), glfw_lambda_callback_manager(window.glfw_window),
          main_loop(
              tbx_engine::parse_int_or_default(configuration.get_value("graphics", "max_fps").value_or("60"), 60)),
          ui_render_suite(batcher) {
        auto all_callbacks =
            tbx_engine::create_default_glcm_for_input_and_camera(glfw_input_adapter, fps_camera, window, shader_cache);
        glfw_lambda_callback_manager.set_all_callbacks(all_callbacks);
        glfw_lambda_callback_manager.register_all_callbacks_with_glfw();

        fps_camera.freeze_camera();
        tbx_engine::register_input_graphics_sound_config_handlers(configuration, fps_camera, main_loop);
        // NOTE: this is required to render the menu
        shader_cache.register_shader_program(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX);
        configuration.apply_config_logic();
        fps_camera.set_cursor_position = [&](double xpos, double ypos) { window.set_cursor_pos(xpos, ypos); };
    }

    vertex_geometry::Rectangle get_fullscreen_rect() {
        auto [carsx, carsy] = window.get_corrective_aspect_ratio_scale();
        vertex_geometry::Rectangle full_screen_rect(glm_utils::zero_R3, 2 * carsx, 2 * carsy);
        return full_screen_rect;
    }

    enum class ActiveMouseMode {
        CameraControl,  // Mouse moves the camera
        MenuInteraction // Mouse interacts with UI menus
    };

    ActiveMouseMode active_mouse_mode = ToolboxEngine::ActiveMouseMode::MenuInteraction;

    // NOTE: must be called every frame so that it updates instantly
    void update_active_mouse_mode(bool any_mouse_interactable_window_open) {
        bool all_mouse_interactable_menus_closed = not any_mouse_interactable_window_open;
        if (all_mouse_interactable_menus_closed) {
            if (active_mouse_mode == ActiveMouseMode::MenuInteraction) {
                fps_camera.unfreeze_camera();
                window.disable_cursor();
                active_mouse_mode = ActiveMouseMode::CameraControl;
            }
        } else {
            if (active_mouse_mode == ActiveMouseMode::CameraControl) {
                fps_camera.freeze_camera();
                window.enable_cursor();
                active_mouse_mode = ActiveMouseMode::MenuInteraction;
            }
        }
    };

    /**
     * @brief must be called to render the menu
     *
     */
    void process_and_queue_render_input_graphics_sound_menu() {
        GlobalLogSection _("process_and_queue_render_input_graphics_sound_menu");

        if (igs_menu_active) {
            global_logger->info("igs menu active about to draw it");
            input_graphics_sound_menu.process_and_queue_render_menu(window, input_state, ui_render_suite);
        }

        // NOTE: escape only opens the menu if no other menu is open ie camera control is on
        if (input_state.is_just_pressed(EKey::ESCAPE) and active_mouse_mode == ActiveMouseMode::CameraControl) {
            igs_menu_active = true;
        }
        if (input_state.is_just_pressed(EKey::ESCAPE) and active_mouse_mode == ActiveMouseMode::MenuInteraction) {
            igs_menu_active = false;
        }
    }

    // NOTE: this had to be named this to avoid a collidion with process_and_queue_rener_ui because UI doesn't use a
    // namespace, and it should so fix that later
    void process_and_queue_render_specific_ui(UI &ui) {

        glm::vec2 acnmp = glm_utils::tuple_to_vec2(
            window.convert_point_from_2d_screen_space_to_2d_aspect_corrected_normalized_screen_space(
                input_state.mouse_position_x, input_state.mouse_position_y));

        process_and_queue_render_ui(acnmp, ui, ui_render_suite, input_state.get_keys_just_pressed_this_tick(),
                                    input_state.is_just_pressed(EKey::BACKSPACE),
                                    input_state.is_just_pressed(EKey::ENTER),
                                    input_state.is_just_pressed(EKey::LEFT_MOUSE_BUTTON));
    }

    /**
     *  @brief draws the stats about the engine that the user has requested to see
     */
    void draw_chosen_engine_stats() {
        if (configuration.is_on("graphics", "show_fps")) {
            draw_fps();
        }
        if (configuration.is_on("graphics", "show_pos")) {
            draw_pos();
        }
        if (configuration.is_on("graphics", "show_main_loop_iteration_count")) {
            draw_iteration_count();
        }
    }

    draw_info::IVPColor fps_ivpc;
    draw_info::IVPColor iteration_count_ivpc;
    draw_info::IVPColor pos_ivpc;

    /**
     * computes the visible volume of an absolute position shader, these all account for aspect ratio, and thus it
     * is used here
     *
     * also we can use an aabb because the abs position shader doesn't use any perspective so its not a frustum or
     * something like that
     */
    vertex_geometry::AxisAlignedBoundingBox get_visible_aabb_of_absolute_position_shader() {
        auto [x_scale, y_scale] = window.get_corrective_aspect_ratio_scale();
        glm::vec3 scale_vec{x_scale, y_scale, 1};
        glm::vec3 min_corner = glm_utils::minus_one_R3 * scale_vec;
        glm::vec3 max_corner = glm_utils::one_R3 * scale_vec;
        return vertex_geometry::AxisAlignedBoundingBox({min_corner, max_corner});
    }

    void draw_fps() {
        GlobalLogSection _("draw_fps");
        int average_fps = main_loop.average_fps.get();
        auto top_right = get_visible_aabb_of_absolute_position_shader().get_max_xy_position();
        auto side_length = 0.2;
        fps_ivpc.logging_enabled = true;

        // NOTE: here the copy assignment function is used, thus ids are not clobbered, but object becomes dirty,
        // which is what we want.
        fps_ivpc.copy_draw_data_from(draw_info::IVPColor(
            grid_font::get_text_geometry(std::to_string(average_fps), vertex_geometry::create_rectangle_from_top_right(
                                                                          top_right, side_length, side_length)),
            colors::grey));

        batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(fps_ivpc);
    }

    void draw_iteration_count() {
        GlobalLogSection _("draw_iteration_count");
        auto loop_stats = main_loop.get_average_loop_stats();
        auto top_right = get_visible_aabb_of_absolute_position_shader().get_max_xy_position();
        auto side_length = 0.2;
        iteration_count_ivpc.logging_enabled = true;

        // NOTE: here the copy assignment function is used, thus ids are not clobbered, but object becomes dirty,
        // which is what we want.
        iteration_count_ivpc.copy_draw_data_from(draw_info::IVPColor(
            grid_font::get_text_geometry(
                std::to_string(main_loop.iteration_count),
                vertex_geometry::slide_rectangle(
                    vertex_geometry::create_rectangle_from_top_right(top_right, side_length, side_length), 0, -1)),
            colors::grey));

        batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(iteration_count_ivpc);
    }

    void draw_pos() {
        GlobalLogSection _("draw_pos");

        auto pos = fps_camera.transform.get_translation();
        auto pos_str = vec3_to_string(pos, 2);

        auto top_right = get_visible_aabb_of_absolute_position_shader().get_max_xy_position();
        auto side_length = 0.2;
        pos_ivpc.logging_enabled = true;

        pos_ivpc.copy_draw_data_from(draw_info::IVPColor(
            grid_font::get_text_geometry(
                pos_str,
                vertex_geometry::slide_rectangle(
                    vertex_geometry::create_rectangle_from_top_right(top_right, side_length, side_length), 0, -2)),
            colors::grey));

        batcher.absolute_position_with_colored_vertex_shader_batcher.queue_draw(pos_ivpc);
    }

    movement::GodModeInput get_god_mode_movement_input() {
        return {input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_slow_move)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_fast_move)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_forward)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_left)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_back)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_right)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_up)),
                input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                    input_state, configuration, tbx_engine::config_value_down))};
    }

    void update_camera_position_with_default_movement(double dt) {

        fps_camera.update_position_based_on_keys_pressed(
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_slow_move)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_fast_move)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_forward)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_left)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_back)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_right)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(input_state, configuration,
                                                                                          tbx_engine::config_value_up)),
            input_state.is_pressed(tbx_engine::get_input_key_from_config_or_default_value(
                input_state, configuration, tbx_engine::config_value_down)),
            dt);
    }

    /**
     * @brief Enables standard alpha blending in OpenGL.
     *
     * This function enables OpenGL's blending mode and configures it to use
     * standard alpha transparency. When enabled, fragment colors are combined
     * with existing framebuffer colors based on their alpha values, allowing
     * for proper rendering of transparent textures and materials.
     *
     * The blending equation used is:
     *     final_color = src_color * src_alpha + dst_color * (1 - src_alpha)
     *
     * This is the most common setup for rendering textures with transparency,
     * such as UI elements, sprites, or decals.
     *
     * Example:
     * @code
     * enable_blending();
     * draw_transparent_object();
     * @endcode
     */
    void enable_blending() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
};

#endif // TOOLBOX_ENGINE_HPP
