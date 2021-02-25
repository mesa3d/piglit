/*
 * Copyright © 2009 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


/**
 * \file rgba-mipmap-texture-with-rgb-visual.c
 * Verify generation of an RGBA mipmap stack with an RGB (no alpha) visual.
 * This tests part of the regression (related to piglit test glsl-lod-bias)
 * reported in bugzilla #25614.
 *
 * \author Ian Romanick
 */

#include "piglit-util-gl.h"

/* Pick the number of LODs to examine and the size of the texture so that the
 * smallest LOD is the one where each of the 4x4 tiles in the checkerboard
 * texture is 1x1.
 */
#define TEST_COLS 5
#define BOX_SIZE  (1 << (TEST_COLS + 1))

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_compat_version = 14;

	config.window_width = (BOX_SIZE+2)*TEST_COLS+1;
	config.window_height = (BOX_SIZE+1)+1;
	config.window_visual = PIGLIT_GL_VISUAL_RGB | PIGLIT_GL_VISUAL_DOUBLE;
	config.khr_no_error_support = PIGLIT_NO_ERRORS;

PIGLIT_GL_TEST_CONFIG_END

static GLuint tex[1];

static void loadTex(void);

static const float clear_color[4] = {0.6, 0.6, 0.6, 1.0};
static const float green[4] = {0.0, 1.0, 0.0, 1.0};
static const float pink[4] = {1.0, 0.0, 1.0, 0.0}; /* Note: 0.0 alpha */

void
piglit_init(int argc, char **argv)
{
	GLint alpha_bits;

	(void) argc;
	(void) argv;

	piglit_require_gl_version(14);

	loadTex();

	glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
	if (alpha_bits == 0)
		piglit_report_result(PIGLIT_SKIP);

	piglit_ortho_projection(piglit_width, piglit_height, GL_FALSE);
	glEnable(GL_TEXTURE_2D);
	glClearColor(clear_color[0], clear_color[1],
		     clear_color[2], clear_color[3]);
}

static void
loadTex(void)
{
	#define height BOX_SIZE
	#define width BOX_SIZE
	int i, j;

	GLfloat texData[width][height][4];
	for (i=0; i < width; ++i) {
		for (j=0; j < height; ++j) {
			if ((i ^ j) & (BOX_SIZE / 4)) {
				memcpy(texData[i][j], pink, sizeof(pink));
			}
			else {
				memcpy(texData[i][j], green, sizeof(green));
			}
		}
	}

	glGenTextures(1, tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
			GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
			GL_RGBA, GL_FLOAT, texData);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	#undef height
	#undef width
}


enum piglit_result
piglit_display(void)
{
	GLboolean pass = GL_TRUE;
	unsigned i;

	glClear(GL_COLOR_BUFFER_BIT);

	for (i = 0; i < 256; i++) {
		int width;

		glGetTexLevelParameteriv(GL_TEXTURE_2D, i, GL_TEXTURE_WIDTH,
					 & width);

		if (width < 4)
			break;

		/* The middle of the lower left tile should be green, and the
		 * middle of the tile next to it should be the clear color.
		 */
		if (!piglit_probe_texel_rgba(GL_TEXTURE_2D, i,
					     0, 0, green)
		    || !piglit_probe_texel_rgba(GL_TEXTURE_2D, i,
						width - 1, width -1, green)
		    || !piglit_probe_texel_rgba(GL_TEXTURE_2D, i,
						0, width - 1, pink)
		    || !piglit_probe_texel_rgba(GL_TEXTURE_2D, i,
						width - 1, 0, pink)) {
			if (!piglit_automatic)
				printf("  level = %d\n", i);
			pass = GL_FALSE;
		}
	}

	piglit_present_results();

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}
