/*
 * Copyright © 2013 Intel Corporation
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
#include <stdbool.h>
#include <string.h>

#include "piglit-util-gl.h"
#include "parser_utils.h"

static void parse_file(const char *filename);

struct test_vector {
	const char *name;
	int minimum;
};

struct test_vector tests[500];
unsigned num_tests = 0;

int required_glsl_version = 0;
char required_glsl_version_string[128];
bool es_shader = false;
bool compat_shader = false;
GLenum shader_type = 0;

/**
 * List of extensions required by the current test set.
 */
char *required_extensions[32];
unsigned num_required_extensions = 0;

/**
 * Array of extension enables for the shader code
 *
 * For each used entry in \c required_extensions, there is text in
 * this string of the form "#extension ...: require\n".
 */
#define MAX_EXTENSION_ENABLE_LINE_LEN 80
char extension_enables[ARRAY_SIZE(required_extensions)
		       * MAX_EXTENSION_ENABLE_LINE_LEN];
unsigned extension_enables_len = 0;

static const char *const uniform_template =
	"uniform float f[%s %s %d ? 1 : -1];\n"
	;

static const char *const passthrough_uniform =
	"uniform float f[1];\n"
	;

static const char *const vertex_shader_body =
	"void main() { gl_Position = vec4(f[0]); }\n"
	;

static const char *const tessellation_control_shader_body =
	"layout(vertices = 1) out;\n"
	"void main() { gl_TessLevelInner[0] = f[0]; }\n"
	;

static const char *const tessellation_evaluation_shader_body =
	"void main() { gl_Position = vec4(f[0]); }\n"
	;

static const char *const geometry_shader_body =
	"layout(points) in;\n"
	"layout(points, max_vertices = 1) out;\n"
	"void main() { gl_Position = vec4(f[0]); EmitVertex(); }\n"
	;

static const char *const compute_shader_body =
	"layout(local_size_x = 1) in;\n"
	"void main() { }\n"
	;

/* The __VERSION__ stuff is to work-around gl_FragColor not existing in GLSL
 * ES 3.00.
 */
static const char *const fragment_shader_body =
	"#if __VERSION__ >= 300\n"
	"out vec4 color;\n"
	"#define gl_FragColor color\n"
	"#endif\n"
	"void main() { gl_FragColor = vec4(f[0]); }\n"
	;


PIGLIT_GL_TEST_CONFIG_BEGIN

	parse_file(argv[1]);

	switch (required_glsl_version) {
	case 100:
		config.supports_gl_compat_version = 10;
		config.supports_gl_es_version = 20;
		break;
	case 300:
		config.supports_gl_compat_version = 10;
		config.supports_gl_es_version = 30;
		break;
	case 310:
		config.supports_gl_es_version = 31;

		/* It seems impossible that a desktop OpenGL implementation
		 * would support GL_ARB_ES3_1_compatibility and not support at
		 * least OpenGL 3.2.  Realistically, the compute shader
		 * requirement means that nearly all
		 * GL_ARB_ES3_1_compatibility implementations will be OpenGL
		 * 4.2 or later.
		 */
		if (!compat_shader) {
			config.supports_gl_core_version = 32;
			break;
		}
	default: {
		const unsigned int gl_version
			= required_gl_version_from_glsl_version(required_glsl_version);
		config.supports_gl_compat_version = gl_version;
		if (gl_version < 31 || compat_shader)
			config.supports_gl_core_version = 0;
		else
			config.supports_gl_core_version = gl_version;
		break;
	}
	}

        config.window_visual = PIGLIT_GL_VISUAL_DOUBLE | PIGLIT_GL_VISUAL_RGBA;

PIGLIT_GL_TEST_CONFIG_END

enum piglit_result
piglit_display(void)
{
        /* UNREACHED */
        return PIGLIT_FAIL;
}


/**
 * Comparison function for qsort of test_vector list
 */
static int
compar(const void *_a, const void *_b)
{
	const struct test_vector *a = (const struct test_vector *) _a;
	const struct test_vector *b = (const struct test_vector *) _b;

	return strcmp(a->name, b->name);
}

