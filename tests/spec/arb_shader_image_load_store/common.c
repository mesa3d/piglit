/*
 * Copyright (C) 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/** @file common.c
 *
 * Common utility functions for the ARB_shader_image_load_store tests.
 */

#include "common.h"

bool
set_uniform_int(GLuint prog, const char *name, int value)
{
        int loc, last_prog;

        loc = glGetUniformLocation(prog, name);
        if (loc < 0)
                return true;

        glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
        if (prog != last_prog)
                glUseProgram(prog);

        glUniform1i(loc, value);

        return piglit_check_gl_error(GL_NO_ERROR);
}

static GLuint *textures, *buffers;
static unsigned num_textures, num_buffers;

GLuint
get_texture(unsigned unit)
{
        if (unit >= num_textures) {
                const unsigned n = unit + 1;

                textures = realloc(textures, sizeof(GLuint) * n);
                memset(&textures[num_textures], 0,
                       sizeof(GLuint) * (n - num_textures));
                num_textures = n;
        }

        return textures[unit];
}

GLuint
get_buffer(unsigned unit)
{
        if (unit >= num_buffers) {
                const unsigned n = unit + 1;

                buffers = realloc(buffers, sizeof(GLuint) * n);
                memset(&buffers[num_buffers], 0,
                       sizeof(GLuint) * (n - num_buffers));
                num_buffers = n;
        }

        return buffers[unit];
}

static GLuint fb[2], cb[2], zb[2];
static GLfloat vp[2][4];

static bool
generate_fb(const struct grid_info grid, unsigned idx)
{
        if (!fb[idx]) {
                glGenFramebuffers(1, &fb[idx]);
                glGenRenderbuffers(1, &cb[idx]);
                glGenRenderbuffers(1, &zb[idx]);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fb[idx]);

        glBindRenderbuffer(GL_RENDERBUFFER, cb[idx]);
        glRenderbufferStorage(GL_RENDERBUFFER, grid.format->format,
                              grid.size.x, grid.size.y);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, cb[idx]);

        glBindRenderbuffer(GL_RENDERBUFFER, zb[idx]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F,
                              grid.size.x, grid.size.y);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, zb[idx]);

        vp[idx][0] = 0;
        vp[idx][1] = 0;
        vp[idx][2] = grid.size.x;
        vp[idx][3] = grid.size.y;
        glViewportIndexedfv(0, vp[idx]);

        return piglit_check_gl_error(GL_NO_ERROR);
}

bool
upload_image(const struct image_info img, unsigned unit,
             const uint32_t *pixels)
{
        return upload_image_levels(img, 1, 0, unit, pixels);
}

