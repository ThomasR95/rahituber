#pragma once

#include "pa_util.h"
#include "pa_ringbuffer.h"
#include "portaudio.h"

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include <memory>
#include <deque>
#include <mutex>

#include "defines.h"

#define SCRW 1280
#define SCRH 720

/* PortAudio setup */
#define NUM_CHANNELS    (2)
#define FRAMES_PER_BUFFER  512
#define SAMPLE_RATE 44100

#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE (0.0f)
#define PRINTF_S_FORMAT "%.8f"

class xmlConfigLoader;

struct AppConfig
{
	xmlConfigLoader* _loader;

	bool _transparent = false;

	sf::Color _bgColor = sf::Color(128,110,128);

	float _fullScrW = 1920;
	float _fullScrH = 1080;
	float _minScrW = SCRW;
	float _minScrH = SCRH;
	float _scrW = SCRW;
	float _scrH = SCRH;
	float _ratio = 16.f / 9.f;
	int _scrX = 0;
	int _scrY = 0;

	bool _enableVSync = true;

	sf::RenderWindow _window;
	sf::RenderWindow* _currentWindow = nullptr;
	sf::RenderTexture _layersRT;
	sf::RenderTexture _menuRT;
	sf::RectangleShape _RTPlane;

	sf::Shader _shader;
	float _minOpacity = 0.25f;

	bool _startMaximised = false;
	bool _isFullScreen = false;
	bool _wasFullScreen = false;
	bool _alwaysOnTop = false;
	bool _useKeyboardHooks = true;

	float _fps = 0;

	sf::Clock _timer;

	std::string _lastLayerSet = "lastLayers";

	//debug audio bars
	std::vector<sf::RectangleShape> bars;
};

typedef struct
{

}
paTestData;

struct AudioConfig
{
	float _cutoff = 0.0006;

	SAMPLE _frame = 0;
	SAMPLE _runningAverage = 0.0001;
	SAMPLE _runningMax = 0.0001;
	SAMPLE _frameMax = 0.0001;

	std::vector<SAMPLE> _frames;
	std::vector<SAMPLE> _fftData;
	std::vector<SAMPLE> _frequencyData;
	std::mutex _freqDataMutex;
	SAMPLE _bassHi = 0.0;
	SAMPLE _bassMax = 0.0;
	SAMPLE _bassAverage = 0.0;

	SAMPLE _trebleHi = 0.0;
	SAMPLE _trebleMax = 0.0;
	SAMPLE _trebleAverage = 0.0;

	SAMPLE _midHi = 0.0;
	SAMPLE _midMax = 0.0;
	SAMPLE _midAverage = 0.0;

	float _smoothAmount = 10.0;
	float _smoothFactor = 24.0;

	SAMPLE _frameHi = 0.0;
	PaStreamParameters _params;
	PaDeviceIndex _devIdx = -1;
	int _nDevices = 0;
	std::vector<std::pair<std::string, int>> _deviceList;
	paTestData* _streamData;
	PaStream* _audioStr = nullptr;
	bool _leftChannel = true;
	int _numChannels = 2;

	sf::Clock _quietTimer;
};

struct UIConfig
{
	sf::Image _ico;
	sf::Texture _moveIcon;
	sf::Sprite _moveIconSprite;
	sf::Vector2f _moveTabSize = { 80,32 };

	bool _showMenuOnStart = true;
	bool _menuShowing = true;
	bool _advancedMenuShowing = false;
	bool _showFPS = false;
	bool _firstMenu = true;
	sf::Color* _editingColour;

	sf::RectangleShape _topLeftBox;
	sf::RectangleShape _bottomRightBox;
	sf::RectangleShape _outlineBox;
	sf::RectangleShape _resizeBox;
	std::pair<bool, bool> _cornerGrabbed = { false, false };
	bool _moveGrabbed = false;
	sf::Vector2i _lastMiddleClickPosition = { -1, -1 };

	std::string _settingsFile;
	std::vector<std::string> _presetNames;
	int _presetIdx;
	std::vector<char> _settingsFileBoxName;
	bool _saveVisInfo = true;
	bool _saveWindowInfo = false;
	bool _clearVisInfo = false;
	bool _clearWindowInfo = false;
	bool _windowSettingsChanged = false;

	std::string _theme = "Default";

	std::map<std::string, std::pair<ImVec4, ImVec4>> _themes = {
		{"Default", { {0.75, 0.2, 0.7, 1.0}, {0.75, 0.2, 0.7, 1.0} }},
		{"Sunset",  { {0.7, 0.2, 0.6, 1.0},  {0.6, 0.2, 0.2, 1.0} }},
		{"Forest",  { {0.3, 0.4, 0.5, 1.0},  {0.2, 0.5, 0.3, 1.0} }},
		{"Iron",    { {0.3, 0.3, 0.35, 1.0}, {0.75, 0.4, 0.2, 1.0} }},
		{"Silver",  { {0.75, 0.55, 0.8, 1.0},  {0.55, 0.6, 0.65, 1.0} }},
		{"Volcano", { {0.7, 0.1, 0.2, 1.0},  {0.9, 0.6, 0.2, 1.0} }},
		{"Uncouth", { {0.5, 0.7, 0.8, 1.0},  {0.7, 0.2, 0.05, 1.0} }},
		{"Pretty",  { {1.0, 0.5, 0.9, 1.0},  {0.7, 0.75, 1.0, 1.0} }},
	};

};