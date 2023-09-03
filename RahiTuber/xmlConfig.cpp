#include "xmlConfig.h"

using namespace tinyxml2;

xmlConfigLoader::xmlConfigLoader(AppConfig* appConfig, UIConfig* uiConfig, AudioConfig* audioConfig, const std::string& settingsXMLFile) :
	_appConfig(appConfig),
	_audioConfig(audioConfig),
	_uiConfig(uiConfig),
	_settingsFileName(settingsXMLFile)
{
}


xmlConfigLoader::~xmlConfigLoader()
{
}

bool xmlConfigLoader::loadCommon()
{
	_errorMessage = "";
	XMLDocument doc;

	doc.LoadFile(_settingsFileName.c_str());

	if (doc.Error())
	{
		if (doc.ErrorID() == XML_ERROR_FILE_NOT_FOUND)
		{
			saveCommon();
			return true;
		}
		else
		{
			_errorMessage = "Failed to open document: " + _settingsFileName;
			return false;
		}
	}

	auto root = doc.FirstChildElement("Config");
	if (!root)
	{
		_errorMessage = "Could not find config element \"Config\": " + _settingsFileName;
		return false;
	}

	auto common = root->FirstChildElement("Common");
	if (!common)
	{
		_errorMessage = "Could not find config element \"Common\": " + _settingsFileName;
		return false;
	}

	common->QueryBoolAttribute("startMaximised", &_appConfig->_startMaximised);
	common->QueryFloatAttribute("lastWidth", &_appConfig->_minScrW);
	common->QueryFloatAttribute("lastHeight", &_appConfig->_minScrH);
	common->QueryIntAttribute("lastX", &_appConfig->_scrX);
	common->QueryIntAttribute("lastY", &_appConfig->_scrY);
	common->QueryBoolAttribute("alwaysOnTop", &_appConfig->_alwaysOnTop);
	common->QueryIntAttribute("lastAudioDevice", &_audioConfig->_devIdx);
	common->QueryBoolAttribute("useKeyboardHook", &_appConfig->_useKeyboardHooks);

	int r = -1;
	int g = -1; 
	int b = -1;
	common->QueryIntAttribute("lastBgCol_r", &r);
	common->QueryIntAttribute("lastBgCol_g", &g);
	common->QueryIntAttribute("lastBgCol_b", &b);
	if (r >= 0 && g >= 0 && b >= 0)
	{
		_appConfig->_bgColor = sf::Color(r, g, b);
	}

	common->QueryAttribute("transparent", &_appConfig->_transparent);
	common->QueryAttribute("menuOnStart", &_uiConfig->_showMenuOnStart);
	const char* lastLayers = common->Attribute("lastLayerSet");
	if (lastLayers != NULL)
		_appConfig->_lastLayerSet = lastLayers;

	common->QueryAttribute("softFall", &_audioConfig->_smoothFactor);

	common->QueryAttribute("audioFilter", &_audioConfig->_doFiltering);

	const char* theme = common->Attribute("theme");
	if (theme != NULL)
		_uiConfig->_theme = theme;


	return true;
}

bool xmlConfigLoader::saveCommon()
{
	_errorMessage = "";
	XMLDocument doc;

	doc.LoadFile(_settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root) root = doc.InsertFirstChild(doc.NewElement("Config"))->ToElement();
	if (root)
	{
		auto common = root->FirstChildElement("Common");
		if (!common) common = doc.NewElement("Common");
		common = root->InsertFirstChild(common)->ToElement();
		if (common)
		{
			common->SetAttribute("startMaximised", _appConfig->_startMaximised);
			common->SetAttribute("lastWidth", _appConfig->_minScrW);
			common->SetAttribute("lastHeight", _appConfig->_minScrH);
			common->SetAttribute("lastX", _appConfig->_scrX);
			common->SetAttribute("lastY", _appConfig->_scrY);
			common->SetAttribute("alwaysOnTop", _appConfig->_alwaysOnTop);
			common->SetAttribute("lastAudioDevice", _audioConfig->_devIdx);
			common->SetAttribute("useKeyboardHook", _appConfig->_useKeyboardHooks);

			common->SetAttribute("lastBgCol_r", _appConfig->_bgColor.r);
			common->SetAttribute("lastBgCol_g", _appConfig->_bgColor.g);
			common->SetAttribute("lastBgCol_b", _appConfig->_bgColor.b);

			common->SetAttribute("transparent", _appConfig->_transparent);
			common->SetAttribute("menuOnStart", _uiConfig->_showMenuOnStart);
			common->SetAttribute("lastLayerSet", _appConfig->_lastLayerSet.c_str());
			common->SetAttribute("softFall", _audioConfig->_smoothFactor);

			common->SetAttribute("audioFilter", _audioConfig->_doFiltering);

			common->SetAttribute("theme", _uiConfig->_theme.c_str());
		}
	}
	else return false;
	
	doc.SaveFile(_settingsFileName.c_str());

	if (doc.Error())
	{
		_errorMessage = "Failed to save document: " + _settingsFileName;
		return false;
	}

	return true;
}

