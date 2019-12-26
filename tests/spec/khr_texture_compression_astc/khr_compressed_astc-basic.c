/*
 * Copyright © 2015 Intel Corporation
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

#include "piglit-util-gl.h"
#include "common.h"

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_compat_version = 11;
	config.supports_gl_es_version = 20;
	config.window_visual = PIGLIT_GL_VISUAL_RGB | PIGLIT_GL_VISUAL_DOUBLE;

PIGLIT_GL_TEST_CONFIG_END

static const GLenum cube_map_face_targets[] = {
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

static const GLenum good_compressed_tex_3d_targets[] = {
	GL_TEXTURE_2D_ARRAY,
	GL_TEXTURE_CUBE_MAP_ARRAY_EXT,
	GL_TEXTURE_3D,
};

#define REQUIRE_ERROR(expected_error) \
do { \
	if (!piglit_check_gl_error(expected_error)) \
		piglit_report_result(PIGLIT_FAIL); \
} while (0)


bool static inline
have_tex_storage_support()
{
#if defined (PIGLIT_USE_OPENGL)
	return piglit_get_gl_version() >= 42 ||
		piglit_is_extension_supported("GL_ARB_texture_storage");
#else
	return piglit_get_gl_version() >= 30 ||
		piglit_is_extension_supported("GL_EXT_texture_storage");
#endif
}


bool static inline
have_cube_map_array_support()
{
#if defined (PIGLIT_USE_OPENGL)
	return piglit_get_gl_version() >= 40 ||
	   piglit_is_extension_supported("GL_ARB_texture_cube_map_array");
#else
	return piglit_get_gl_version() >= 32 ||
	   piglit_is_extension_supported("GL_EXT_texture_cube_map_array");
#endif
}

bool static inline
have_gen_mipmap_support()
{
#if defined (PIGLIT_USE_OPENGL)
	return piglit_get_gl_version() >= 30;
#else
	return piglit_get_gl_version() >= 20;
#endif
}


/*
 * The KHR_texture_compression_astc_ldr spec says:
 *	* An INVALID_OPERATION error is generated by CompressedTexImage3D if
 *	<internalformat> is one of the the formats in table 8.19 and <target>
 *	is not TEXTURE_2D_ARRAY, TEXTURE_CUBE_MAP_ARRAY, or TEXTURE_3D.
 *
 *	* An INVALID_OPERATION error is generated by CompressedTexImage3D if
 *	<internalformat> is TEXTURE_CUBE_MAP_ARRAY and the "Cube Map Array"
 *	column of table 8.19 is *not* checked, or if <internalformat> is
 *	TEXTURE_3D and the "3D Tex." column of table 8.19 is *not* checked"
 *
 * Discussion:
 *	* Since this extension only increases the allowed targets, the
 *	existing errors are assumed to be already handled, and the allowed
 *	targets are tested to be free of errors.
 *
 *	* Since all ASTC formats have the "Cube Map Array" column checked,
 *	test that no error is generated from calling CompressedTexImage3D with
 *	the TEXTURE_CUBE_MAP_ARRAY target.
 *
 */
void test_compressed_teximg_3d(int fi, bool have_cube_map_ext,
			       bool have_hdr_or_sliced_3d)
{
	int j;
	GLuint tex3D;
	char fake_tex_data[6*16];

	for (j = 0; j < ARRAY_SIZE(good_compressed_tex_3d_targets); ++j) {

		/* Skip the cube_map target if there's no support */
		if ((good_compressed_tex_3d_targets[j] ==
                    GL_TEXTURE_CUBE_MAP_ARRAY_EXT) && !have_cube_map_ext)
			continue;

		/* Run the command */
		glGenTextures(1, &tex3D);
		glBindTexture(good_compressed_tex_3d_targets[j], tex3D);
		glCompressedTexImage3D(good_compressed_tex_3d_targets[j], 0,
		formats[fi].fmt, 4, 4, 6, 0, 6*formats[fi].bb, fake_tex_data);

		/* Test expected GL errors */
		if (good_compressed_tex_3d_targets[j] == GL_TEXTURE_3D
		    && !have_hdr_or_sliced_3d) {
			REQUIRE_ERROR(GL_INVALID_OPERATION);
		} else {
			REQUIRE_ERROR(GL_NO_ERROR);
		}

		glDeleteTextures(1, &tex3D);
	}
}