bool
upload_image_levels(const struct image_info img, unsigned num_levels,
                    unsigned level, unsigned unit, const uint32_t *pixels)
{
        const unsigned m = image_num_components(img.format);
        int i, l;

        if (get_texture(unit)) {
                glDeleteTextures(1, &textures[unit]);
                textures[unit] = 0;
        }

        if (get_buffer(unit)) {
                glDeleteBuffers(1, &buffers[unit]);
                buffers[unit] = 0;
        }

        glGenTextures(1, &textures[unit]);
        glBindTexture(img.target->target, textures[unit]);

        if (img.target->target != GL_TEXTURE_BUFFER) {
            glTexParameteri(img.target->target, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(img.target->target, GL_TEXTURE_MAX_LEVEL, num_levels - 1);
        }

        switch (img.target->target) {
        case GL_TEXTURE_1D:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage1D(GL_TEXTURE_1D, l, img.format->format,
                                     size.x, 0, img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_2D:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage2D(GL_TEXTURE_2D, l, img.format->format,
                                     size.x, size.y, 0,
                                     img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_3D:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage3D(GL_TEXTURE_3D, l, img.format->format,
                                     size.x, size.y, size.z, 0,
                                     img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_RECTANGLE:
                assert(num_levels == 1);

                glTexImage2D(GL_TEXTURE_RECTANGLE, 0, img.format->format,
                             img.size.x, img.size.y, 0, img.format->pixel_format,
                             image_base_type(img.format), pixels);
                break;

        case GL_TEXTURE_CUBE_MAP:
                for (l = 0; l < num_levels; ++l) {
                        const unsigned offset = m * image_level_offset(img, l);
                        const struct image_extent size = image_level_size(img, l);
                        const unsigned face_sz = m * product(size) / 6;

                        for (i = 0; i < 6; ++i)
                                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, l,
                                             img.format->format, size.x, size.y, 0,
                                             img.format->pixel_format,
                                             image_base_type(img.format),
                                             &pixels[offset + face_sz * i]);
                }
                break;

        case GL_TEXTURE_BUFFER: {
                /*
                 * glTexImage*() isn't supposed to work with buffer
                 * textures.  We copy the unpacked pixels to a texture
                 * with the desired internal format to let the GL pack
                 * them for us.
                 */
                const struct image_extent grid = image_optimal_extent(img.size);
                GLuint packed_tex;

                assert(num_levels == 1);

                glGenBuffers(1, &buffers[unit]);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, buffers[unit]);
                glBufferData(GL_PIXEL_PACK_BUFFER,
                             img.size.x * image_pixel_size(img.format) / 8,
                             NULL, GL_STATIC_DRAW);

                glGenTextures(1, &packed_tex);
                glBindTexture(GL_TEXTURE_2D, packed_tex);

                glTexImage2D(GL_TEXTURE_2D, 0, img.format->format,
                             grid.x, grid.y, 0, img.format->pixel_format,
                             image_base_type(img.format), pixels);
                glGetTexImage(GL_TEXTURE_2D, 0, img.format->pixel_format,
                              img.format->pixel_type, NULL);
                glDeleteTextures(1, &packed_tex);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

                glTexBuffer(GL_TEXTURE_BUFFER, image_compat_format(img.format),
                            buffers[unit]);
                break;
        }
        case GL_TEXTURE_1D_ARRAY:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage2D(GL_TEXTURE_1D_ARRAY, l, img.format->format,
                                     size.x, size.y, 0, img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_2D_ARRAY:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage3D(GL_TEXTURE_2D_ARRAY, l, img.format->format,
                                     size.x, size.y, size.z, 0,
                                     img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_CUBE_MAP_ARRAY:
                for (l = 0; l < num_levels; ++l) {
                        const struct image_extent size = image_level_size(img, l);

                        glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, l, img.format->format,
                                     size.x, size.y, size.z, 0,
                                     img.format->pixel_format,
                                     image_base_type(img.format),
                                     &pixels[m * image_level_offset(img, l)]);
                }
                break;

        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: {
                /*
                 * GL doesn't seem to provide any direct way to
                 * initialize a multisample texture, so we use
                 * imageStore() to render to it from the fragment
                 * shader copying the contents of a larger
                 * single-sample 2D texture.
                 */
                const struct grid_info grid = {
                        get_image_stage(GL_FRAGMENT_SHADER)->bit,
                        img.format,
                        image_optimal_extent(img.size)
                };
                GLuint prog = generate_program(
                        grid, GL_FRAGMENT_SHADER,
                        concat(image_hunk(image_info_for_grid(grid), "SRC_"),
                               image_hunk(img, "DST_"),
                               hunk("readonly SRC_IMAGE_UNIFORM_T src_img;\n"
                                    "writeonly DST_IMAGE_UNIFORM_T dst_img;\n"
                                    "\n"
                                    "GRID_T op(ivec2 idx, GRID_T x) {\n"
                                    "       imageStore(dst_img, DST_IMAGE_ADDR(idx),\n"
                                    "          imageLoad(src_img, SRC_IMAGE_ADDR(idx)));\n"
                                    "       return x;\n"
                                    "}\n"), NULL));
                bool ret = prog && generate_fb(grid, 1);
                GLuint tmp_tex;

                assert(num_levels == 1);

                glGenTextures(1, &tmp_tex);
                glBindTexture(GL_TEXTURE_2D, tmp_tex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

                if (img.target->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
                        glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                                img.size.x, img.format->format,
                                                img.size.y, img.size.z, img.size.w,
                                                GL_FALSE);
                } else {
                        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                                img.size.x, img.format->format,
                                                img.size.y, img.size.z,
                                                GL_FALSE);
                }

                glTexImage2D(GL_TEXTURE_2D, 0, img.format->format,
                             grid.size.x, grid.size.y, 0,
                             img.format->pixel_format, image_base_type(img.format),
                             pixels);

                glBindImageTexture(unit, textures[unit], 0, GL_TRUE, 0,
                                   GL_WRITE_ONLY, img.format->format);
                glBindImageTexture(6, tmp_tex, 0, GL_TRUE, 0,
                                   GL_READ_ONLY, img.format->format);

                ret &= set_uniform_int(prog, "src_img", 6) &&
                        set_uniform_int(prog, "dst_img", unit) &&
                        draw_grid(grid, prog);

                glDeleteProgram(prog);
                glDeleteTextures(1, &tmp_tex);

                glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
                glViewportIndexedfv(0, vp[0]);

                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                if (!ret)
                        return false;
                break;
        }
        default:
                abort();
        }

        glBindImageTexture(unit, textures[unit], level, GL_TRUE, 0,
                           GL_READ_WRITE, img.format->format);

        return piglit_check_gl_error(GL_NO_ERROR);
}

bool
download_image(const struct image_info img, unsigned unit,
               uint32_t *r_pixels)
{
        return download_image_levels(img, 1, unit, r_pixels);
}

bool
download_image_levels(const struct image_info img, unsigned num_levels,
                      unsigned unit, uint32_t *r_pixels)
{
        const unsigned m = image_num_components(img.format);
        int i, l;

        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT |
                        GL_BUFFER_UPDATE_BARRIER_BIT |
                        GL_PIXEL_BUFFER_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glBindTexture(img.target->target, textures[unit]);

        switch (img.target->target) {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
                assert(img.target->target != GL_TEXTURE_RECTANGLE ||
                       num_levels == 1);

                for (l = 0; l < num_levels; ++l)
                        glGetTexImage(img.target->target, l,
                                      img.format->pixel_format,
                                      image_base_type(img.format),
                                      &r_pixels[m * image_level_offset(img, l)]);
                break;

        case GL_TEXTURE_CUBE_MAP:
                for (l = 0; l < num_levels; ++l) {
                        const unsigned offset = m * image_level_offset(img, l);
                        const unsigned face_sz =
                                m * product(image_level_size(img, l)) / 6;

                        for (i = 0; i < 6; ++i)
                                glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                              l, img.format->pixel_format,
                                              image_base_type(img.format),
                                              &r_pixels[offset + face_sz * i]);

                }
                break;

        case GL_TEXTURE_BUFFER: {
                /*
                 * glGetTexImage() isn't supposed to work with buffer
                 * textures.  We copy the packed pixels to a texture
                 * with the same internal format as the image to let
                 * the GL unpack it for us.
                 */
                const struct image_extent grid = image_optimal_extent(img.size);
                GLuint packed_tex;

                assert(num_levels == 1);

                glGenTextures(1, &packed_tex);
                glBindTexture(GL_TEXTURE_2D, packed_tex);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffers[unit]);

                glTexImage2D(GL_TEXTURE_2D, 0, img.format->format,
                             grid.x, grid.y, 0, img.format->pixel_format,
                             img.format->pixel_type, NULL);
                glGetTexImage(GL_TEXTURE_2D, 0, img.format->pixel_format,
                              image_base_type(img.format), r_pixels);

                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                glDeleteTextures(1, &packed_tex);
                break;
        }
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: {
                /*
                 * GL doesn't seem to provide any direct way to read
                 * back a multisample texture, so we use imageLoad()
                 * to copy its contents to a larger single-sample 2D
                 * texture from the fragment shader.
                 */
                const struct grid_info grid = {
                        get_image_stage(GL_FRAGMENT_SHADER)->bit,
                        img.format,
                        image_optimal_extent(img.size)
                };
                GLuint prog = generate_program(
                        grid, GL_FRAGMENT_SHADER,
                        concat(image_hunk(img, "SRC_"),
                               image_hunk(image_info_for_grid(grid), "DST_"),
                               hunk("readonly SRC_IMAGE_UNIFORM_T src_img;\n"
                                    "writeonly DST_IMAGE_UNIFORM_T dst_img;\n"
                                    "\n"
                                    "GRID_T op(ivec2 idx, GRID_T x) {\n"
                                    "       imageStore(dst_img, DST_IMAGE_ADDR(idx),\n"
                                    "          imageLoad(src_img, SRC_IMAGE_ADDR(idx)));\n"
                                    "       return x;\n"
                                    "}\n"), NULL));
                bool ret = prog && generate_fb(grid, 1);
                GLuint tmp_tex;

                assert(num_levels == 1);

                glGenTextures(1, &tmp_tex);
                glBindTexture(GL_TEXTURE_2D, tmp_tex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

                glTexImage2D(GL_TEXTURE_2D, 0, img.format->format,
                             grid.size.x, grid.size.y, 0,
                             img.format->pixel_format, image_base_type(img.format),
                             NULL);

                glBindImageTexture(unit, textures[unit], 0, GL_TRUE, 0,
                                   GL_READ_ONLY, img.format->format);
                glBindImageTexture(6, tmp_tex, 0, GL_TRUE, 0,
                                   GL_WRITE_ONLY, img.format->format);

                ret &= set_uniform_int(prog, "src_img", unit) &&
                        set_uniform_int(prog, "dst_img", 6) &&
                        draw_grid(grid, prog);

                glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);

                glGetTexImage(GL_TEXTURE_2D, 0, img.format->pixel_format,
                              image_base_type(img.format), r_pixels);

                glDeleteProgram(prog);
                glDeleteTextures(1, &tmp_tex);

                glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
                glViewportIndexedfv(0, vp[0]);

                if (!ret)
                        return false;
                break;
        }
        default:
                abort();
        }

        return piglit_check_gl_error(GL_NO_ERROR);
}

