#ifndef TOOLBOX_ENGINE_HPP
#define TOOLBOX_ENGINE_HPP

#include "sbpt_generated_includes.hpp"

void potentially_switch_between_menu_and_3d_view(InputState &input_state,
                                                 InputGraphicsSoundMenu &input_graphics_sound_menu,
                                                 FPSCamera &fps_camera, Window &window);

GLFWLambdaCallbackManager create_default_glcm_for_input_and_camera(InputState &input_state, FPSCamera &fps_camera,
                                                                   Window &window, ShaderCache &shader_cache);

#endif // TOOLBOX_ENGINE_HPP