static GLenum
parse_shader_type(const char *line, ptrdiff_t len)
{
	int i;
	static struct {
		const char *name;
		GLenum type;
	} shader_types[] = {
		{ "GL_VERTEX_SHADER", GL_VERTEX_SHADER },
		{ "GL_TESS_CONTROL_SHADER", GL_TESS_CONTROL_SHADER },
		{ "GL_TESS_EVALUATION_SHADER", GL_TESS_EVALUATION_SHADER },
		{ "GL_GEOMETRY_SHADER", GL_GEOMETRY_SHADER },
		{ "GL_FRAGMENT_SHADER", GL_FRAGMENT_SHADER },
		{ "GL_COMPUTE_SHADER", GL_COMPUTE_SHADER },
	};

	for (i = 0; i < ARRAY_SIZE(shader_types); i++) {
		if (len == strlen(shader_types[i].name) &&
		    strncmp(shader_types[i].name, line, 3) == 0) {
			return shader_types[i].type;
		}
	}

	return 0;
}

/**
 * Parse the file of values to test, fill in test vector list.
 */
void
parse_file(const char *filename)
{
	unsigned text_size;
	char *text = piglit_load_text_file(filename, &text_size);
	char *line = text;
	char *end_of_line;
	ptrdiff_t len;
	char *endptr;

	if (line == NULL) {
		fprintf(stderr, "could not read file \"%s\"\n", filename);
		piglit_report_result(PIGLIT_FAIL);
	}

	/* The format of the test file is:
	 *
	 * version [es|core|compatibility]
	 * GL_VERTEX_SHADER|GL_GEOMETRY_SHADER|GL_FRAGMENT_SHADER|GL_COMPUTE_SHADER
	 * GL_ARB_some_extension
	 * gl_MaxFoo 8
	 * gl_MaxBar 16
	 * gl_MinAsdf -2
	 */

	/* Process the version requirement.
	 */
	end_of_line = strchrnul(line, '\n');
	len = end_of_line - line;

	if (len + 1 >= ARRAY_SIZE(required_glsl_version_string)) {
		fprintf(stderr, "Version line too long.\n");
		piglit_report_result(PIGLIT_FAIL);
	}

	memcpy(required_glsl_version_string, line, len);
	required_glsl_version_string[len] = '\0';

	required_glsl_version = strtol(line, &endptr, 10);
	parse_whitespace(endptr, (const char **)&line);
	es_shader = strncmp("es\n", line, 3) == 0;
	compat_shader = strncmp("compatibility\n", line, 7) == 0;

	if (required_glsl_version <= 0 ||
	    (line != end_of_line &&
	     strncmp("es\n", line, 3) != 0 &&
	     strncmp("core\n", line, 5) != 0 &&
	     strncmp("compatibility\n", line, 7) != 0)) {
		fprintf(stderr, "Parse error in version line.\n");
		piglit_report_result(PIGLIT_FAIL);
	}

	/* Skip to the next line.
	 */
	line = strchrnul(line, '\n');
	if (line[0] != '\0')
		line++;

	end_of_line = strchrnul(line, '\n');
	len = end_of_line - line;
	assert(end_of_line[0] == '\n' || end_of_line[0] == '\0');

	/* Process the shader type.
	 */
	shader_type = parse_shader_type(line, len);
	if (shader_type != 0) {
		/* Advance to the next input line.
		 */
		line = end_of_line;
		if (line[0] == '\n')
			line++;
	}

	/* Process the list of required extensions.
	 */
	while (strncmp("GL_", line, 3) == 0) {
		end_of_line = strchrnul(line, '\n');
		len = end_of_line - line;

		assert(end_of_line[0] == '\n' || end_of_line[0] == '\0');

		if (num_required_extensions >= ARRAY_SIZE(required_extensions)) {
			fprintf(stderr, "Too many required extensions!\n");
			piglit_report_result(PIGLIT_FAIL);
		}

		/* Copy the new extension to the list.
		 */
		required_extensions[num_required_extensions] =
			strndup(line, len);
		num_required_extensions++;

		/* Advance to the next input line.
		 */
		line = end_of_line;
		if (line[0] == '\n')
			line++;
	}

	while (line[0] != '\0') {
		if (!(parse_word(line, (const char **)&line,
				 (const char **)&endptr) &&
		      (parse_str(line, "gl_Max", NULL) ||
		       parse_str(line, "gl_Min", NULL)))) {
			char bad_name[80] = "";
			parse_word_copy(line, bad_name, sizeof(bad_name), NULL);

			fprintf(stderr,
				"Invalid built-in constant name \"%s\".\n",
				bad_name);
			piglit_report_result(PIGLIT_FAIL);
		}

		tests[num_tests].name = line;
		*endptr = 0;
		line = endptr + 1;

		if (!parse_int(line, &tests[num_tests].minimum,
			       (const char **)&endptr)) {
			char bad_number[80] = "";
			parse_word_copy(line, bad_number, sizeof(bad_number),
					NULL);

			fprintf(stderr,
				"Invalid built-in constant value \"%s\".\n",
				bad_number);
			piglit_report_result(PIGLIT_FAIL);
		}
		line = endptr;

		num_tests++;

		/* Skip to the next line.
		 */
		line = strchrnul(line, '\n');
		if (line[0] != '\0')
			line++;
	}

	/* After parsing the full list of values to test, sort the list by
	 * variable name.  This ensures that the piglit results will be
	 * generated in a consistent order... no matter what happens in the
	 * control file.
	 */
	qsort(tests, num_tests, sizeof(tests[0]), compar);
}

