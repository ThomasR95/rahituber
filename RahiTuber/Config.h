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

#include "TextureManager.h"

#include <fstream>
#include <thread>

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
class WebSocket;

struct AppConfig
{
	xmlConfigLoader* _loader = nullptr;
	std::string lastLayerSettingsFile = "";
	std::string windowName = "RahiTuber";
	bool _pendingNameChange = false;
	bool _pendingSpoutNameChange = false;
	std::mutex _nameLock;
	bool _nameWindowWithSet = false;

	bool _listenHTTP = false;
	int _httpPort = 8000;
	WebSocket* _webSocket;

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
	float scalingFactor = 1.0;
	float mainWindowScaling = 1.0;
	float menuWindowScaling = 1.0;
	float customScaling = 1.0;

	bool _enableVSync = false;


	sf::RenderWindow _window;
	sf::RenderWindow _menuWindow;
	sf::RenderTexture _layersRT;
	sf::RenderTexture _menuRT;
	sf::RectangleShape _RTPlane;

	sf::Shader _shader;
	float _minOpacity = 0.25f;

	bool _startMaximised = false;
	bool _isFullScreen = false;
	bool _wasFullScreen = false;
	bool _alwaysOnTop = false;
	bool _useKeyboardHooks = false;
	bool _menuPopped = false;
	bool _menuPopPending = false;

	sf::Vector2i _lastMenuPopPosition = { 0, 0 };

	float _fps = 0;
	int _fpsLimit = 60;

	sf::Clock _timer;

	sf::Clock _hoverTimer;

	sf::Clock _runTime;

	std::string _lastLayerSet = u8"lastLayers";

	//debug audio bars
	std::vector<sf::RectangleShape> bars;

	TextureManager _textureMan;

	std::string _appLocation = u8"";

	std::string _logFileLocation = "";
	std::fstream _logStream = {};

	bool _useSpout2Sender = false;
	bool _spoutNeedsCPU = false;

	bool _createMinimalLayers = false;
	bool _undoRotationEffectFix = false;

	bool _mouseTrackingEnabled = true;

	float _versionNumber = 0.0;
	bool _checkForUpdates = true;
	bool _updateAvailable = false;
	float _versionAvailable = 0.0;
	std::thread* _checkUpdateThread = nullptr;

	bool _compositeOntoBackground = true;
	bool _alphaPremultiplied = true;
	int _unloadTimeoutSetting = 10;
	int _unloadTimeout = 10;
	bool _unloadTimeoutEnabled = false;
};

typedef struct
{

}
paTestData;

struct AudioConfig
{
	float _cutoff = 0.0006f;

	SAMPLE _frame = 0.f;
	SAMPLE _runningAverage = 0.0001f;
	SAMPLE _runningMax = 0.0001f;
	SAMPLE _frameMax = 0.0001f;

	std::vector<SAMPLE> _frames;
	std::vector<SAMPLE> _fftData;
	std::vector<SAMPLE> _frequencyData;
	std::mutex _freqDataMutex;
	SAMPLE _bassHi = 0.0f;
	SAMPLE _bassMax = 0.0f;
	SAMPLE _bassAverage = 0.0f;

	SAMPLE _trebleHi = 0.0f;
	SAMPLE _trebleMax = 0.0f;
	SAMPLE _trebleAverage = 0.0f;

	SAMPLE _midHi = 0.0f;
	SAMPLE _midMax = 0.0f;
	SAMPLE _midAverage = 0.0f;

	float _smoothAmount = 10.0f;
	float _smoothFactor = 24.0f;

	SAMPLE _frameHi = 0.0f;
	PaStreamParameters _params = {};
	PaDeviceIndex _devIdx = -1;
	int _nDevices = 0;
	std::vector<std::pair<std::string, int>> _deviceList;
	paTestData* _streamData = nullptr;
	PaStream* _audioStr = nullptr;
	bool _leftChannel = true;
	int _numChannels = 2;

	SAMPLE _fixedMax = 1.0;
	bool _softMaximum = false;

	sf::Clock _quietTimer;
	sf::Clock _recordTimer;

	bool _doFiltering = false;

	bool _compression = false;

	bool _processedNew = false;
};

struct UIConfig
{
	enum LayerUIType
	{
		LayersUI_Old,
		LayersUI_Menus,
		LayersUI_Buttons
	};

	std::map<LayerUIType, std::string> _layerUITypeNames = {
		{LayersUI_Old, "Legacy"},
		{LayersUI_Menus, "Menus"},
		{LayersUI_Buttons, "Buttons"}
	};

	LayerUIType _layersUIType = LayersUI_Buttons;


	sf::Image _ico;
	sf::Texture _moveIcon;
	sf::Sprite _moveIconSprite;
	sf::Vector2f _moveTabSize = { 80,32 };

