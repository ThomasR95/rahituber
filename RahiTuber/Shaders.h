#pragma once

#include <map>
#include <variant>
#include "SFML/Graphics.hpp"


template <typename T>
inline bool operator ==(const sf::priv::Vector4<T>& left, const sf::priv::Vector4<T>& right) {
  return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
}

inline bool operator ==(const sf::priv::Matrix<3, 3>& left, const sf::priv::Matrix<3, 3>& right) {
  for (int i = 0; i < 9; i++)
  {
    if (left.array[i] != right.array[i])
      return false;
  }
}

inline bool operator ==(const sf::priv::Matrix<4, 4>& left, const sf::priv::Matrix<4, 4>& right) {
  for (int i = 0; i < 16; i++)
  {
    if (left.array[i] != right.array[i])
      return false;
  }
} 


class Shader
{
public:

  Shader() 
  {
    _shader = std::make_shared<sf::Shader>();
  };

   typedef std::variant <
     int, bool, float,
      sf::Glsl::Bvec2, sf::Glsl::Bvec3, //sf::Glsl::Bvec4,
      sf::Glsl::Ivec2, sf::Glsl::Ivec3, //sf::Glsl::Ivec4,
      sf::Glsl::Vec2, sf::Glsl::Vec3//, sf::Glsl::Vec4,
      //sf::Glsl::Mat3,
      //sf::Glsl::Mat4
   > Uniform;


  void loadFromMemory(const std::string& vert, const std::string& frag)
  {
    _shader->loadFromMemory(vert, frag);
  }

  void setUniform(const std::string& key, Uniform value)
  {
    if (_uniforms.count(key))
    {
      if (_uniforms[key] == value)
        return;
    }

    _uniforms[key] = value;
    std::visit([&](auto&& arg) { _shader->setUniform(key, arg); }, value);
  }

  sf::Shader* get() { return _shader.get(); }

private:
  std::map<std::string, Uniform> _uniforms;

  std::shared_ptr<sf::Shader> _shader;

};

static const char* SFML_DefaultVert = // vertex shader:
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

static const char* SFML_DefaultFrag = // fragment shader:
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

static const char* SFML_PremultFrag = // fragment shader:
R"GLSL(
#version 130

uniform sampler2D texture;

uniform bool invert = false;
uniform bool premult = true;
uniform bool sharpEdge = true;
uniform float alphaClip = 0.001;

int isTransparentEdge(vec2 texCoord, vec4 origPixel, out vec4 opaquePixel)
{
    vec2 texSize = textureSize(texture,0);

    if(alphaClip <= 0.0)
      return 0;

    vec2 invSize = vec2(1.0/texSize.x, 1.0/texSize.y);

    vec2 pixelCoord = texCoord * texSize;
    vec2 sharpPixelCoord = vec2(floor(pixelCoord.x) + 0.5, floor(pixelCoord.y) + 0.5);

    vec4 sharpPixel = texture2D(texture, invSize * sharpPixelCoord);

    if(length(origPixel - sharpPixel) < 0.05)
      return 0;
    
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
      sharpDiff.x = sharpDiff.x / abs(sharpDiff.x);
      sharpDiff.y = sharpDiff.y / abs(sharpDiff.y);

      vec2 nextPixX = vec2(sharpPixelCoord.x + sharpDiff.x, sharpPixelCoord.y);
      vec2 nextPixY = vec2(sharpPixelCoord.x, sharpPixelCoord.y + sharpDiff.y);
      vec2 nextPixXY = vec2(sharpPixelCoord.x + sharpDiff.x, sharpPixelCoord.y + sharpDiff.y);

      vec4 sharpPixelX = texture2D(texture, invSize * nextPixX);
      if(transparent && sharpPixelX.a > 0.8)
      {
        opaquePixel = sharpPixelX;
        return 1; // transparent and the adjacent pixel is solid
      }
      else if( opaque && sharpPixelX.a < max(alphaClip, 0.1)) 
        return 2; // opaque and the adjacent pixel is transparent

      vec4 sharpPixelY = texture2D(texture, invSize * nextPixY);
      if(transparent && sharpPixelY.a > 0.8)
      {
        opaquePixel = sharpPixelY;
        return 1; // transparent and the adjacent pixel is solid
      }
      else if(opaque && sharpPixelY.a < max(alphaClip, 0.1)) 
        return 2; // opaque and the adjacent pixel is transparent

      vec4 sharpPixelXY = texture2D(texture, invSize * nextPixXY);
      if(transparent && sharpPixelXY.a > 0.8)
      {
        opaquePixel = sharpPixelXY;
        return 1; // transparent and the adjacent pixel is solid
      }
      else if(opaque && sharpPixelXY.a < max(alphaClip, 0.1))
        return 2; // opaque and the adjacent pixel is transparent

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
      int sharp = isTransparentEdge(texCoord, pixel, opaquePixel);

      if(pixel.a < alphaClip)
        pixel.a = 0;

      pixel = gl_Color * pixel;

      if(sharp == 1) // transparent and the adjacent pixel is solid
      {
        if(pixel.a < max(alphaClip, 0.6))
          pixel.a = 0;

        pixel.xyz = gl_Color.xyz * opaquePixel.xyz;
      }

      if(sharp == 2) // opaque and the adjacent pixel is transparent
      { 
        if(pixel.a > max(alphaClip, 0.6))
          pixel = gl_Color * opaquePixel;
        else
          pixel.a = 0;
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