bool
init_pixels(const struct image_info img, uint32_t *r_pixels,
            double r, double g, double b, double a)
{
        const double init[4] = { r, g, b, a };
        const unsigned m = image_num_components(img.format);
        unsigned i, j;

        for (i = 0; i < product(img.size); ++i) {
                for (j = 0; j < m; ++j) {
                        r_pixels[i * m + j] = encode(img.format, init[j]);
                }
        }

        return true;
}

static bool
check_pixels_vs(const struct image_info img, unsigned stride,
                const uint32_t *pixels, const uint32_t *expect)
{
        const unsigned m = image_num_components(img.format);
        unsigned i, j;

        for (i = 0; i < product(img.size); ++i) {
                const uint32_t *v = &pixels[m * i];
                const uint32_t *u = &expect[stride * m * i];

                for (j = 0; j < m; ++j) {
                        if ((fabs(decode(img.format, v[j]) - decode(img.format, u[j])) >
                             get_idx(img.epsilon, j)) &&
                            isfinite(decode(img.format, u[j]))) {
                                printf("Probe value at (%u, %u, %u, %u)\n",
                                       i % img.size.x,
                                       i / img.size.x % img.size.y,
                                       i / img.size.x / img.size.y % img.size.z,
                                       i / img.size.x / img.size.y / img.size.z);

                                printf("  Expected:");

                                for (j = 0; j < m; ++j)
                                        printf(" %f", decode(img.format, u[j]));

                                printf("\n  Observed:");

                                for (j = 0; j < m; ++j)
                                        printf(" %f", decode(img.format, v[j]));

                                printf("\n");
                                return false;
                        }
                }
        }

        return true;
}

