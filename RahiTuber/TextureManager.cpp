#include "TextureManager.h"

#include "file_browser_modal.h"
#include <thread>

void TextureManager::LoadIcons(const std::string& appLocation)
{
	if (_icons.count(ICON_ANIM) == 0)
		LoadIcon(appLocation + "res/anim.png", _icons[ICON_ANIM]);
	if (_icons.count(ICON_EMPTY) == 0)
		LoadIcon(appLocation + "res/empty.png", _icons[ICON_EMPTY]);
	if (_icons.count(ICON_UP) == 0)
		LoadIcon(appLocation + "res/arrowup.png", _icons[ICON_UP]);
	if (_icons.count(ICON_DN) == 0)
		LoadIcon(appLocation + "res/arrowdn.png", _icons[ICON_DN]);
	if (_icons.count(ICON_EDIT) == 0)
		LoadIcon(appLocation + "res/edit.png", _icons[ICON_EDIT]);
	if (_icons.count(ICON_DEL) == 0)
		LoadIcon(appLocation + "res/delete.png", _icons[ICON_DEL]);
	if (_icons.count(ICON_DUPE) == 0)
		LoadIcon(appLocation + "res/duplicate.png", _icons[ICON_DUPE]);
	if (_icons.count(ICON_NEWFILE) == 0)
		LoadIcon(appLocation + "res/new_file.png", _icons[ICON_NEWFILE]);
	if (_icons.count(ICON_OPEN) == 0)
		LoadIcon(appLocation + "res/open.png", _icons[ICON_OPEN]);
	if (_icons.count(ICON_SAVE) == 0)
		LoadIcon(appLocation + "res/save.png", _icons[ICON_SAVE]);
	if (_icons.count(ICON_SAVEAS) == 0)
		LoadIcon(appLocation + "res/save_as.png", _icons[ICON_SAVEAS]);
	if (_icons.count(ICON_MAKEPORTABLE) == 0)
		LoadIcon(appLocation + "res/make_portable.png", _icons[ICON_MAKEPORTABLE]);
	if (_icons.count(ICON_RELOAD) == 0)
		LoadIcon(appLocation + "res/reload.png", _icons[ICON_RELOAD]);
	if (_icons.count(ICON_NEWLAYER) == 0)
		LoadIcon(appLocation + "res/new_layer.png", _icons[ICON_NEWLAYER]);
	if (_icons.count(ICON_NEWFOLDER) == 0)
		LoadIcon(appLocation + "res/new_folder.png", _icons[ICON_NEWFOLDER]);
	if (_icons.count(ICON_STATES) == 0)
		LoadIcon(appLocation + "res/states.png", _icons[ICON_STATES]);
	if (_icons.count(ICON_RESET) == 0)
		LoadIcon(appLocation + "res/reset.png", _icons[ICON_RESET]);
	if (_icons.count(ICON_PLUS) == 0)
		LoadIcon(appLocation + "res/plus.png", _icons[ICON_PLUS]);
	if (_icons.count(ICON_LOCK_OPEN) == 0)
		LoadIcon(appLocation + "res/lock_open.png", _icons[ICON_LOCK_OPEN]);
	if (_icons.count(ICON_LOCK_CLOSED) == 0)
		LoadIcon(appLocation + "res/lock_closed.png", _icons[ICON_LOCK_CLOSED]);
	if (_icons.count(ICON_EYE_OPEN) == 0)
		LoadIcon(appLocation + "res/eye_open.png", _icons[ICON_EYE_OPEN]);
	if (_icons.count(ICON_EYE_CLOSED) == 0)
		LoadIcon(appLocation + "res/eye_closed.png", _icons[ICON_EYE_CLOSED]);
	if (_icons.count(ICON_PIN) == 0)
		LoadIcon(appLocation + "res/pin.png", _icons[ICON_PIN]);
	if (_icons.count(ICON_PIN_OFF) == 0)
		LoadIcon(appLocation + "res/pin_off.png", _icons[ICON_PIN_OFF]);

	for (auto& ic : _icons)
		ic.second->setSmooth(true);
}

sf::Texture* TextureManager::GetTexture(const std::string& path, void* caller, std::string* errString)
{
	if(errString != nullptr)
		*errString = "";

	if (path.empty())
		return nullptr;

	sf::Texture* out = nullptr;

	while (_textures.count(path) && _textures[path].busyLoading)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	
	if ( _textures[path].tex != nullptr)
	{
		_textures[path].refHolders[caller] = true;
		out = _textures[path].tex.get();
	}
	else
	{
		_textures[path].busyLoading = true;

		bool success = LoadTexture(path, caller, errString);
		if (success)
			out = _textures[path].tex.get();
		else
			_textures.erase(path);
	}

	return out;
}

bool TextureManager::LoadIcon(const std::string& path, sf::Texture*& storage)
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

		tries--;
	}

	return false;
}

bool TextureManager::LoadTexture(const std::string& path, void* caller, std::string* errString)
{
	auto loadingTex = std::make_unique<sf::Texture>();
	int tries = 5;
	while (tries > 0)
	{
		bool success = false;
		std::string err = "";
		try
		{
			success = loadingTex->loadFromFile(path);

			if (success)
			{
				std::scoped_lock loadLock(_loadMutex);
				_textures[path].refHolders[caller] = true;
				_textures[path].tex = std::move(loadingTex);
			}
		}
		catch (const std::exception& exc)
		{
			err = ": " + std::string(exc.what());
		}

		if (success)
		{
			_textures[path].busyLoading = false;
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

void TextureManager::UnloadTexture(const std::string& path, void* caller)
{
	if (_textures.count(path) != 0)
	{
		if (_textures[path].refHolders.size() <= 1)
		{
			_textures[path].tex = nullptr;
			_textures.erase(path);
		}
		else if (_textures[path].refHolders.count(caller))
		{
			_textures[path].refHolders.erase(caller);
		}
	}
}

void TextureManager::Reset()
{
	std::scoped_lock loadLock(_loadMutex);
	auto it = _textures.begin();
	for (; it != _textures.end(); it++)
	{
		(*it).second.refHolders.clear();
		(*it).second.tex = nullptr;
	}
	_textures.clear();
}

sf::Texture* TextureManager::GetIcon(IconID id)
{
	if (_icons.count(id))
		return _icons[id];

	return nullptr;
}
