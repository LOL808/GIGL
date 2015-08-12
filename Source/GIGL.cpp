#include "UI\Window.h"
#include "UI\InputHandler.h"
#include "Common\Constants.h"
#include "Common\Timer.h"
#include "Common\Utility.hpp"
#include "Common\Random.h"
#include "Common\Camera.h"
#include "Common\Renderer.h"
#include "Common\Scene.h"
#include "GL\GLShader.hpp"
#include "GL\GLRTBLockMngr.h"
#include "VPL\PointLight.hpp"
#include "VPL\LightArray.hpp"

using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;

Scene* scene;

//*******************************
//*  World         X = right    *
//*  coordinate    Y = forward  *
//*  system		   Z = up       *
//*******************************

int main(int, char**) {
    // Randomize
    UnitRNG::init();
    // Create a window
    Window window{WINDOW_RES, WINDOW_RES};
    if (!window.isOpen()) return -1;
    // Set up the renderer
    DeferredRenderer engine;
    // Set up the camera
    const PerspectiveCamera cam{{278.0f, 273.0f, -800.0f},  // Position
                                {0.0f, 0.0f, 1.0f},			// Direction
                                {0.0f, 1.0f, 0.0f},			// Up
                                0.22f * PI,					// Horizontal FoV
                                WINDOW_RES, WINDOW_RES,     // Resolution
                                0.1f, 2500.0f};				// Dist. to near and far clipping planes
    // Define transformation matrices
    const mat4 model_mat{1.0f};                             // Model to World space (Identity)
    const mat4 model_view{cam.computeModelView(model_mat)};	// Model to Camera space
    const mat4 MVP{cam.computeMVP(model_view)};			    // Model-view-projection matrix
    const mat3 norm_mat{cam.computeNormXForm(model_mat)};	// Matrix transforming normals
    // Load the scene
    scene = new Scene;
    scene->loadObjects("Assets\\cornell_box.obj", engine.getShaderProgram().id());
    // Set up fog
    scene->addFog("Assets\\df.3dt", "Assets\\pi_df.3dt", MAJ_EXT_K, ABS_K, SCA_K, cam);
    const BBox& fog_box{scene->getFogBounds()};
    const vec3 fog_pt_min{fog_box.minPt()};
    const vec3 fog_pt_max{fog_box.maxPt()};
    const vec3 box_top_mid{fog_pt_max - 0.5f * vec3{fog_pt_max.x - fog_pt_min.x, 0.0f,
                                                    fog_pt_max.z - fog_pt_min.z}};
    // Set up lights
    LightArray<PPL> ppls{1};
    LightArray<VPL> vpls{MAX_N_VPLS};
    // Set static uniforms
    engine.getShaderProgram().setUniformValue("cam_w_pos",       cam.worldPos());
    engine.getShaderProgram().setUniformValue("vol_dens",        TEX_U_DENS_V);
    engine.getShaderProgram().setUniformValue("pi_dens",         TEX_U_PI_DENS);
    engine.getShaderProgram().setUniformValue("ppl_shadow_cube", TEX_U_PPL_SM);
    engine.getShaderProgram().setUniformValue("vpl_shadow_cube", TEX_U_VPL_SM);
    engine.getShaderProgram().setUniformValue("accum_buffer",    TEX_U_ACCUM);
    engine.getShaderProgram().setUniformValue("halton_seq",      TEX_U_HALTON);
    engine.getShaderProgram().setUniformValue("inv_max_dist_sq", invSq(MAX_DIST));
    engine.getShaderProgram().setUniformValue("fog_bounds[0]",   fog_pt_min);
    engine.getShaderProgram().setUniformValue("fog_bounds[1]",   fog_pt_max);
    engine.getShaderProgram().setUniformValue("inv_fog_dims",    1.0f / (fog_pt_max - fog_pt_min));
    engine.getShaderProgram().setUniformValue("model_mat",       model_mat);
    engine.getShaderProgram().setUniformValue("MVP",             MVP);
    engine.getShaderProgram().setUniformValue("norm_mat",        norm_mat);
    ppls.bind(UB_PPL_ARR);
    vpls.bind(UB_VPL_ARR);
    // Init dynamic uniforms
    InputHandler::init(&engine.settings);
    // Create a ring-triple-buffer lock manager
    GLRTBLockMngr rtb_lock_mngr;
    int tri_buf_idx = 0;
    /* Rendering loop */
    while (!window.shouldClose()) {
        // Start frame timing
        const uint t0{HighResTimer::time_ms()};
        // Process input
        InputHandler::updateParams(window.get());
        // Wait for buffer write access
        rtb_lock_mngr.waitForLockExpiration();
        // Update the lights
        engine.updateLights(*scene, box_top_mid, ppls, vpls);
        // Generate shadow maps
        const uint t1{HighResTimer::time_ms()};
        engine.generateShadowMaps(*scene, model_mat, ppls, vpls);
        // RENDER
        const uint t2{HighResTimer::time_ms()};
        window.clear();
        engine.shade(*scene, tri_buf_idx);
        // Switch to the next buffer
        rtb_lock_mngr.lockBuffer();
        ppls.switchToNextBuffer();
        vpls.switchToNextBuffer();
        tri_buf_idx = (tri_buf_idx + 1) % 3;
        // Prepare to draw the next frame
        window.refresh();
        engine.settings.frame_num++;
        // Display frame time
        const uint t3{HighResTimer::time_ms()};
        char title[TITLE_LEN];
        if (engine.settings.frame_num <= 30) {
            sprintf_s(title, TITLE_LEN, "GLGI (Render: %u ms | SM: %u ms | Misc: %u ms)",
                      t3 - t2, t2 - t1, t1 - t0);
        } else {
            sprintf_s(title, TITLE_LEN, "GLGI (done)");
        }
        window.setTitle(title);
    }
    return 0;
}
