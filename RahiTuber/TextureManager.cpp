#include "TextureManager.h"

#include "file_browser_modal.h"


void TextureManager::LoadIcons(const std::string& appLocation)
{
	if (_icons.count(ICON_ANIM) == 0)
		LoadTexture(appLocation + "res/anim.png", _icons[ICON_ANIM]);
	if (_icons.count(ICON_EMPTY) == 0)
		LoadTexture(appLocation + "res/empty.png", _icons[ICON_EMPTY]);
	if (_icons.count(ICON_UP) == 0)
		LoadTexture(appLocation + "res/arrowup.png", _icons[ICON_UP]);
	if (_icons.count(ICON_DN) == 0)
		LoadTexture(appLocation + "res/arrowdn.png", _icons[ICON_DN]);
	if (_icons.count(ICON_EDIT) == 0)
		LoadTexture(appLocation + "res/edit.png", _icons[ICON_EDIT]);
	if (_icons.count(ICON_DEL) == 0)
		LoadTexture(appLocation + "res/delete.png", _icons[ICON_DEL]);
	if (_icons.count(ICON_DUPE) == 0)
		LoadTexture(appLocation + "res/duplicate.png", _icons[ICON_DUPE]);
	if (_icons.count(ICON_NEWFILE) == 0)
		LoadTexture(appLocation + "res/new_file.png", _icons[ICON_NEWFILE]);
	if (_icons.count(ICON_OPEN) == 0)
		LoadTexture(appLocation + "res/open.png", _icons[ICON_OPEN]);
	if (_icons.count(ICON_SAVE) == 0)
		LoadTexture(appLocation + "res/save.png", _icons[ICON_SAVE]);
	if (_icons.count(ICON_SAVEAS) == 0)
		LoadTexture(appLocation + "res/save_as.png", _icons[ICON_SAVEAS]);
	if (_icons.count(ICON_MAKEPORTABLE) == 0)
		LoadTexture(appLocation + "res/make_portable.png", _icons[ICON_MAKEPORTABLE]);
	if (_icons.count(ICON_RELOAD) == 0)
		LoadTexture(appLocation + "res/reload.png", _icons[ICON_RELOAD]);
	if (_icons.count(ICON_NEWLAYER) == 0)
		LoadTexture(appLocation + "res/new_layer.png", _icons[ICON_NEWLAYER]);
	if (_icons.count(ICON_NEWFOLDER) == 0)
		LoadTexture(appLocation + "res/new_folder.png", _icons[ICON_NEWFOLDER]);
	if (_icons.count(ICON_STATES) == 0)
		LoadTexture(appLocation + "res/states.png", _icons[ICON_STATES]);
	if (_icons.count(ICON_RESET) == 0)
		LoadTexture(appLocation + "res/reset.png", _icons[ICON_RESET]);
	if (_icons.count(ICON_PLUS) == 0)
		LoadTexture(appLocation + "res/plus.png", _icons[ICON_PLUS]);
	if (_icons.count(ICON_LOCK_OPEN) == 0)
		LoadTexture(appLocation + "res/lock_open.png", _icons[ICON_LOCK_OPEN]);
	if (_icons.count(ICON_LOCK_CLOSED) == 0)
		LoadTexture(appLocation + "res/lock_closed.png", _icons[ICON_LOCK_CLOSED]);
	if (_icons.count(ICON_EYE_OPEN) == 0)
		LoadTexture(appLocation + "res/eye_open.png", _icons[ICON_EYE_OPEN]);
	if (_icons.count(ICON_EYE_CLOSED) == 0)
		LoadTexture(appLocation + "res/eye_closed.png", _icons[ICON_EYE_CLOSED]);

	for (auto& ic : _icons)
		ic.second->setSmooth(true);
}

sf::Texture* TextureManager::GetTexture(const std::string& path, std::string* errString)
{
	if(errString != nullptr)
		*errString = "";

	if (path.empty())
		return nullptr;

	sf::Texture* out = nullptr;

	if (_textures.count(path))
	{
		out = _textures[path];
	}
	else
	{
		bool success = LoadTexture(path, _textures[path], errString);
		if (!success)
			_textures.erase(path);
		else
			out = _textures[path];
	}

	return out;
}

bool TextureManager::LoadTexture(const std::string& path, sf::Texture*& storage, std::string* errString)
{
	storage = new sf::Texture();
	int tries = 5;
	while (tries > 0)
	{
		bool success = false;
		std::string err = "";
		try
		{
			success = storage->loadFromFile(path);
		}
		catch (const std::exception& exc)
		{
			err = ": " + std::string(exc.what());
		}

		if (success)
		{
			return true;
		}
		else
		{
			if (errString)
			{
				fs::path fpath = path;
				*errString = "Failed to load " + fpath.filename().string() + ": ";
				std::error_code ec;
				if (fs::exists(path, ec))
				{
					*errString += "Load error" + err;
					sf::Vector2i imgDim = GetDimensions(path.c_str());
					int maxDim = sf::Texture::getMaximumSize();
					if (imgDim.x > maxDim || imgDim.y > maxDim)
					{
						*errString += " - Too large. Max: " + std::to_string(maxDim);
						return false;
					}
				}
				else
				{
					*errString += "File does not exist";
					return false;
				}
			}
			
		}
		tries--;
	}

	return false;
}

void TextureManager::Reset()
{
	auto it = _textures.begin();
	for (; it != _textures.end(); it++)
	{
		delete (*it).second;
		(*it).second = nullptr;
	}
	_textures.clear();
}

sf::Texture* TextureManager::GetIcon(IconID id)
{
	if (_icons.count(id))
		return _icons[id];

	return nullptr;
}
