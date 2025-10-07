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


	void loadFromMemory(const std::string& vert, const std::string& frag, bool force = false)
	{
    if (_lastVert == vert && _lastFrag == frag && force == false)
      return;

		_shader->loadFromMemory(vert, frag);
    _lastVert = vert;
    _lastFrag = frag;
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
  std::string _lastVert = "";
  std::string _lastFrag = "";

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

void main()
{
    bool unmultiplyAlpha = !premult;

    vec2 texCoord = gl_TexCoord[0].xy;

    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, texCoord);

    if(pixel.a < alphaClip)
        pixel = vec4(0);

    if(unmultiplyAlpha)
    {
       pixel.xyz = pixel.xyz / pixel.a;
    }

    pixel = pixel * gl_Color;

    if(invert)
    {   
        pixel.xyz = pixel.xyz + (vec3(1.0, 1.0, 1.0) * (1.0 - pixel.a));
        //pixel.a = 1;
    }

    gl_FragColor = pixel;
}
)GLSL";

static const char* SFML_FXAAFrag = // fragment shader:
R"GLSL(
#version 130

uniform sampler2D texture;

uniform vec2 u_texelStep;
uniform int u_showEdges = 0;
uniform int u_fxaaOn = 1;

uniform float u_lumaThreshold = 0.125f;
uniform float u_mulReduce = 1.0f;
uniform float u_minReduce = 0.5f;
uniform float u_maxSpan = 4.0f;

void main()
{
    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
    float alpha = pixel.w;
    vec4 rgbM = pixel;

    // Possibility to toggle FXAA on and off.
    if (u_fxaaOn == 0)
    {
      gl_FragColor = vec4(rgbM);
    
      return;
    }

  // Sampling neighbour texels. Offsets are adapted to OpenGL texture coordinates. 
  vec4 rgbNW = textureOffset(texture, gl_TexCoord[0].xy, ivec2(-1, 1));
  vec4 rgbNE = textureOffset(texture, gl_TexCoord[0].xy, ivec2(1, 1));
  vec4 rgbSW = textureOffset(texture, gl_TexCoord[0].xy, ivec2(-1, -1));
  vec4 rgbSE = textureOffset(texture, gl_TexCoord[0].xy, ivec2(1, -1));

  // see http://en.wikipedia.org/wiki/Grayscale
  const vec3 toLuma = vec3(0.299, 0.587, 0.114);
  
  // Convert from RGB to luma.
  float lumaNW = dot(rgbNW.rgb, toLuma);
  float lumaNE = dot(rgbNE.rgb, toLuma);
  float lumaSW = dot(rgbSW.rgb, toLuma);
  float lumaSE = dot(rgbSE.rgb, toLuma);
  float lumaM = dot(rgbM.rgb, toLuma);

  // Gather minimum and maximum luma.
  float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

  float alphaMin = min(rgbM.w, min(min(rgbNW.w, rgbNE.w), min(rgbSW.w, rgbSE.w)));
  float alphaMax = max(rgbM.w, max(max(rgbNW.w, rgbNE.w), max(rgbSW.w, rgbSE.w)));
  
  // If contrast is lower than a maximum threshold ...
  if ((lumaMax - lumaMin <= lumaMax * u_lumaThreshold) && (alphaMax - alphaMin <= alphaMax * u_lumaThreshold))
  {
    // ... do no AA and return.
    gl_FragColor = vec4(rgbM);
    
    return;
  }  
  
  // Sampling is done along the gradient.
  vec2 samplingDirection;	
  samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
  samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
  // Sampling step distance depends on the luma: The brighter the sampled texels, the smaller the final sampling step direction.
  // This results, that brighter areas are less blurred/more sharper than dark areas.  
  float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * u_mulReduce, u_minReduce);

  // Factor for norming the sampling direction plus adding the brightness influence. 
  float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
    
  // Calculate final sampling direction vector by reducing, clamping to a range and finally adapting to the texture size. 
  samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-u_maxSpan), vec2(u_maxSpan)) * u_texelStep;
  
  // Inner samples on the tab.
  vec4 rgbSampleNeg = texture2D(texture, gl_TexCoord[0].xy + samplingDirection * (1.0/3.0 - 0.5));
  vec4 rgbSamplePos = texture2D(texture, gl_TexCoord[0].xy + samplingDirection * (2.0/3.0 - 0.5));

  vec4 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;  

  // Outer samples on the tab.
  vec4 rgbSampleNegOuter = texture2D(texture, gl_TexCoord[0].xy + samplingDirection * (0.0/3.0 - 0.5));
  vec4 rgbSamplePosOuter = texture2D(texture, gl_TexCoord[0].xy + samplingDirection * (3.0/3.0 - 0.5));
  
  vec4 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;   
  
  // Calculate luma for checking against the minimum and maximum value.
  float lumaFourTab = dot(rgbFourTab.rgb, toLuma);
  
  // Are outer samples of the tab beyond the edge ... 
  if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
  {
    // ... yes, so use only two samples.
    gl_FragColor = vec4(rgbTwoTab); 
  }
  else
  {
    // ... no, so use four samples. 
    gl_FragColor = vec4(rgbFourTab);
  }

  // Show edges for debug purposes.	
  if (u_showEdges != 0)
  {
    gl_FragColor.r = 1.0;
  }

}
)GLSL";