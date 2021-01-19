#version 460

in vec4 vertexColor;
in vec4 position;
out vec4 FragColor;

uniform sampler2D texture1;

uniform float x_offset;
uniform float y_offset;
uniform float x_scale;
uniform float y_scale;

uniform uint screen_height;
uniform uint screen_width;

void main()
{
    vec2 normalizedCoordinate = gl_FragCoord.xy / vec2(screen_width, screen_height); // Create normalized UV coordinates on screen
    normalizedCoordinate.y /= screen_width / screen_height; // Normalize aspect ratio
    normalizedCoordinate /= vec2(x_scale, y_scale);
    normalizedCoordinate += vec2(x_offset, y_offset);
    FragColor = texture(texture1, normalizedCoordinate);
}