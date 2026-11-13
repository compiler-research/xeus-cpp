/***************************************************************************
 * Copyright (c) 2026, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

// Verifies that the Emscripten build provides a working WebGL2 /
// OffscreenCanvas runtime.
//
// Needs a browser: skipped under node (no DOM, no OffscreenCanvas).

#include <cstdint>
#include <string>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include "doctest/doctest.h"

namespace
{
    constexpr int clear_red = 64;
    constexpr int clear_green = 128;
    constexpr int clear_blue = 191;

    bool in_browser()
    {
        return EM_ASM_INT({
            return (typeof document !== 'undefined'
                    && typeof OffscreenCanvas !== 'undefined') ? 1 : 0;
        }) != 0;
    }

    bool browser_supports_webgl2()
    {
        return EM_ASM_INT({
            try
            {
                return document.createElement('canvas').getContext('webgl2') ? 1 : 0;
            }
            catch (e)
            {
                return 0;
            }
        }) != 0;
    }

    // Whether the browser itself can hand a canvas to a worker. Deliberately
    // independent of the build options: when the browser can and the kernel
    // still fails to create the context, the build is missing the WebGL link
    // options and the test must fail.
    bool browser_can_transfer_canvas()
    {
        return EM_ASM_INT({
            return (typeof OffscreenCanvas !== 'undefined'
                    && typeof document.createElement('canvas').transferControlToOffscreen === 'function')
                ? 1 : 0;
        }) != 0;
    }
}

TEST_SUITE("webgl_runtime")
{
    TEST_CASE("Module.GL.offscreenCanvases is exported")
    {
        if (!in_browser())
        {
            // node has no DOM; the browser runs of this binary do the check.
            return;
        }
        // What xeus-lite uses to hand a transferred OffscreenCanvas to the
        // kernel worker.
        CHECK(EM_ASM_INT({
            return (Module.GL && Module.GL.offscreenCanvases !== undefined) ? 1 : 0;
        }) == 1);
    }

    TEST_CASE("WebGL2 context on an OffscreenCanvas renders")
    {
        if (!in_browser())
        {
            return;
        }
        // Browser test runs must provide WebGL2 and OffscreenCanvas transfer support.
        if (!browser_supports_webgl2())
        {
            FAIL("browser lacks WebGL2 support");
        }
        if (!browser_can_transfer_canvas())
        {
            FAIL("browser lacks OffscreenCanvas transfer support");
        }

        // Emscripten's HTML shells always ship this canvas, but create it when
        // running from a shell that does not.
        EM_ASM({
            if (!document.querySelector('#canvas'))
            {
                var canvas = document.createElement('canvas');
                canvas.id = 'canvas';
                document.body.appendChild(canvas);
            }
        });
        // clang-format on

        EmscriptenWebGLContextAttributes attributes;
        emscripten_webgl_init_context_attributes(&attributes);
        attributes.majorVersion = 2;
        // Only honoured with OFFSCREENCANVAS_SUPPORT: transfers control of the
        // canvas to an OffscreenCanvas and registers it in
        // Module.GL.offscreenCanvases.
        attributes.explicitSwapControl = true;

        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context("#canvas", &attributes);
        REQUIRE(context > 0);

        // The transfer happened and emscripten tracks the OffscreenCanvas.
        CHECK(EM_ASM_INT({
            var entry = Module.GL.offscreenCanvases['canvas'];
            return (entry && entry.canvas instanceof OffscreenCanvas) ? 1 : 0;
        }) == 1);

        REQUIRE(emscripten_webgl_make_context_current(context) == EMSCRIPTEN_RESULT_SUCCESS);

        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        REQUIRE(version != nullptr);
        CHECK(std::string(version).find("WebGL 2.0") != std::string::npos);

        // Needs GL_ENABLE_GET_PROC_ADDRESS, otherwise the linker rejects it.
        CHECK(emscripten_webgl_get_proc_address("glClearColor") != nullptr);

        glClearColor(clear_red / 255.0f, clear_green / 255.0f, clear_blue / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        CHECK(glGetError() == GL_NO_ERROR);

        std::uint8_t pixel[4] = {0, 0, 0, 0};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        CHECK(glGetError() == GL_NO_ERROR);
        CHECK(pixel[0] == clear_red);
        CHECK(pixel[1] == clear_green);
        CHECK(pixel[2] == clear_blue);
        CHECK(pixel[3] == 255);

        CHECK(emscripten_webgl_commit_frame() == EMSCRIPTEN_RESULT_SUCCESS);
        CHECK(emscripten_webgl_destroy_context(context) == EMSCRIPTEN_RESULT_SUCCESS);
    }
}
