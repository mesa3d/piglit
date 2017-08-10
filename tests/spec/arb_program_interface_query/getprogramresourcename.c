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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * \file getprogramresourcename.c
 *
 * Tests the error cases of the GetProgramResourceName interface. The real
 * functional test is resource-query.
 *
 * From the GL_ARB_program_interface_query spec:
 *      "The command
 *
 *      void GetProgramResourceName(uint program, enum programInterface,
 *                                  uint index, sizei bufSize, sizei *length,
 *                                  char *name);
 *
 *      returns the name string assigned to the single active resource with an
 *      index of <index> in the interface <programInterface> of program object
 *      <program>.  The error INVALID_VALUE is generated if <index> is greater
 *      than or equal to the number of entries in the active resource list for
 *      <programInterface>.  The error INVALID_ENUM is generated if
 *      <programInterface> is ATOMIC_COUNTER_BUFFER, since active atomic counter
 *      buffer resources are not assigned name strings.
 *
 *      The name string assigned to the active resource identified by <index> is
 *      returned as a null-terminated string in <name>.  The actual number of
 *      characters written into <name>, excluding the null terminator, is
 *      returned in <length>.  If <length> is NULL, no length is returned. The
 *      maximum number of characters that may be written into <name>, including
 *      the null terminator, is specified by <bufSize>.  If the length of the
 *      name string (including the null terminator) is greater than <bufSize>,
 *      the first <bufSize>-1 characters of the name string will be written to
 *      <name>, followed by a null terminator.  If <bufSize> is zero, no error
 *      will be generated but no characters will be written to <name>.  The
 *      length of the longest name string for <programInterface>, including a
 *      null terminator, can be queried by calling GetProgramInterfaceiv with a
 *      <pname> of MAX_NAME_LENGTH.
 *
 *      [...]
 *
 *      An INVALID_VALUE error is generated by GetProgramInterfaceiv,
 *      GetProgramResourceIndex, GetProgramResourceName, GetProgramResourceiv,
 *      GetProgramResourceLocation, and GetProgramResourceLocationIndex if
 *      <program> is not the name of either a shader or program object.
 *
 *      An INVALID_OPERATION error is generated by GetProgramInterfaceiv,
 *      GetProgramResourceIndex, GetProgramResourceName, GetProgramResourceiv,
 *      GetProgramResourceLocation, and GetProgramResourceLocationIndex if
 *      <program> is the name of a shader object.
 *
 *      [...]
 *
 *      INVALID_VALUE is generated by GetProgramResourceName if <index> is
 *      greater than or equal to the number of entries in the active resource
 *      list for <programInterface>.
 *
 *      INVALID_ENUM is generated by GetProgramResourceName if
 *      <programInterface> is ATOMIC_COUNTER_BUFFER."
 */

#include "piglit-util-gl.h"
#include "common.h"

PIGLIT_GL_TEST_CONFIG_BEGIN

	config.supports_gl_core_version = 32;
	config.khr_no_error_support = PIGLIT_HAS_ERRORS;

PIGLIT_GL_TEST_CONFIG_END

void
piglit_init(int argc, char **argv)
{
	piglit_require_extension("GL_ARB_program_interface_query");
}

enum piglit_result
piglit_display(void)
{
	GLsizei length;
	bool pass = true, local;
	GLuint shader, prog;
	GLchar name[100];
	GLint pos;

	/* test using an unexisting program ID */
	glGetProgramResourceName(1337, GL_UNIFORM, 0, 100, &length, name);
	local = piglit_check_gl_error(GL_INVALID_VALUE);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "Invalid program (undefined ID)");

	/* test using a shader ID */
	shader = piglit_compile_shader_text(GL_VERTEX_SHADER, vs_empty);
	glGetProgramResourceName(shader, GL_UNIFORM, 0, 100, &length, name);
	local = piglit_check_gl_error(GL_INVALID_OPERATION);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "Invalid program (call on shader)");

	prog = piglit_build_simple_program(vs_array, NULL);
	if (!piglit_link_check_status(prog)) {
		glDeleteProgram(prog);
		return PIGLIT_FAIL;
	}

	glGetProgramResourceName(prog, GL_TRUE, 0, 100, &length, name);
	local = piglit_check_gl_error(GL_INVALID_ENUM);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "invalid programInterface");

	glGetProgramResourceName(prog, GL_UNIFORM, -1, 100, &length, name);
	local = piglit_check_gl_error(GL_INVALID_VALUE);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "idx < 0");

	glGetProgramResourceName(prog, GL_UNIFORM, 1337, 100, &length, name);
	local = piglit_check_gl_error(GL_INVALID_VALUE);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "idx > #entries");

	pos = glGetProgramResourceIndex(prog, GL_UNIFORM, "sa[0].world");

	glGetProgramResourceName(prog, GL_UNIFORM, pos, -1, &length, name);
	local = piglit_check_gl_error(GL_INVALID_VALUE);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "size < 0");

	glGetProgramResourceName(prog, GL_UNIFORM, pos, 0, &length, name);
	local = piglit_check_gl_error(GL_NO_ERROR);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "size == 0");

	glGetProgramResourceName(prog, GL_UNIFORM, pos, 0, &length, NULL);
	local = piglit_check_gl_error(GL_NO_ERROR);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "NULL name");

	if (piglit_is_extension_supported("GL_ARB_shader_atomic_counters")) {
		glGetProgramResourceName(prog, GL_ATOMIC_COUNTER_BUFFER, 0,
					 100, &length, name);
		local = piglit_check_gl_error(GL_INVALID_ENUM);
		pass = pass && local;
		piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
					     "GL_ATOMIC_COUNTER_BUFFER");
	} else {
		piglit_report_subtest_result(PIGLIT_SKIP,
					     "GL_ATOMIC_COUNTER_BUFFER");
	}

	glGetProgramResourceName(prog, GL_UNIFORM, pos, 0, NULL, name);
	local = piglit_check_gl_error(GL_NO_ERROR);
	pass = pass && local;
	piglit_report_subtest_result(local ? PIGLIT_PASS : PIGLIT_FAIL,
				     "length == NULL");

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}
