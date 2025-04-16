#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

#ifndef _WIN32
typedef  __uint32_t uint32_t;
typedef  __uint8_t uint8_t;
#endif

static uint32_t _ntohl(uint32_t const net) {
	uint8_t data[4] = {};
    std::memcpy(&data, &net, sizeof(data));

	return ((uint32_t)data[3] << 0)
		| ((uint32_t)data[2] << 8)
		| ((uint32_t)data[1] << 16)
		| ((uint32_t)data[0] << 24);
}

class TextureManager
{
public:

	enum IconID {
		ICON_EMPTY,
		ICON_ANIM,
		ICON_UP,
		ICON_DN,
		ICON_EDIT,
		ICON_DEL,
		ICON_DUPE,
		ICON_NEWFILE,
		ICON_OPEN,
		ICON_SAVE,
		ICON_SAVEAS,
		ICON_MAKEPORTABLE,
		ICON_RELOAD,
		ICON_NEWLAYER,
		ICON_NEWFOLDER,
		ICON_STATES,
		ICON_RESET,
		ICON_PLUS,
		ICON_LOCK_OPEN,
		ICON_LOCK_CLOSED,
		ICON_EYE_OPEN,
		ICON_EYE_CLOSED,
		ICON_PIN,
		ICON_PIN_OFF,
	};

	void LoadIcons(const std::string& appLocation);

	sf::Texture* GetTexture(const std::string& path, void* caller, std::string* errString = nullptr);


	bool LoadTexture(const std::string& path, void* caller, std::string* errString = nullptr);
	bool LoadIcon(const std::string& path, sf::Texture*& storage);

	void UnloadTexture(const std::string& path, void* caller);

	void Reset();

	sf::Texture* GetIcon(IconID id);

private:

	struct TextureItem {
		std::unique_ptr<sf::Texture> tex;
		std::map<void*, bool> refHolders;
		bool busyLoading = false;
	};

	std::map<std::string, TextureItem> _textures;

	std::map<IconID, sf::Texture*> _icons;

	std::mutex _loadMutex;

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