/*
 * The KHR_texture_compression_astc_ldr spec says:
 *  "An INVALID_VALUE error is generated by
 *
 *     * CompressedTexImage2D if <target> is
 *       one of the cube map face targets from table 8.21, and
 *     * CompressedTexImage3D if <target> is TEXTURE_CUBE_MAP_ARRAY,
 *
 *   and <width> and <height> are not equal."
 */
void test_non_square_img(int fi, bool have_hdr)
{
	int j;
	char fake_tex_data[6*16];
	GLuint cube_tex;

	glGenTextures(1, &cube_tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY_EXT, cube_tex);

	/* Test CompressedTexImage2D */
	for (j = 0; j < ARRAY_SIZE(cube_map_face_targets); ++j) {
		glCompressedTexImage2D(cube_map_face_targets[j], 0,
					formats[fi].fmt, 4, 3, 0,
					formats[fi].bb, fake_tex_data);
		REQUIRE_ERROR(GL_INVALID_VALUE);
	}

	/* Test CompressedTexImage3D */
	glCompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY_EXT, 0,
		formats[fi].fmt, 4, 3, 6, 0, 6*formats[fi].bb, fake_tex_data);
	REQUIRE_ERROR(GL_INVALID_VALUE);

	glDeleteTextures(1, &cube_tex);

}

int get_expected_size(int width, int height, int bw, int bh, int bb)
{
	int nbw = (width + bw - 1) / bw;
	int nbh = (height + bh - 1) / bh;
	return nbw * nbh * bb;
}


/*
 * The KHR_texture_compression_astc_ldr spec says:
 *  * "An INVALID_OPERATION error is generated if format is one of the formats
 *    in table 8.19 and any of the following conditions occurs. The block
 *    width and height refer to the values in the corresponding column of the
 *    table.
 *
 *      * <width> is not a multiple of the format's block width, and <width> +
 *        <xoffset> is not equal to the value of TEXTURE_WIDTH.
 *      * height is not a multiple of the format's block height, and <height>
 *        + <yoffset> is not equal to the value of TEXTURE_HEIGHT.
 *      * <xoffset> or <yoffset> is not a multiple of the block width or
 *        height, respectively."
 *
 *   [...]
 *
 *   ASTC texture formats are supported by immutable-format textures only if
 *   such textures are supported by the underlying implementation (e.g.
 *   OpenGL 4.1 or later, OpenGL ES 3.0 or later, or earlier versions
 *   supporting the GL_EXT_texture_storage extension). Otherwise, remove all
 *   references to the Tex*Storage* commands from this specification.
 */
void test_sub_img(int fi)
{
	GLuint tex;
	char fake_tex_data[4*16];
	int width = 7;
	int height = 7;
	int expected_size_good = get_expected_size(4, 4,
				formats[fi].bw,
				formats[fi].bh, formats[fi].bb);
	int expected_size_bad = get_expected_size(width, height,
				formats[fi].bw,
				formats[fi].bh, formats[fi].bb);

	/* Ensure enough space has been allocated */
	assert(expected_size_bad  <= sizeof(fake_tex_data));
	assert(expected_size_good <= sizeof(fake_tex_data));

	/* Allocate enough to hold the larger case */
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, formats[fi].fmt, 14, 14);
	REQUIRE_ERROR(GL_NO_ERROR);

	/* Check for No Error */
	glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		formats[fi].bw, formats[fi].bh,
		formats[fi].fmt, expected_size_good, fake_tex_data);
	REQUIRE_ERROR(GL_NO_ERROR);

	/* Check for expected Error */
	glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			formats[fi].fmt, expected_size_bad, fake_tex_data);
	REQUIRE_ERROR(GL_INVALID_OPERATION);

	glDeleteTextures(1, &tex);

	/* Check for expected Error */
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_3D, tex);
	glTexStorage3D(GL_TEXTURE_3D, 1, formats[fi].fmt, 14, 14, 1);
	glCompressedTexImage3D(GL_TEXTURE_3D, 0, formats[fi].fmt,
			7, 7, 1, 0, expected_size_bad, fake_tex_data);
	REQUIRE_ERROR(GL_INVALID_OPERATION);
	glDeleteTextures(1, &tex);

	/* XXX : Test the methods exposed by GL_EXT_texture_storage
	 * once support is added in Mesa.
	*/
}

