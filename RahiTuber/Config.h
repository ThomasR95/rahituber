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

#define PI 3.14159265359

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

	float _fullScrW;
	float _fullScrH;
	float _minScrW;
	float _minScrH;
	float _scrW;
	float _scrH;
	float _ratio;
	int _scrX;
	int _scrY;

	bool _enableVSync = true;

	sf::RenderWindow _window;
	sf::RenderWindow* _currentWindow;
	sf::RenderTexture _RT;

	sf::Shader _shader;
	float _minOpacity = 0.25f;

	bool _startMaximised = false;
	bool _isFullScreen = false;
	bool _wasFullScreen = false;
	bool _alwaysOnTop = false;

	sf::Clock _timer;

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
	SAMPLE _bassHi;
	SAMPLE _bassMax;
	SAMPLE _bassAverage;

	SAMPLE _trebleHi;
	SAMPLE _trebleMax;
	SAMPLE _trebleAverage;

	SAMPLE _midHi;
	SAMPLE _midMax;
	SAMPLE _midAverage;

	float _smoothAmount = 10;

	SAMPLE _frameHi;
	PaStreamParameters _params;
	PaDeviceIndex _devIdx = -1;
	int _nDevices;
	std::vector<std::pair<std::string, int>> _deviceList;
	paTestData* _streamData;
	PaStream* _audioStr;
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

	bool _menuShowing = false;
	bool _advancedMenuShowing = false;
	bool _showFPS = false;
	bool _firstMenu = true;
	sf::Color* _editingColour;

	sf::RectangleShape _topLeftBox;
	sf::RectangleShape _bottomRightBox;
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
};