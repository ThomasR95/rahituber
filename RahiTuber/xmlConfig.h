#pragma once

#include "Config.h"
#include "tinyxml2\tinyxml2.h"

class xmlConfigLoader
{
public:
	xmlConfigLoader(AppConfig* gameConfig, UIConfig* uiConfig, AudioConfig* audioConfig, const std::string& settingsXMLFile);
	~xmlConfigLoader();

	bool loadCommon();
	bool saveCommon();

	bool loadPresetNames();

	bool loadPreset(const std::string& presetName);
	bool savePreset(const std::string& presetName);

	std::string getSettingsFilePath() { return _settingsFileName; }

	std::string _errorMessage = "";

private:

	AppConfig* _appConfig = nullptr;
	UIConfig* _uiConfig = nullptr;
	AudioConfig* _audioConfig = nullptr;
	std::string _settingsFileName = "";
};

