/* GKD350H Ultra post-processing overlay for OnscripterYuri.
 *
 * The phosphor modes are performance-oriented GLES2 adaptations of Roc-Y's
 * Phosphor Line v2.0 and Phosphor Dot v3.3 RetroArch shaders. The original
 * shader sources and licenses are kept under ROCgalgame/shaders.
 */
#if defined(USE_GLES)
#include "renderer/gles_renderer.h"
#include "Utils.h"

#include <cstdlib>
#include <cstring>

extern "C" bool ROCGalgameGetVirtualMouse(int *x, int *y);
extern "C" bool ROCGalgameVirtualMousePressed();

#if defined(IOS) || defined(ANDROID)
#include <GLES2/gl2.h>
#endif

#define GLES_CHECK_ERROR(tag) \
    do { \
        GLenum err = glGetError(); \
        if (err != GL_NO_ERROR) { \
            utils::printError("%s: GLES error: (0x%X)\n", tag, err); \
        } \
    } while (0)

namespace {
const GLchar *kPostVertexShader = R"GLSL(
#version 100
attribute vec2 a_position;

void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)GLSL";

const GLchar *kPostFragmentShader = R"GLSL(
#version 100
precision highp float;
precision highp int;

uniform sampler2D u_texture;
uniform vec4 u_sizes;
uniform vec4 u_view;
uniform vec4 u_cursor;

const float kPi = 3.14159265358979323846;

vec3 sampleSource(vec2 uv) {
    return texture2D(u_texture, clamp(uv, vec2(0.0), vec2(1.0))).bgr;
}

vec3 fxaa(vec2 uv) {
    vec2 inverseSize = 1.0 / u_sizes.xy;
    vec3 rgbNW = sampleSource(uv + vec2(-1.0, -1.0) * inverseSize);
    vec3 rgbNE = sampleSource(uv + vec2( 1.0, -1.0) * inverseSize);
    vec3 rgbSW = sampleSource(uv + vec2(-1.0,  1.0) * inverseSize);
    vec3 rgbSE = sampleSource(uv + vec2( 1.0,  1.0) * inverseSize);
    vec3 rgbM = sampleSource(uv);
    vec3 lumaWeight = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, lumaWeight);
    float lumaNE = dot(rgbNE, lumaWeight);
    float lumaSW = dot(rgbSW, lumaWeight);
    float lumaSE = dot(rgbSE, lumaWeight);
    float lumaM = dot(rgbM, lumaWeight);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) return rgbM;

    vec2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    float inverseDirection = 1.0 / (min(abs(direction.x), abs(direction.y)) + reduce);
    direction = clamp(direction * inverseDirection, vec2(-8.0), vec2(8.0)) * inverseSize;

    vec3 rgbA = 0.5 * (
        sampleSource(uv + direction * (1.0 / 3.0 - 0.5)) +
        sampleSource(uv + direction * (2.0 / 3.0 - 0.5)));
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        sampleSource(uv + direction * -0.5) +
        sampleSource(uv + direction * 0.5));
    float lumaB = dot(rgbB, lumaWeight);
    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
}

float scanlineBeam(float outputY, float pitch) {
    float phase = mod(outputY + 0.5, pitch) / pitch;
    return 0.62 + 0.38 * pow(max(sin(phase * kPi), 0.0), 0.78);
}

