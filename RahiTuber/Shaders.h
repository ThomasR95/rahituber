const char* SFML_DefaultVert = // vertex shader:
R"GLSL(
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

const char* SFML_DefaultFrag = // fragment shader:
R"GLSL(
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

const char* SFML_PremultFrag = // fragment shader:
R"GLSL(
#version 130

uniform sampler2D texture;

uniform bool invert = false;
uniform vec2 texSize = vec2(0,0);
uniform bool premult = true;
uniform bool sharpEdge = true;
uniform float alphaClip = 0.001;

int isTransparentEdge(vec2 texCoord, out vec4 opaquePixel)
{
    if(alphaClip <= 0.0)
      return 0;

    vec2 invSize = vec2(1.0/texSize.x, 1.0/texSize.y);

    vec2 pixelCoord = texCoord * texSize;
    vec2 sharpPixelCoord = vec2(floor(pixelCoord.x) + 0.5, floor(pixelCoord.y) + 0.5);

    vec4 sharpPixel = texture2D(texture, invSize * sharpPixelCoord);
    
    bool sharp = false;
    bool transparent = false;
    if(sharpPixel.a < alphaClip)
      transparent = true;

    bool opaque = false;
    if(sharpPixel.a >= 0.96)
    { 
      opaque = true;
      opaquePixel = sharpPixel;
    }


    if(opaque || transparent)
    {
      // Test if this is an edge pixel
      vec2 sharpDiff = pixelCoord - sharpPixelCoord;
      sharpDiff.x = 1.1 * sign(sharpDiff.x) * ceil(clamp(abs(sharpDiff.x), 0, 1));
      sharpDiff.y = 1.1 * sign(sharpDiff.y) * ceil(clamp(abs(sharpDiff.y), 0, 1));

      vec2 nextPixX = vec2(sharpPixelCoord.x + sharpDiff.x, sharpPixelCoord.y);
      vec2 nextPixY = vec2(sharpPixelCoord.x, sharpPixelCoord.y + sharpDiff.y);
      vec2 nextPixXY = vec2(sharpPixelCoord.x + sharpDiff.x, sharpPixelCoord.y + sharpDiff.y);

      vec4 sharpPixelX = texture2D(texture, invSize * nextPixX);
      if(transparent && sharpPixelX.a > 0.6 || opaque && sharpPixelX.a < max(alphaClip, 0.4))
        if(transparent){
          opaquePixel = sharpPixelX;
          return 1;
        }
        else if(opaque) return 2;

      vec4 sharpPixelY = texture2D(texture, invSize * nextPixY);
      if(transparent && sharpPixelY.a > 0.6 || opaque && sharpPixelY.a < max(alphaClip, 0.4))
        if(transparent){
          opaquePixel = sharpPixelX;
          return 1;
        }
        else if(opaque) return 2;

      vec4 sharpPixelXY = texture2D(texture, invSize * nextPixXY);
      if(transparent && sharpPixelXY.a > 0.6 || opaque && sharpPixelXY.a < max(alphaClip, 0.4))
        if(transparent){
          opaquePixel = sharpPixelX;
          return 1;
        }
        else if(opaque) return 2;

    }

    return 0;
}


void main()
{
    bool multiplyAlpha = premult;

    vec2 texCoord = gl_TexCoord[0].xy;

    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, texCoord);

    if(sharpEdge)
    {
      vec4 opaquePixel;
      int sharp = isTransparentEdge(texCoord, opaquePixel);

      // trim to keep hard edge
      if(sharp > 0 && pixel.a < 0.6)
        pixel.a = 0;

      if(pixel.a < alphaClip)
        pixel.a = 0;

      pixel = gl_Color * pixel;

      if(sharp == 1)
      {
        pixel.xyz = opaquePixel.xyz;
      }

      if(sharp == 2)
      { 
        
        if(pixel.a > 0.6)
          pixel = opaquePixel;
      }
    }
    else
    {
      if(pixel.a < alphaClip)
              pixel.a = 0;

      pixel = gl_Color * pixel;
    }

    if(multiplyAlpha)
    {
        if(invert)
        {   
            pixel.xyz = pixel.xyz + vec3(1.0, 1.0, 1.0) * (1.0 - pixel.a);
        }
        else
        {
            pixel.xyz = pixel.xyz * pixel.a;
        }
    }

    gl_FragColor = pixel;
}
)GLSL";



#pragma once
