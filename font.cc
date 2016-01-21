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
    GLuint quad_vbo;
    GLuint coord_vbo;
    GLuint vao;

    int screen_width;
    int screen_height;

    float text_cursor[2];
};


struct GLFont {
    stbtt_bakedchar*    baked_chars;

    GLuint              atlas_texture;
    int                 atlas_w, atlas_h;
};

// x and y are in screen coordinates.
void render(GLFontState* s, GLFont* f, char* text, float color[3], float x, float y)
{
    glUseProgram(s->program);
    GLint pos_loc     = glGetAttribLocation(s->program, "position");
    GLint sampler_loc = glGetUniformLocation(s->program, "raster_buffer");
    GLint color_loc   = glGetUniformLocation(s->program, "txt_color");
    assert (pos_loc     >= 0);
    assert (sampler_loc >= 0);
    assert (color_loc >= 0);

    GLCHK (glUniform3fv(color_loc, 1, color ));
    s->text_cursor[0] = x;
    s->text_cursor[1] = y;

    stbtt_aligned_quad q;

    GLCHK (glBindVertexArray(s->vao));
    for (char* c = text; *c != '\0'; ++c) {
#if 1
        GLCHK (glActiveTexture (GL_TEXTURE0) );
        glBindTexture(GL_TEXTURE_2D, f->atlas_texture);
        stbtt_GetBakedQuad(f->baked_chars, f->atlas_w, f->atlas_h,  // same data as above
                           *c-32,             // character to display
                           &(s->text_cursor[0]), &s->text_cursor[1],  // pointers to current position in screen pixel space
                           &q,      // output: quad to draw
                           /*opengl_fillrule=*/true);       // true if opengl fill rule; false if DX9 or earlier

        // update buffer objects
        GLfloat vert_data[] = {
            q.x0, q.y1,
            q.x0, q.y0,
            q.x1, q.y0,
            q.x1, q.y1,
        };
        GLfloat coord_data[] = {
            q.s0, q.t1,
            q.s0, q.t0,
            q.s1, q.t0,
            q.s1, q.t1,
        };
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, s->quad_vbo));
        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(vert_data), vert_data, GL_STATIC_DRAW));
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, s->coord_vbo));
        GLCHK (glBufferData (GL_ARRAY_BUFFER, sizeof(coord_data), coord_data, GL_STATIC_DRAW));
        GLCHK (glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
#endif
    }
}

void setup_resources(GLFontState* s, int width, int height)
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    const char* shader_contents[2];
    shader_contents[0] =
            "#version 120\n"
            "attribute vec2 position;\n"
            "attribute vec2 _coord;\n"
            "uniform vec2 screen_size;\n"
            "\n"
            "varying vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   vec2 transformed = vec2( 2*position.x/screen_size.x - 1,  -2*position.y/screen_size.y + 1);\n"
            "   coord = _coord;  // Interpolate tex coordinate. \n"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(transformed, 0.0, 1.0);\n"
            "}\n";

    shader_contents[1] =
            "#version 120\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform float aspect_ratio;\n"
            "uniform vec3 txt_color;\n"
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            "   float a = texture2D(raster_buffer, coord).a; \n"
            "    \n"
            "    \n"
            "        vec4 color = vec4(txt_color, a); \n "
            "        gl_FragColor = color; \n"
            "    \n"
            //"   out_color = color; \n"
            "}\n";

    GLuint shader_objects[2] = {0};
    for ( int i = 0; i < 2; ++i ) {
        GLuint shader_type = (GLuint)((i == 0) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
        shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
    }

    s->program = glCreateProgram();

    gl_link_program(s->program, shader_objects, 2);

    GLCHK (glUseProgram(s->program));

    glViewport(0, 0, width, height);
    s->screen_width = width;
    s->screen_height = height;

    GLint size_loc = glGetUniformLocation(s->program, "screen_size");
    assert (size_loc >= 0);
    GLfloat sizegl[2] = { s->screen_width, s->screen_height };
    glUniform2fv ( size_loc, 1, sizegl);

    return;
}

void on_resize(GLFontState* s, int width, int height)
{
    glUseProgram(s->program);

    glViewport(0, 0, width, height);
    s->screen_width = width;
    s->screen_height = height;

    GLint size_loc = glGetUniformLocation(s->program, "screen_size");
    assert (size_loc >= 0);
    GLfloat sizegl[2] = { s->screen_width, s->screen_height };
    glUniform2fv ( size_loc, 1, sizegl);
}

