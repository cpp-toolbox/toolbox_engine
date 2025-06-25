#include "toolbox_engine.hpp"

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

GLFWLambdaCallbackManager create_default_glcm_for_input_and_camera(InputState &input_state, FPSCamera &fps_camera,
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
    std::function<void(int, int)> frame_buffer_size_callback = [&](int width, int height) {
        // this gets called whenever the window changes size, because the framebuffer automatically
        // changes size, that is all done in glfw's context, then we need to update opengl's size.
        std::cout << "framebuffersize callback called, width" << width << "height: " << height << std::endl;
        glViewport(0, 0, width, height);
        window.width_px = width;
        window.height_px = height;

        shader_cache.set_uniform(ShaderType::ABSOLUTE_POSITION_WITH_COLORED_VERTEX, ShaderUniformVariable::ASPECT_RATIO,
                                 glm::vec2(height / (float)width, 1));
    };
    GLFWLambdaCallbackManager glcm(window.glfw_window, char_callback, key_callback, mouse_pos_callback,
                                   mouse_button_callback, frame_buffer_size_callback);

    return glcm;
}