bool
check_pixels(const struct image_info img, const uint32_t *pixels,
             double r, double g, double b, double a)
{
        const uint32_t expect[4] = {
                encode(img.format, r), encode(img.format, g),
                encode(img.format, b), encode(img.format, a)
        };

        return check_pixels_vs(img, 0, pixels, expect);
}

bool
check_pixels_v(const struct image_info img, const uint32_t *pixels,
               const uint32_t *expect)
{
        return check_pixels_vs(img, 1, pixels, expect);
}

bool
init_fb(const struct grid_info grid)
{
        bool ret = true;

        if (grid.stages & GL_COMPUTE_SHADER_BIT) {
                const struct image_info img = image_info_for_grid(grid);
                const unsigned n = product(grid.size) *
                        image_num_components(grid.format);
                uint32_t *pixels = malloc(n * sizeof(*pixels));

                ret = init_pixels(img, pixels, 0.5, 0.5, 0.5, 0.5) &&
                        upload_image(img, max_image_units(), pixels);

                free(pixels);
        } else {
                ret = generate_fb(grid, 0);

                glClearColor(0.5, 0.5, 0.5, 0.5);
                glClear(GL_COLOR_BUFFER_BIT);

                glClearDepth(0.5);
                glClear(GL_DEPTH_BUFFER_BIT);
        }

        return ret;
}

bool
download_result(const struct grid_info grid, uint32_t *r_pixels)
{
        if (grid.stages & GL_COMPUTE_SHADER_BIT) {
                /* No actual framebuffer.  Results are returned into
                 * an image. */
                return download_image(image_info_for_grid(grid),
                                      max_image_units(), r_pixels);

        } else {
                glReadPixels(0, 0, grid.size.x, grid.size.y,
                             grid.format->pixel_format,
                             image_base_type(grid.format),
                             r_pixels);
                return piglit_check_gl_error(GL_NO_ERROR);
        }
}