/*
 * The KHR_texture_compression_astc_ldr spec says:
 *	[...] the ASTC format specifiers will not be added to
 *	Table 3.14, and thus will not be accepted by the TexImage*D
 *	functions, and will not be returned by the (already deprecated)
 *	COMPRESSED_TEXTURE_FORMATS query.
 *
 * Discussion:
 *	The deprecated query is handled by:
 *	tests/spec/arb_texture_compression/invalid-formats.c
 *	In TexImage*D, the format should be automatically
 *	converted to the base internal format of GL_RGBA.
 */
void test_tex_img(int fi, bool have_mipmap)
{
	GLuint tex;
	char fake_tex_data[16];

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	/* Since TexImage*D must fail, then the corresponding TexSubImage
	 * calls must fail as well.
	 */
	glCompressedTexImage2D(GL_TEXTURE_2D, 0,
		formats[fi].fmt, 4, 4, 0, formats[fi].bb, fake_tex_data);
	REQUIRE_ERROR(GL_NO_ERROR);

	/* Check for expected error from Tex*Image*D family of functions */
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		4, 4, GL_RGBA, GL_UNSIGNED_BYTE, fake_tex_data);
	REQUIRE_ERROR(GL_INVALID_OPERATION);

	glTexImage2D(GL_TEXTURE_2D, 0,
		formats[fi].fmt, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	REQUIRE_ERROR(GL_INVALID_OPERATION);

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
	REQUIRE_ERROR(GL_INVALID_OPERATION);

	glCopyTexImage2D(GL_TEXTURE_2D, 0, formats[fi].fmt, 0, 0, 4, 4, 0);
	REQUIRE_ERROR(GL_INVALID_OPERATION);

	/* Check for expected error from the online compression resulting from
	 * calling GenerateMipmap.
	 */
	if (have_mipmap) {
		glGenerateMipmap(GL_TEXTURE_2D);
		REQUIRE_ERROR(GL_INVALID_OPERATION);
	}

	glDeleteTextures(1, &tex);

}

void
piglit_init(int argc, char **argv)
{
	piglit_require_extension("GL_KHR_texture_compression_astc_ldr");
}

enum piglit_result
piglit_display(void)
{
	unsigned i;
	bool have_cube_map_ext = have_cube_map_array_support();
	bool have_tex_stor_ext = have_tex_storage_support();
	bool have_gen_mipmap   = have_gen_mipmap_support();
	bool have_hdr = piglit_is_extension_supported(
			"GL_KHR_texture_compression_astc_hdr");
	bool have_hdr_or_sliced_3d = have_hdr ||
			piglit_is_extension_supported(
			"GL_KHR_texture_compression_astc_sliced_3d");

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		test_tex_img(i, have_gen_mipmap);

		if (have_cube_map_ext)
			test_non_square_img(i, have_hdr);
		if (have_tex_stor_ext)
			test_sub_img(i);
		test_compressed_teximg_3d(i, have_cube_map_ext,
					  have_hdr_or_sliced_3d);
	}

	piglit_report_result(PIGLIT_PASS);
}
