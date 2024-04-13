#include "TextureManager.h"

#include "file_browser_modal.h"

sf::Texture* TextureManager::GetTexture(const std::string& path, std::string* errString)
{
	sf::Texture* out = nullptr;
	if (path.empty())
		return nullptr;

	if (_textures.count(path))
	{
		out = _textures[path];
	}
	else
	{
		_textures[path] = new sf::Texture();
		int tries = 5;
		while (tries > 0)
		{
			bool success = false;
			std::string err = "";
			try
			{
				success = _textures[path]->loadFromFile(path);
			}
			catch (const std::exception& exc)
			{
				err = ": " + std::string(exc.what());
			}

			if (success)
			{
				out = _textures[path];
				break;
			}
			else
			{
				_textures.erase(path);
				if (errString)
				{
					fs::path fpath = path;
					*errString = "Failed to load " + fpath.filename().string() + ": ";
					if (fs::exists(path))
					{
						*errString += "Load error" + err;
						sf::Vector2i imgDim = GetDimensions(path.c_str());
						int maxDim = sf::Texture::getMaximumSize();
						if (imgDim.x > maxDim || imgDim.y > maxDim)
							*errString += " - Too large. Max: " + std::to_string(maxDim);
					}
					else
					{
						*errString += "File does not exist";
						break;
					}
				}
			}
			tries--;
		}
		
	}

	return out;
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