bool xmlConfigLoader::loadPresetNames()
{
	XMLDocument doc;

	_uiConfig->_presetNames.clear();

	doc.LoadFile(_settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root) return false;

	auto presets = root->FirstChildElement("Presets");
	if (!presets) return false;

	auto preset = presets->FirstChildElement("Preset");
	while (preset)
	{
		_uiConfig->_presetNames.push_back(preset->Attribute("Name"));
		preset = preset->NextSiblingElement("Preset");
	}
	
	return true;
}

bool xmlConfigLoader::loadPreset(const std::string & presetName)
{
	XMLDocument doc;
	doc.LoadFile(_settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root) root = doc.InsertFirstChild(doc.NewElement("Config"))->ToElement();
	if (!root) return false;

	auto presets = root->FirstChildElement("Presets");
	if (!presets) presets = root->InsertEndChild(doc.NewElement("Presets"))->ToElement();
	if (!presets) return false;

	auto preset = presets->FirstChildElement("Preset");

	//Check to see if a preset with this name exists already
	while (preset)
	{
		if (preset->Attribute("Name") == presetName)
		{
			break;
		}
		preset = preset->NextSiblingElement("Preset");
	}

	if (!preset)
	{
		//preset doesn't exist
		return false;
	}

	preset->QueryBoolAttribute("AffectsWindow", &_uiConfig->_saveWindowInfo);

	if (_uiConfig->_saveWindowInfo)
	{
		auto window = preset->FirstChildElement("Window");
		if (window)
		{
			window->QueryIntAttribute("x", &_appConfig->_scrX);
			window->QueryIntAttribute("y", &_appConfig->_scrY);
			window->QueryFloatAttribute("w", &_appConfig->_minScrW);
			window->QueryFloatAttribute("h", &_appConfig->_minScrH);
			window->QueryBoolAttribute("AlwaysOnTop", &_appConfig->_alwaysOnTop);
			window->QueryBoolAttribute("Transparent", &_appConfig->_transparent);
			window->QueryFloatAttribute("MinOpacity", &_appConfig->_minOpacity);

			int r = -1;
			int g = -1;
			int b = -1;
			window->QueryIntAttribute("bgCol_r", &r);
			window->QueryIntAttribute("bgCol_g", &g);
			window->QueryIntAttribute("bgCol_b", &b);
			if (r >= 0 && g >= 0 && b >= 0)
			{
				_appConfig->_bgColor = sf::Color(r, g, b);
			}

			_uiConfig->_windowSettingsChanged = true;
		}
	}

	return true;
}

bool xmlConfigLoader::savePreset(const std::string & presetName)
{
	XMLDocument doc;
	doc.LoadFile(_settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root) root = doc.InsertFirstChild(doc.NewElement("Config"))->ToElement();
	if (!root) return false;

	auto presets = root->FirstChildElement("Presets");
	if (!presets) presets = root->InsertEndChild(doc.NewElement("Presets"))->ToElement();
	if (!presets) return false;

	auto preset = presets->FirstChildElement("Preset");

	//Check to see if a preset with this name exists already
	while (preset)
	{
		if (preset->Attribute("Name") == presetName)
		{
			break;
		}
		preset = preset->NextSiblingElement("Preset");
	}

	if (!preset)
	{
		preset = presets->InsertEndChild(doc.NewElement("Preset"))->ToElement();
	}

	
	preset->SetAttribute("Name", presetName.c_str());
	preset->SetAttribute("AffectsWindow", _uiConfig->_saveWindowInfo);

	if (_uiConfig->_saveWindowInfo)
	{
		auto window = preset->FirstChildElement("Window");
		if (!window) window = preset->InsertEndChild(doc.NewElement("Window"))->ToElement();

		window->SetAttribute("x", _appConfig->_scrX);
		window->SetAttribute("y", _appConfig->_scrY);
		window->SetAttribute("w", _appConfig->_minScrW);
		window->SetAttribute("h", _appConfig->_minScrH);
		window->SetAttribute("AlwaysOnTop", _appConfig->_alwaysOnTop);
		window->SetAttribute("Transparent", _appConfig->_transparent);
		window->SetAttribute("MinOpacity", _appConfig->_minOpacity);
		window->SetAttribute("bgCol_r", _appConfig->_bgColor.r);
		window->SetAttribute("bgCol_g", _appConfig->_bgColor.g);
		window->SetAttribute("bgCol_b", _appConfig->_bgColor.b);
	}

	doc.SaveFile(_settingsFileName.c_str());

	return true;
}
