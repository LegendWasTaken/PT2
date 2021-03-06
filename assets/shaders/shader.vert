#version 460

layout(location = 0) in vec3 aPos;

out vec4 vertexColor;
out vec4 position;

void main ()
{
    gl_Position = vec4(aPos, 1.0);
    position = vec4(aPos, 1.0);
    vertexColor = vec4(0, 0, 0, 0);
}