static bool
check_compile_status(const char *name, GLuint sh)
{
	GLint ok;

        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLchar *info;
		GLint size;

		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &size);
		info = malloc(size);
		glGetShaderInfoLog(sh, size, NULL, info);

		fprintf(stderr,
			"Failed to compile shader %s: %s\n",
			name, info);

		free(info);
	}

	return !!ok;
}

static bool
is_tessellation_type(GLenum type)
{
	return type == GL_TESS_CONTROL_SHADER ||
		type == GL_TESS_EVALUATION_SHADER;
}

static GLuint
create_shader(GLenum type)
{
	if (shader_type != 0 && shader_type != type && !is_tessellation_type(shader_type))
		return 0;
	if (es_shader) {
		if (is_tessellation_type(type) &&
		    required_glsl_version < 320 &&
		    (required_glsl_version < 310 ||
		     !piglit_is_extension_supported("GL_OES_tessellation_shader")))
			return 0;

		if (type == GL_GEOMETRY_SHADER &&
		    required_glsl_version < 320 &&
		    (required_glsl_version < 310 ||
		     !piglit_is_extension_supported("GL_OES_geometry_shader")))
			return 0;
	} else {
		if (is_tessellation_type(type) && (required_glsl_version < 150 ||
		    (required_glsl_version < 400 &&
		     !piglit_is_extension_supported("GL_ARB_tessellation_shader"))))
			return 0;

		/* Only support geometry shaders on desktop as introduced in
		 * OpenGL 3.2.
		 */
		if (type == GL_GEOMETRY_SHADER &&
		    required_glsl_version < 150)
			return 0;
	}
	/* Only create compute shaders when explicitly requested
	 */
	if (type == GL_COMPUTE_SHADER && shader_type != type)
		return 0;
	return glCreateShader(type);
}

