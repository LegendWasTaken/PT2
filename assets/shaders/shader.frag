#version 460

in vec4 vertexColor;
in vec4 position;
out vec4 FragColor;

uniform sampler2D texture1;

void main()
{
//    FragColor = texture(texture1, vec2(0.1, 0.5));
//    FragColor = position + vec4(1, 1, 2, 0);
    FragColor = texture(texture1, vec2(position.xy + vec2(.5, .5)));
}