vec3 phosphorLine(vec2 uv, vec2 outputPixel) {
    vec2 inverseSize = 1.0 / u_sizes.xy;
    vec3 center = sampleSource(uv);
    vec3 horizontal = 0.5 * (
        sampleSource(uv - vec2(inverseSize.x * 0.45, 0.0)) +
        sampleSource(uv + vec2(inverseSize.x * 0.45, 0.0)));
    vec3 color = mix(center, horizontal, 0.18);
    float pitch = clamp(floor(u_sizes.w / 260.0 + 0.5), 4.0, 6.0);
    color *= scanlineBeam(outputPixel.y, pitch);
    return pow(max(pow(max(color, vec3(0.0)), vec3(2.2)) * 1.12, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 phosphorDot(vec2 uv, vec2 outputPixel) {
    vec3 source = sampleSource(uv);
    float pitch = clamp(floor(u_sizes.w / 240.0 + 0.5), 4.0, 6.0);
    vec2 cell = mod(outputPixel + vec2(0.5), vec2(pitch)) - vec2(pitch * 0.5);
    float distanceToCenter = length(cell);
    vec3 radius = (source * 0.70 + 0.30) * pitch * 0.52;
    vec3 dotColor = sqrt(clamp(radius - vec3(distanceToCenter) + 0.5, 0.0, 1.0));
    dotColor *= source / max(source * 0.70 + 0.30, vec3(0.001));
    return mix(source, dotColor, 0.72);
}

vec3 sampleReflection(vec2 uv) {
    vec2 texel = 1.0 / u_sizes.xy;
    vec3 color = sampleSource(uv) * 0.44;
    color += sampleSource(uv + vec2(texel.x * 2.0, 0.0)) * 0.14;
    color += sampleSource(uv - vec2(texel.x * 2.0, 0.0)) * 0.14;
    color += sampleSource(uv + vec2(0.0, texel.y * 2.0)) * 0.14;
    color += sampleSource(uv - vec2(0.0, texel.y * 2.0)) * 0.14;
    return color;
}

vec3 containReflection(vec2 screenPixel, vec2 windowSize, vec2 renderOrigin,
                       vec2 renderSize) {
    vec2 renderEnd = renderOrigin + renderSize;
    vec2 local = screenPixel - renderOrigin;
    bool insidePicture = local.x >= 0.0 && local.x <= renderSize.x &&
                         local.y >= 0.0 && local.y <= renderSize.y;
    if (insidePicture) {
        vec2 uv = local / renderSize;
        uv.y = 1.0 - uv.y;
        return sampleSource(uv);
    }

    if (screenPixel.x < renderOrigin.x || screenPixel.x > renderEnd.x) {
        return vec3(0.0);
    }

    float distanceFromPicture;
    float barSize;
    float sourceY;
    if (screenPixel.y < renderOrigin.y) {
        distanceFromPicture = renderOrigin.y - screenPixel.y;
        barSize = max(renderOrigin.y, 1.0);
        sourceY = 1.0 - distanceFromPicture / renderSize.y;
    } else if (screenPixel.y > renderEnd.y) {
        distanceFromPicture = screenPixel.y - renderEnd.y;
        barSize = max(windowSize.y - renderEnd.y, 1.0);
        sourceY = distanceFromPicture / renderSize.y;
    } else {
        return vec3(0.0);
    }

    vec2 reflectedUv = vec2(clamp(local.x / renderSize.x, 0.0, 1.0),
                            clamp(sourceY, 0.002, 0.998));
    float progress = clamp(distanceFromPicture / barSize, 0.0, 1.0);
    float fade = mix(0.30, 0.94, pow(1.0 - progress, 0.68));
    float innerRim = 1.0 - smoothstep(0.0, 5.0, distanceFromPicture);
    vec3 reflection = sampleReflection(reflectedUv) * vec3(0.72, 0.80, 0.90);
    reflection *= fade;
    reflection += vec3(0.055, 0.072, 0.092) * innerRim;
    return reflection;
}

vec3 fullFrameReflection(vec2 screenPixel, vec2 windowSize, vec2 renderOrigin,
                         vec2 renderSize) {
    vec2 visibleOrigin = max(-renderOrigin, vec2(0.0));
    vec2 visibleEnd = min(renderSize, windowSize - renderOrigin);
    vec2 visibleSize = max(visibleEnd - visibleOrigin, vec2(1.0));
    vec2 sourceOrigin = vec2(
        visibleOrigin.x / renderSize.x,
        1.0 - (visibleOrigin.y + visibleSize.y) / renderSize.y);
    vec2 sourceScale = visibleSize / renderSize;

    vec2 border = clamp(windowSize * 0.030, vec2(30.0), vec2(46.0));
    vec2 innerMin = border;
    vec2 innerMax = windowSize - border;
    vec2 innerSize = max(innerMax - innerMin, vec2(1.0));
    bool insidePicture = screenPixel.x >= innerMin.x && screenPixel.x <= innerMax.x &&
                         screenPixel.y >= innerMin.y && screenPixel.y <= innerMax.y;
    if (insidePicture) {
        vec2 innerUv = (screenPixel - innerMin) / innerSize;
        vec2 uv = sourceOrigin + vec2(innerUv.x, 1.0 - innerUv.y) * sourceScale;
        return sampleSource(uv);
    }

    float leftFold = max((innerMin.x - screenPixel.x) / border.x, 0.0);
    float rightFold = max((screenPixel.x - innerMax.x) / border.x, 0.0);
    float bottomFold = max((innerMin.y - screenPixel.y) / border.y, 0.0);
    float topFold = max((screenPixel.y - innerMax.y) / border.y, 0.0);
    float xFold = max(leftFold, rightFold);
    float yFold = max(bottomFold, topFold);
    float largestFold = max(max(xFold, yFold), 0.001);
    float normalizedFoldDelta = (xFold - yFold) / largestFold;
    float verticalSide = smoothstep(-0.55, 0.55, normalizedFoldDelta);
    float sideProgress = mix(yFold, xFold, verticalSide);

    float reflectedX = clamp((screenPixel.x - innerMin.x) / innerSize.x, 0.0, 1.0);
    float reflectedY = clamp((screenPixel.y - innerMin.y) / innerSize.y, 0.0, 1.0);
    float xDepth = xFold * border.x / innerSize.x;
    float yDepth = yFold * border.y / innerSize.y;
    float verticalSourceX = leftFold > 0.0 ? xDepth : 1.0 - xDepth;
    float horizontalSourceY = bottomFold > 0.0 ? 1.0 - yDepth : yDepth;
    vec2 verticalUv = sourceOrigin +
        vec2(clamp(verticalSourceX, 0.002, 0.998), 1.0 - reflectedY) * sourceScale;
    vec2 horizontalUv = sourceOrigin +
        vec2(reflectedX, clamp(horizontalSourceY, 0.002, 0.998)) * sourceScale;

    vec3 verticalReflection = sampleReflection(verticalUv) * vec3(0.70, 0.79, 0.91);
    vec3 horizontalReflection = sampleReflection(horizontalUv) * vec3(0.78, 0.84, 0.92);
    vec3 reflection = mix(horizontalReflection, verticalReflection, verticalSide);
    float fade = mix(0.24, 0.94, pow(1.0 - clamp(sideProgress, 0.0, 1.0), 0.72));
    float cornerZone = step(0.001, xFold) * step(0.001, yFold);
    float cornerCurve = cornerZone *
        (1.0 - smoothstep(0.10, 0.82, abs(normalizedFoldDelta)));
    float innerRim = 1.0 - smoothstep(0.0, 0.075, sideProgress);
    reflection *= fade * mix(1.0, 1.06, cornerCurve);
    reflection += vec3(0.024, 0.031, 0.041) * cornerCurve *
                  (1.0 - clamp(sideProgress, 0.0, 1.0) * 0.62);
    reflection += vec3(0.065, 0.082, 0.105) * innerRim;
    reflection += vec3(0.018, 0.024, 0.032) * (1.0 - fade);
    return reflection;
}

void main() {
    vec2 outputPixel = gl_FragCoord.xy - u_view.xy;
    vec2 uv = outputPixel / u_sizes.zw;
    uv.y = 1.0 - uv.y;
    float mode = u_view.z;
    vec3 color;
    if (mode < 0.5) {
        color = sampleSource(uv);
    } else if (mode < 1.5) {
        color = fxaa(uv);
    } else if (mode < 2.5) {
        color = phosphorLine(uv, outputPixel);
    } else if (mode < 3.5) {
        color = phosphorDot(uv, outputPixel);
    } else {
        float windowWidth = floor(u_view.w / 4096.0);
        float windowHeight = u_view.w - windowWidth * 4096.0;
        vec2 windowSize = max(vec2(windowWidth, windowHeight), vec2(1.0));
        vec2 screenPixel = gl_FragCoord.xy;
        vec2 renderEnd = u_view.xy + u_sizes.zw;
        bool contained = u_view.x >= -0.5 && u_view.y >= -0.5 &&
                         renderEnd.x <= windowSize.x + 0.5 &&
                         renderEnd.y <= windowSize.y + 0.5 &&
                         (u_sizes.z < windowSize.x - 0.5 ||
                          u_sizes.w < windowSize.y - 0.5);
        color = contained
            ? containReflection(screenPixel, windowSize, u_view.xy, u_sizes.zw)
            : fullFrameReflection(screenPixel, windowSize, u_view.xy, u_sizes.zw);
    }
    if (u_cursor.w > 0.5) {
        float cursorDistance = length(gl_FragCoord.xy - u_cursor.xy);
        float ringDistance = abs(cursorDistance - u_cursor.z);
        float darkOutline = 1.0 - smoothstep(1.0, 3.6, ringDistance);
        float whiteRing = 1.0 - smoothstep(0.5, 1.8, ringDistance);
        float centerOutline = 1.0 - smoothstep(2.0, 4.2, cursorDistance);
        float centerDot = 1.0 - smoothstep(1.0, 2.4, cursorDistance);
        color = mix(color, vec3(0.0), max(darkOutline, centerOutline) * 0.72);
        color = mix(color, vec3(1.0), max(whiteRing, centerDot));
    }
    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)GLSL";

float ParseFilterMode() {
    const char *filter = std::getenv("ROCGALGAME_FILTER");
    if (!filter || !*filter || std::strcmp(filter, "clean") == 0) return 0.0f;
    if (std::strcmp(filter, "antialias") == 0 || std::strcmp(filter, "crt-soft") == 0) return 1.0f;
    if (std::strcmp(filter, "scanline") == 0) return 2.0f;
    if (std::strcmp(filter, "dot") == 0 || std::strcmp(filter, "mask") == 0) return 3.0f;
    if (std::strcmp(filter, "reflection") == 0) return 4.0f;
    return 0.0f;
}
}  // namespace

void GlesRenderer::setConstBuffer(const float input_size[2], const float output_size[2], float) {
    this->output_size[0] = static_cast<int>(output_size[0]);
    this->output_size[1] = static_cast<int>(output_size[1]);
    cas_con[0] = input_size[0];
    cas_con[1] = input_size[1];
}

void GlesRenderer::initVertexData() {
    const GLfloat vertices[8] = {-1.0f, 1.0f, 1.0f, 1.0f,
                                 -1.0f, -1.0f, 1.0f, -1.0f};
    for (int i = 0; i < 8; ++i) vertex_data[i] = vertices[i];
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
}

GlesRenderer::GlesRenderer(SDL_Window *window, SDL_Texture *texture,
                           const float input_size[2], const float output_size[2],
                           float sharpness) {
    this->window = window;
    this->texture = texture;
    cas_con[4] = ParseFilterMode();
    SDL_GL_BindTexture(texture, nullptr, nullptr);
    context = SDL_GL_GetCurrentContext();
    if (!context) {
        utils::printError("GlesRenderer context error: %s\n", SDL_GetError());
    }

    vert_shader = createShader(GL_VERTEX_SHADER, kPostVertexShader);
    frag_shader = createShader(GL_FRAGMENT_SHADER, kPostFragmentShader);
    post_program = glCreateProgram();
    glAttachShader(post_program, vert_shader);
    glAttachShader(post_program, frag_shader);
    glBindAttribLocation(post_program, 0, "a_position");
    glLinkProgram(post_program);
    GLint linked = GL_FALSE;
    glGetProgramiv(post_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info_log[1024] = {};
        glGetProgramInfoLog(post_program, sizeof(info_log), nullptr, info_log);
        utils::printError("Failed to link GKD filter shader:\n%s\n", info_log);
        std::exit(-1);
    }
    const_buffer_location[0] = glGetUniformLocation(post_program, "u_sizes");
    const_buffer_location[1] = glGetUniformLocation(post_program, "u_view");
    const_buffer_location[2] = glGetUniformLocation(post_program, "u_cursor");
    glUseProgram(post_program);
    glUniform1i(glGetUniformLocation(post_program, "u_texture"), 0);
    setConstBuffer(input_size, output_size, sharpness);
    initVertexData();
    utils::printInfo("ROCGalgame GLES filter mode: %.0f\n", cas_con[4]);
}

GlesRenderer::~GlesRenderer() {
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteProgram(post_program);
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
}

GLuint GlesRenderer::createShader(GLenum shader_type, const GLchar *shader_src) {
    const GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shader_src, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info_log[1024] = {};
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        utils::printError("Failed to compile GKD filter shader:\n%s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void GlesRenderer::copy(int window_x, int window_y) {
    if (_pause) return;
    if (SDL_GL_GetCurrentContext() != context && SDL_GL_MakeCurrent(window, context) < 0) return;

    glActiveTexture(GL_TEXTURE0);
    SDL_GL_BindTexture(texture, nullptr, nullptr);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glUseProgram(post_program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    int drawable_width = 0;
    int drawable_height = 0;
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
    const bool reflection_mode = cas_con[4] > 3.5f;
    if (reflection_mode) {
        glViewport(0, 0, drawable_width, drawable_height);
    } else {
        glViewport(window_x, window_y, output_size[0], output_size[1]);
    }
    const float packed_window_size = static_cast<float>(
        drawable_width * 4096 + drawable_height);
    int mouse_x = 0;
    int mouse_y = 0;
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    float cursor_radius = 0.0f;
    float cursor_visible = 0.0f;
    if (ROCGalgameGetVirtualMouse(&mouse_x, &mouse_y) && cas_con[0] > 0.0f && cas_con[1] > 0.0f) {
        const float scale_x = static_cast<float>(output_size[0]) / cas_con[0];
        const float scale_y = static_cast<float>(output_size[1]) / cas_con[1];
        const float local_cursor_x = (static_cast<float>(mouse_x) + 0.5f) * scale_x;
        const float local_cursor_y = static_cast<float>(output_size[1]) -
                                     (static_cast<float>(mouse_y) + 0.5f) * scale_y;
        cursor_x = static_cast<float>(window_x) + local_cursor_x;
        cursor_y = static_cast<float>(window_y) + local_cursor_y;
        if (reflection_mode) {
            const bool contained = window_x >= 0 && window_y >= 0 &&
                window_x + output_size[0] <= drawable_width &&
                window_y + output_size[1] <= drawable_height &&
                (output_size[0] < drawable_width || output_size[1] < drawable_height);
            if (!contained) {
                const float visible_origin_x = static_cast<float>(window_x < 0 ? -window_x : 0);
                const float visible_origin_y = static_cast<float>(window_y < 0 ? -window_y : 0);
                const float visible_end_x = static_cast<float>(
                    output_size[0] < drawable_width - window_x
                        ? output_size[0] : drawable_width - window_x);
                const float visible_end_y = static_cast<float>(
                    output_size[1] < drawable_height - window_y
                        ? output_size[1] : drawable_height - window_y);
                const float visible_width = visible_end_x - visible_origin_x;
                const float visible_height = visible_end_y - visible_origin_y;
                const float border_x = utils::clamp(drawable_width * 30 / 1000, 30, 46);
                const float border_y = utils::clamp(drawable_height * 30 / 1000, 30, 46);
                cursor_x = border_x +
                    (local_cursor_x - visible_origin_x) /
                    (visible_width > 1.0f ? visible_width : 1.0f) *
                    (static_cast<float>(drawable_width) - border_x * 2.0f);
                cursor_y = border_y +
                    (local_cursor_y - visible_origin_y) /
                    (visible_height > 1.0f ? visible_height : 1.0f) *
                    (static_cast<float>(drawable_height) - border_y * 2.0f);
            }
        }
        const float output_scale = scale_x > scale_y ? scale_x : scale_y;
        const int normal_radius = utils::clamp(
            static_cast<int>(14.0f * output_scale + 0.5f), 12, 30);
        cursor_radius = static_cast<float>(ROCGalgameVirtualMousePressed()
            ? (normal_radius * 2 / 3 < 9 ? 9 : normal_radius * 2 / 3)
            : normal_radius);
        cursor_visible = 1.0f;
    }
    glUniform4f(const_buffer_location[0], cas_con[0], cas_con[1],
                static_cast<float>(output_size[0]), static_cast<float>(output_size[1]));
    glUniform4f(const_buffer_location[1], static_cast<float>(window_x),
                static_cast<float>(window_y), cas_con[4], packed_window_size);
    glUniform4f(const_buffer_location[2], cursor_x, cursor_y,
                cursor_radius, cursor_visible);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLES_CHECK_ERROR("GKD filter copy");
}

void GlesRenderer::pause() { _pause = true; }

void GlesRenderer::resume() { _pause = false; }
#endif
