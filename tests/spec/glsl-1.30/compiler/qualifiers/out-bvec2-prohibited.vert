// [config]
// expect_result: fail
// glsl_version: 1.30
// [end config]
//
// Check that a bvec2 can't be used as a varying in GLSL 1.30.
//
// From section 4.3.6 ("Outputs") of the GLSL 1.30 spec:
//     Vertex output variables ... can only be float, floating-point
//     vectors, matrices, signed or unsigned integers or integer
//     vectors, or arrays of any these. ... Structures cannot be
//     output.

#version 130

flat out bvec2 foo;

void main()
{
  gl_Position = vec4(0.0);
  foo = bvec2(true, false);
}