bool bake_font(GLFontState* s, GLFont* f,
               int atlas_w, int atlas_h,
               float font_height)
{
    bool result = false;
    // ====
    // Create a test texture
    // ====


    GLCHK (glActiveTexture (GL_TEXTURE0) );
    // Create texture
    GLCHK (glGenTextures   (1, &f->atlas_texture));
    GLCHK (glBindTexture   (GL_TEXTURE_2D, f->atlas_texture));

    u8* atlas_data;
    atlas_data = (u8*)malloc( 512 * 512 );

    FILE* fd = fopen("assets/Oswald-Light.ttf", "rb");

    if ( fd ) {
        f->atlas_w = atlas_w;
        f->atlas_h = atlas_h;
        i32 num_chars = 96;  // Number of chars we will be using (ASCII subset)

        f->baked_chars = sgl_calloc(stbtt_bakedchar, num_chars);
        size_t sz_ttbuffer           = 1 << 21; // From wikipedia, 10ffff is the maximum unicode codepoint. This is an upper bound for a single-font ttf file.
        u8* ttbuffer                 = sgl_calloc(u8, sz_ttbuffer);

        size_t count = fread(ttbuffer, 1, sz_ttbuffer, fd);

        if ( count != sz_ttbuffer && !feof(fd) ) {
            sgl_assert ( !"Read without reaching expected EOF!" );
        }

        printf(" Read %lu characters from font file. \n", count);

        // 32 to 128 (96 characters) covers the standard english glyphs
        // http://www.asciitable.com/
        int stb_res = stbtt_BakeFontBitmap(ttbuffer, 0,  // font location (use offset=0 for plain .ttf)
                                           font_height,                     // height of font in pixels
                                           atlas_data, atlas_w, atlas_h,  // bitmap to be filled in
                                           /*first_char=*/32, /*num_chars=*/num_chars,  // characters to bake
                                           f->baked_chars);             // you allocate this, it's num_chars long
        sgl_free(ttbuffer);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas_w, atlas_h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas_data);
        glFinish();
        sgl_free(atlas_data);

        // Note for the future: These are needed.
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));


        GLCHK (glGenVertexArrays(1, &s->vao));
        GLCHK (glBindVertexArray(s->vao));

        GLint pos_loc     = glGetAttribLocation(s->program, "position");
        GLint coord_loc   = glGetAttribLocation(s->program, "_coord");
        GLint sampler_loc = glGetUniformLocation(s->program, "raster_buffer");

        sgl_assert (pos_loc >= 0);
        sgl_assert (coord_loc >= 0);
        sgl_assert (sampler_loc >= 0);

        GLCHK (glGenBuffers(1, &s->quad_vbo));
        assert (s->quad_vbo > 0);
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, s->quad_vbo));

        GLCHK (glVertexAttribPointer(/*attrib location*/pos_loc,
                                     /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                     /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray (pos_loc) );


        GLCHK (glGenBuffers(1, &s->coord_vbo));
        assert (s->coord_vbo > 0);
        GLCHK (glBindBuffer (GL_ARRAY_BUFFER, s->coord_vbo));

        GLCHK (glVertexAttribPointer(/*attrib location*/coord_loc,
                                     /*size*/2, GL_FLOAT, /*normalize*/GL_FALSE,
                                     /*stride*/0, /*ptr*/0));
        GLCHK (glEnableVertexAttribArray (coord_loc) );


        GLCHK (glUniform1i(sampler_loc, 0 /*GL_TEXTURE0*/));


        result = true;
        fclose(fd);
    } else {
        result = false;
    }

    return result;
}

int main()
{
    i32 width  = 640;
    i32 height = 480;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_Window* window = SDL_CreateWindow("Font Test",
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
    GLFont font;

    setup_resources(&font_state, width, height);
    if (!bake_font(&font_state, &font, 512, 512, 80)) {
        fprintf(stderr, "Could not bake font!. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    bool should_quit = false;

    SDL_Event sdl_event;
    while ( !should_quit ) {
        while ( SDL_PollEvent(&sdl_event) ) {
            switch ( sdl_event.type ) {
            case SDL_KEYDOWN:
                {
                    SDL_Keycode keycode = sdl_event.key.keysym.sym;
                    switch ( keycode ) {
                    case SDLK_ESCAPE:
                        should_quit = true;
                    }
                    break;
                }
            case SDL_QUIT:
                should_quit = true;
                break;
            case SDL_WINDOWEVENT:
                {
                    switch ( sdl_event.window.event ) {
                    case SDL_WINDOWEVENT_RESIZED:
                        on_resize(&font_state,
                                  sdl_event.window.data1,
                                  sdl_event.window.data2);
                        break;
                    }
                    break;
                }
            }
        }

        glClearColor(1,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        render(&font_state, &font, "Hola mundo!",
               (float[3]){ 0, 0.5, 0.8 },
               10.0f, (float)font_state.screen_height / 2);

        SDL_GL_SwapWindow(window);

        // Uncomment this after setting up a timer.
        // Sleep until the timer ticks.
        SDL_WaitEvent(NULL);
    }

    return EXIT_SUCCESS;
}
