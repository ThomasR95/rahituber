#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"


#include <fstream>
#include <iostream>

static uint32_t _ntohl(uint32_t const net) {
	uint8_t data[4] = {};
	memcpy(&data, &net, sizeof(data));

	return ((uint32_t)data[3] << 0)
		| ((uint32_t)data[2] << 8)
		| ((uint32_t)data[1] << 16)
		| ((uint32_t)data[0] << 24);
}

class TextureManager
{
public:

	sf::Texture* GetTexture(const std::string& path, std::string* errString = nullptr);

	void Reset();

private:
	std::map<std::string, sf::Texture*> _textures;

	sf::Vector2i GetDimensions(const char* path) 
	{
		std::ifstream in(path);
		unsigned int width, height;

		in.seekg(16);
		in.read((char*)&width, 4);
		in.read((char*)&height, 4);

		width = _ntohl(width);
		height = _ntohl(height);

		return sf::Vector2i(width, height);
	}
};