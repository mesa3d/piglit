// [config]
// expect_result: fail
// glsl_version: 3.00
// check_link: true
// [end config]
//
// From section 4.5.4 ("Default Precision Qualifiers") of the GLSL ES
// 3.00 spec:
//
//     "The fragment language has no default precision qualifier for
//     floating point types. Hence for float, floating point vector
//     and matrix variable declarations, either the declaration must
//     include a precision qualifier or the default float precision
//     must have been previously declared."

#version 300 es

void main()
{
	float f = 1.0;
}
