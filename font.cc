// font.cc - Sergio Gonzalez
//
// This file is released into the public domain.
//
// Tested on:
//  Ubuntu 15.10
//
// Dependencies
//  ubuntu:
//      libsdl2-dev libglew-dev


#include <SDL.h>
#include <GL/glew.h>

#include "sgl.h"
#define assert sgl_assert
#include "gl_helpers.h"
#undef assert

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct GLFontState {
    GLuint program;

    // Draw a quad...
    GLuint vbo;
    GLuint vao;

    u8* test_data;
    GLuint test_texture;
};


struct GLFont {

};

void render(GLFontState* s)
{
    GLint pos_loc     = glGetAttribLocation(s->program, "position");
    GLint sampler_loc = glGetUniformLocation(s->program, "raster_buffer");
    assert (pos_loc     >= 0);
    assert (sampler_loc >= 0);

    GLCHK (glBindVertexArray(s->vao));
    GLCHK (glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
}

void setup_resources(GLFontState* s)
{
    const char* shader_contents[2];
    shader_contents[0] =
            "#version 120\n"
            "attribute vec2 position;\n"
            "\n"
            "varying vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   coord = (position + vec2(1.0,1.0))/2.0;\n"
            "   coord.y = 1.0 - coord.y;"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";

    shader_contents[1] =
            "#version 120\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform float aspect_ratio;\n"
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   vec4 color = texture2D(raster_buffer, coord).bgra; \n"
            "   gl_FragColor = color; \n"
            //"   out_color = color; \n"
            "}\n";

    GLuint shader_objects[2] = {0};
    for ( int i = 0; i < 2; ++i ) {
        GLuint shader_type = (GLuint)((i == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
    }

    s->program = glCreateProgram();

    gl_link_program(s->program, shader_objects, 2);

    //GLCHK (glUseProgramObjectARB(s->program));
    GLCHK (glUseProgram(s->program));


    // ====
    // Create a test texture
    // ====


    GLCHK (glActiveTexture (GL_TEXTURE0) );
    // Create texture
    GLCHK (glGenTextures   (1, &s->test_texture));
    GLCHK (glBindTexture   (GL_TEXTURE_2D, s->test_texture));

    i32 test_w = 512;
    i32 test_h = 512;

    s->test_data = (u8*)malloc( 512 * 512 * 4 );

    FILE* fd = fopen("assets/Oswald-Light.ttf", "rb");

    sgl_assert (fd);


    // TODO: put this into the font struct

    stbtt_bakedchar* baked_chars = sgl_calloc(stbtt_bakedchar, 96);
    u8* ttbuffer                 = sgl_calloc(u8, 1 << 20);
    float font_height            = 100;

    // --

    fread(ttbuffer, 1, 1 << 20, fd);

    // 32 to 128 (96 characters) covers the standard english glyphs
    // http://www.asciitable.com/
    int stb_res = stbtt_BakeFontBitmap(ttbuffer, 0,  // font location (use offset=0 for plain .ttf)
                                       font_height,                     // height of font in pixels
                                       s->test_data, test_w, test_h,  // bitmap to be filled in
                                       /*first_char=*/32, /*num_chars=*/96,          // characters to bake
                                       baked_chars);             // you allocate this, it's num_chars long
    // Test data:
#if 0
    for ( i32 j = 0; j < test_h; ++j ) {
        for ( i32 i = 0; i < test_w; ++i ) {

            i32 idx = j * test_w + i;
            s->test_data[idx*4 + 0] = 128;
            s->test_data[idx*4 + 1] = idx % 255;
            s->test_data[idx*4 + 2] = idx % 255;
            s->test_data[idx*4 + 3] = 255;
        }
    }
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, test_w, test_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->test_data);

    // Note for the future: These are needed.
    GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));



    // ====
    // Create a quad to show our texture
    // ====


#define u -1.0f
        // full
        GLfloat vert_data[] = {
            -u, +u,
            -u, -u,
            +u, -u,
            +u, +u,
        };
#undef u
        GLCHK (glGenVertexArrays(1, &s->vao));
        GLCHK (glBindVertexArray(s->vao));

        GLCHK (glGenBuffersARB(1, &s->vbo));
        assert (s->vbo > 0);
        GLCHK (glBindBufferARB(GL_ARRAY_BUFFER, s->vbo));

        GLint pos_loc     = glGetAttribLocation(s->program, "position");
        GLint sampler_loc = glGetUniformLocation(s->program, "raster_buffer");
        sgl_assert (pos_loc     >= 0);
        sgl_assert (sampler_loc >= 0);

        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
        GLCHK (glVertexAttribPointer(/*attrib location*/pos_loc,
                                     /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                     /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray (pos_loc) );

        GLCHK (glUniform1i(sampler_loc, 0 /*GL_TEXTURE0*/));

}

int main()
{
    i32 width  = 1024;
    i32 height = 768;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window) {
        fprintf(stderr, "[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context) {
        fprintf(stderr, "Could not create OpenGL context\n");
        exit(EXIT_FAILURE);
    }

    GLenum err = glewInit();

    if ( err != GLEW_OK ) {
        assert(!"Could not load GLEW\n");
    }

    GLFontState font_state;

    setup_resources(&font_state);

    bool should_quit = false;

    SDL_Event sdl_event;
    while ( !should_quit ) {
        SDL_PollEvent(&sdl_event);
        switch ( sdl_event.type ) {
        case SDL_QUIT:
            should_quit = true;
            break;
        }


        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        render(&font_state);

        SDL_GL_SwapWindow(window);

        // Uncomment this after setting up a timer.
        // Sleep until the timer ticks.
        SDL_WaitEvent(NULL);
    }

    return EXIT_SUCCESS;
}
