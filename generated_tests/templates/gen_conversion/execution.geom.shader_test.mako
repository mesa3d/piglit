## coding=utf-8
<%inherit file="execution_base.mako"/>\

[vertex shader]
#version 150

in vec4 piglit_vertex;
out vec4 vertex_to_gs;

void main()
{
	vertex_to_gs = piglit_vertex;
}

[geometry shader]
<%include file="shader.geom.mako"/>
[fragment shader]
#version 150

in vec4 fs_color;
out vec4 color;

void main()
{
	color = fs_color;
}