void
piglit_init(int argc, char **argv)
{
	bool pass = true;
	char uniform[80];
	char *version_string = NULL;
	char *passthrough_version_string = NULL;
	unsigned i;

	const char *shader_source[3];

	GLuint test_vs;
	GLuint test_tcs;
	GLuint test_tes;
	GLuint test_gs;
	GLuint test_fs;
	GLuint test_cs;

	bool is_es;
	int major;
	int minor;
	int glsl_version;

	piglit_get_glsl_version(&is_es, &major, &minor);
	glsl_version = major * 100 + minor;

	if ((es_shader || required_glsl_version == 100) && !is_es) {
		switch (required_glsl_version) {
		case 100:
			if (!piglit_is_extension_supported("GL_ARB_ES2_compatibility"))
			    piglit_report_result(PIGLIT_SKIP);
			break;
		case 300:
			if (!piglit_is_extension_supported("GL_ARB_ES3_compatibility"))
			    piglit_report_result(PIGLIT_SKIP);
			break;
		case 310:
			if (!piglit_is_extension_supported("GL_ARB_ES3_1_compatibility"))
			    piglit_report_result(PIGLIT_SKIP);
			break;
		default:
			printf("Unknown GLSL ES version.\n");
			piglit_report_result(PIGLIT_FAIL);
		}
	} else if ((!es_shader && required_glsl_version != 100) && is_es) {
		/* It should actually be impossible to get here because
		 * supports_gl_es_version won't get set, and that is required
		 * in the ES builds.
		 */
		printf("Desktop OpenGL shaders are not valid in OpenGL ES.\n");
		piglit_report_result(PIGLIT_FAIL);
	} else if (glsl_version < required_glsl_version)
		piglit_report_result(PIGLIT_SKIP);

	/* Geometry shaders must use the #extension directive in GLSL ES
	 * before version 3.20.
	 */
	if (es_shader && required_glsl_version < 320 &&
	    required_glsl_version >= 310 &&
	    piglit_is_extension_supported("GL_OES_geometry_shader")) {
		assert(num_required_extensions < ARRAY_SIZE(required_extensions));
		required_extensions[num_required_extensions] =
			strdup("GL_OES_geometry_shader");
		num_required_extensions++;
	}

	/* Tessellation shaders must use the #extension directive. */
	const char *const tess_ext_name = es_shader
		? "GL_OES_tessellation_shader"
		: "GL_ARB_tessellation_shader";
	if (((es_shader && required_glsl_version >= 310) ||
	    !es_shader) &&
	    piglit_is_extension_supported(tess_ext_name)) {
		assert(num_required_extensions < ARRAY_SIZE(required_extensions));
		required_extensions[num_required_extensions] =
			strdup(tess_ext_name);
		num_required_extensions++;
	}

	/* Process the list of required extensions.  While doing this,
	 * generate the GLSL code that will enable those extensions in the
	 * shaders.
	 */
	for (i = 0; i < num_required_extensions; i++) {
		int len;

		if (!piglit_is_extension_supported(required_extensions[i])) {
			printf("%s not supported\n", required_extensions[i]);
			piglit_report_result(PIGLIT_SKIP);
		}

		if ((extension_enables_len + MAX_EXTENSION_ENABLE_LINE_LEN)
		    >= sizeof(extension_enables)) {
			printf("Extension enables too long.\n");
			piglit_report_result(PIGLIT_FAIL);
		}

		len = snprintf(&extension_enables[extension_enables_len],
			       MAX_EXTENSION_ENABLE_LINE_LEN,
			       "#extension %s: require\n",
			       required_extensions[i]);

		/* After the last use of the extension string, free it.
		 */
		free(required_extensions[i]);

		if (len <= 0) {
			printf("Extension enable snprintf failed.\n");
			piglit_report_result(PIGLIT_FAIL);
		}

		extension_enables_len += len;
	}

	/* Generate the version declaration that will be used by all of the
	 * shaders in the test run.
	 */
	(void)!asprintf(&version_string,
		 "#version %s\n"
		 "%s"
		 "#ifdef GL_ES\n"
		 "precision mediump float;\n"
		 "#endif\n",
		 required_glsl_version_string,
		 extension_enables);

	(void)!asprintf(&passthrough_version_string,
		 "#version %s\n"
		 "%s"
		 "#ifdef GL_ES\n"
		 "precision mediump float;\n"
		 "#endif\n",
		 required_glsl_version_string,
		 extension_enables);


	/* Create the shaders that will be used for the real part of the test.
	 */
	test_vs = create_shader(GL_VERTEX_SHADER);
	test_tcs = create_shader(GL_TESS_CONTROL_SHADER);
	test_tes = create_shader(GL_TESS_EVALUATION_SHADER);
	test_gs = create_shader(GL_GEOMETRY_SHADER);
	test_fs = create_shader(GL_FRAGMENT_SHADER);
	test_cs = create_shader(GL_COMPUTE_SHADER);

	for (i = 0; i < num_tests; i++) {
		bool subtest_pass = true;
		const char *comparitor =
			parse_str(tests[i].name, "gl_Min", NULL) ? "<=" : ">=";

		/* Generate the uniform declaration for the test.  This will
		 * be shared by all shader stages.
		 */
		snprintf(uniform, sizeof(uniform),
			 uniform_template,
			 tests[i].name, comparitor, tests[i].minimum);

		/* Try to compile the vertex shader.
		 */
		if (test_vs != 0) {
			if (!is_tessellation_type(shader_type)) {
				shader_source[0] = version_string;
				shader_source[1] = uniform;
			} else {
				shader_source[0] = passthrough_version_string;
				shader_source[1] = passthrough_uniform;
			}
			shader_source[2] = vertex_shader_body;

			glShaderSource(test_vs, 3, shader_source, NULL);
			glCompileShader(test_vs);

			subtest_pass = check_compile_status(tests[i].name, test_vs)
				&& subtest_pass;
		}

		/* Try to compile the tessellation control shader.
		 */
		if (test_tcs != 0) {
			shader_source[0] = version_string;
			shader_source[1] = uniform;
			shader_source[2] = tessellation_control_shader_body;

			glShaderSource(test_tcs, 3, shader_source, NULL);
			glCompileShader(test_tcs);

			subtest_pass = check_compile_status(tests[i].name, test_tcs)
				&& subtest_pass;
		}

		/* Try to compile the tessellation evaluation shader.
		 */
		if (test_tes != 0) {
			shader_source[0] = version_string;
			shader_source[1] = uniform;
			shader_source[2] = tessellation_evaluation_shader_body;

			glShaderSource(test_tes, 3, shader_source, NULL);
			glCompileShader(test_tes);

			subtest_pass = check_compile_status(tests[i].name, test_tes)
				&& subtest_pass;
		}

		/* Try to compile the geometry shader.
		 */
		if (test_gs != 0) {
			if (!is_tessellation_type(shader_type)) {
				shader_source[0] = version_string;
				shader_source[1] = uniform;
			} else {
				shader_source[0] = passthrough_version_string;
				shader_source[1] = passthrough_uniform;
			}
			shader_source[2] = geometry_shader_body;

			glShaderSource(test_gs, 3, shader_source, NULL);
			glCompileShader(test_gs);

			subtest_pass = check_compile_status(tests[i].name, test_gs)
				&& subtest_pass;
		}

		/* Try to compile the fragment shader.
		 */
		if (test_fs != 0) {
			if (!is_tessellation_type(shader_type)) {
				shader_source[0] = version_string;
				shader_source[1] = uniform;
			} else {
				shader_source[0] = passthrough_version_string;
				shader_source[1] = passthrough_uniform;
			}
			shader_source[2] = fragment_shader_body;

			glShaderSource(test_fs, 3, shader_source, NULL);
			glCompileShader(test_fs);

			subtest_pass = check_compile_status(tests[i].name, test_fs)
				&& subtest_pass;
		}

		/* Try to compile the compute shader.
		 */
		if (test_cs != 0) {
			shader_source[0] = version_string;
			shader_source[1] = uniform;
			shader_source[2] = compute_shader_body;

			glShaderSource(test_cs, 3, shader_source, NULL);
			glCompileShader(test_cs);

			subtest_pass = check_compile_status(tests[i].name, test_cs)
				&& subtest_pass;
		}

		/* If both compilation phases passed, try to link the shaders
		 * together.
		 */
		if (subtest_pass) {
			GLuint prog = glCreateProgram();

			if (test_vs != 0)
				glAttachShader(prog, test_vs);
			if (test_gs != 0)
				glAttachShader(prog, test_gs);
			if (test_fs != 0)
				glAttachShader(prog, test_fs);
			if (test_cs != 0)
				glAttachShader(prog, test_cs);

			glLinkProgram(prog);
			subtest_pass = !!piglit_link_check_status(prog);

			glDeleteProgram(prog);
		}

		piglit_report_subtest_result(subtest_pass ? PIGLIT_PASS : PIGLIT_FAIL,
					     "%s", tests[i].name);

		pass = subtest_pass && pass;
	}

	free(version_string);
	piglit_report_result(pass ? PIGLIT_PASS : PIGLIT_FAIL);
}
