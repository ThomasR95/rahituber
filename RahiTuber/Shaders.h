const char* SFML_DefaultVert = R"GLSL(
#version 130

void main()
{
    // transform the vertex position
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    // transform the texture coordinates
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

    // forward the vertex color
    gl_FrontColor = gl_Color;
}
)GLSL";

const char* SFML_DefaultFrag = R"GLSL(
#version 130

uniform sampler2D texture;

void main()
{
    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

    // multiply it by the color
    gl_FragColor = gl_Color * pixel;
}
)GLSL";

const char* SFML_PremultFrag = R"GLSL(
#version 130

uniform sampler2D texture;

uniform bool invert = false;

void main()
{
    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

    pixel = gl_Color * pixel;

    if(invert)
    {   
        pixel.xyz = pixel.xyz + vec3(1.0, 1.0, 1.0) * (1.0 - pixel.a);
    }
    else
    {
        pixel.xyz = pixel.xyz * pixel.a;
    }

    gl_FragColor = pixel;
}
)GLSL";



#pragma once