	sf::Vector2f _helpBtnPosition = { 0,0 };

	sf::Font _font;
	bool _showMenuOnStart = true;
	bool _menuShowing = true;
	bool _advancedMenuShowing = false;
	bool _showFPS = false;
	bool _firstMenu = true;
	sf::Color* _editingColour;
	bool _showLayerBounds = true;
	bool _hilightHovered = false;

	bool _helpPopupActive = false;
	bool _presetPopupActive = false;
	float _advancedMenuHeight = 540;

	sf::RectangleShape _topLeftBox;
	sf::RectangleShape _bottomRightBox;
	sf::RectangleShape _outlineBox;
	sf::RectangleShape _resizeBox;
	std::pair<bool, bool> _cornerGrabbed = { false, false };
	bool _moveGrabbed = false;
	sf::Vector2i _lastMiddleClickPosition = { -1, -1 };
	bool _middleClickGrabbed = false;

	std::string _settingsFile;
	std::vector<std::string> _presetNames;
	int _presetIdx = 0;
	std::vector<char> _settingsFileBoxName;
	bool _saveVisInfo = true;
	bool _saveWindowInfo = false;
	bool _clearVisInfo = false;
	bool _clearWindowInfo = false;
	bool _windowSettingsChanged = false;

	bool _audioExpanded = true;

	std::string _theme = "Default";
	std::string _fontName = "res/monof55.ttf";
	float _fontSize = 26.f;
	bool _fontReloadNeeded = false;

	struct ThemeDef
	{
		ImVec4 first = ImVec4(0.37f, 0.1f, 0.35f, 1.0f);
		ImVec4 second = ImVec4(0.75f, 0.2f, 0.7f, 1.0f);
		std::string fontName = "res/monof55.ttf";
		float fontSize = 26.f;
	};

	std::map<std::string, ThemeDef> _themes = {
		{"Default", { {0.37f, 0.1f, 0.35f, 1.0f}, {0.75f, 0.2f, 0.7f, 1.0f} }},
		{"Sunset",  { {0.35f, 0.05f, 0.3f, 1.0f},  {0.6f, 0.2f, 0.2f, 1.0f} }},
		{"Forest",  { {0.15f, 0.2f, 0.25f, 1.0f},  {0.2f, 0.5f, 0.3f, 1.0f} }},
		{"Iron",    { {0.15f, 0.15f, 0.175f, 1.0f}, {0.75f, 0.4f, 0.2f, 1.0f} }},
		{"Silver",  { {0.37f, 0.275f, 0.4f, 1.0f},  {0.55f, 0.6f, 0.65f, 1.0f} }},
		{"Volcano", { {0.35f, 0.05f, 0.1f, 1.0f},  {0.9f, 0.6f, 0.2f, 1.0f} }},
		{"Oxide",   { {0.25f, 0.35f, 0.4f, 1.0f},  {0.7f, 0.2f, 0.05f, 1.0f} }},
		{"Pretty",  { {0.5f, 0.25f, 0.45f, 1.0f},  {0.7f, 0.75f, 1.0f, 1.0f} }},
		{"Contrast",  { {0.3f, 0.2f, 0.4f, 1.0f},  {0.2f, 0.5f, 0.5f, 1.f}, "res/verdana.ttf", 30.f }},
	};

	std::string _lastTheme = "";
	bool _styleLoaded = false;

	sf::Texture fontTex;
	sf::Image fontimg;
	bool fontBuilt = false;

	std::map<int, std::string> _numbertypes = {
		{0, "Slider"},
		{1, "Drag (use Alt for fine adjustment)"},
		{2, "Input Only"}
	};

	int _numberEditType = 0;
};


static void logToFile(AppConfig* appCfg, const std::string& msg, bool clear = false)
{
	if (appCfg != nullptr)
	{
		if(appCfg->_logFileLocation == "")
			appCfg->_logFileLocation = appCfg->_appLocation + "RahiTuber_Log.txt";

		if (clear)
		{
			if (appCfg->_logStream.is_open())
			{
				appCfg->_logStream.close();
			}

			std::error_code ec;
			fs::remove(appCfg->_logFileLocation, ec);

			std::ofstream outfile(appCfg->_logFileLocation);
			outfile << "" << std::endl;
			outfile.close();
		}

		if (appCfg->_logStream.is_open() == false)
		{
			appCfg->_logStream.open(appCfg->_logFileLocation);
		}

		std::string cmd = "";

#ifdef _DEBUG
		std::cout << msg << std::endl;
		OutputDebugStringA((msg + "\n").c_str());
#endif

		if (appCfg->_logStream.is_open())
		{
			appCfg->_logStream << msg << std::endl << std::flush;
		}
			
	}
}
