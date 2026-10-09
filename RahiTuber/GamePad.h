#ifndef GAMEPAD__H__
#define GAMEPAD__H__

#define LOGRAWINPUT 0

#include "SFML/Main.hpp"
#include "SFML/System.hpp"
#include "SFML/Window.hpp"

#ifdef _WIN32
	#include <Xinput.h>
	#include <WinUser.h>
	#include <debugapi.h>

	#define QWORD uint64_t
	#include <hidsdi.h>
	#include <hidpi.h>
	#include <inttypes.h>
#endif

#include <map>
#include <memory>
#include <list>

struct AppConfig;

static const char* const g_gamepadAPINames[3] = {
	"RawInput", "XInput", "SFML"
};

static const char* const g_gamepadModelNames[7] = {
	"XBOX 360", "XBOX One", "XBOX S/X", "Switch", "PS4", "PS5", "VJoy"
};

static const char* const g_gamepadAPITooltips[3] = {
	"Most likely to work when the window is out of focus", "XInput (Only compatible with XBOX GamePads)", "The default method before version 13.8"
};

enum GamepadAPI {
#ifdef _WIN32
	GAMEPAD_API_RAWINPUT,
	GAMEPAD_API_XINPUT,
#endif
	GAMEPAD_API_SFML,

	GAMEPAD_API_END
};

enum GamePadModel : int {
	GAMEPAD_MODEL_XBOX,
	GAMEPAD_MODEL_SWITCH,
	GAMEPAD_MODEL_PS4,
	GAMEPAD_MODEL_PS5,
	GAMEPAD_MODEL_VJOY,

	GAMEPAD_MODEL_END
};

static const char* g_gamepad_model_names[GAMEPAD_MODEL_END] =
{
	"GAMEPAD_MODEL_XBOX		 ",
	"GAMEPAD_MODEL_SWITCH	 ",
	"GAMEPAD_MODEL_PS4		 ",
	"GAMEPAD_MODEL_PS5		 ",
	"GAMEPAD_MODEL_VJOY		 ",
};

struct GamePadID
{
	GamePadID() {}
	GamePadID(const std::string& _name) : name(_name), alikeIdx(0) {}
	GamePadID(const std::string& _name, int _alikeIdx) : name(_name), alikeIdx(_alikeIdx) {}

	std::string name = "";
	int alikeIdx = 0;

	bool empty()
	{
		return name == "";
	}

	bool operator==(const GamePadID& rhs) const
	{
		return name == rhs.name && alikeIdx == rhs.alikeIdx;
	}
};

class GamePadImpl
{

public:

	void setAPI(GamepadAPI api)
	{
		unregister();
		init(windowHandle, appConfig, api, mouseInputAPI);
	}

	void setMouseAPI(GamepadAPI api)
	{
		if (api == GAMEPAD_API_XINPUT)
			return;

		unregister();
		init(windowHandle, appConfig, inputAPI, api);
	}

	void setMouseRelativeTracking(bool rel) {
		mouseRelativeTracking = rel; 
		if (mouseRelativeTracking)
			mousePos = { 0,0 };
	}

	void init(void* wndHandle = 0, AppConfig* appcfg = nullptr, GamepadAPI api = (GamepadAPI)0, GamepadAPI mouseApi = GAMEPAD_API_SFML);

	void update();

	void unregister();

	float getAxisPosition(unsigned int gamepadID, sf::Joystick::Axis axis);

	bool isButtonPressed(unsigned int gamepadID, unsigned int button);

	std::map<int, GamePadID>& enumerateGamePads();

	std::map<int, GamePadID>& getGamePads() { return _gamePadList; }

	sf::Vector2i getMousePosition() 
	{ 
		return mouseInputAPI == GAMEPAD_API_RAWINPUT ? mousePos : sf::Mouse::getPosition();
	}
	bool isMouseButtonPressed(sf::Mouse::Button btn) 
	{ 
		return mouseInputAPI == GAMEPAD_API_RAWINPUT ? mouseBtns[btn] : sf::Mouse::isButtonPressed(btn);
	}

	void SetMouseMovementOptions(int speed, float ease, float delay, float dzone)
	{
		mouseEase = ease * 1000;
		mouseTimeout = delay * 1000;
		returnSpeed = speed;
		deadZone = dzone;
	}

private:
	std::map<int, GamePadID> _gamePadList;

#ifdef _WIN32

    bool isDualshock4(RID_DEVICE_INFO_HID info)
    {
        const DWORD sonyVendorID = 0x054C;
        const DWORD ds4Gen1ProductID = 0x05C4;
        const DWORD ds4Gen2ProductID = 0x09CC;

        return info.dwVendorId == sonyVendorID && (info.dwProductId == ds4Gen1ProductID || info.dwProductId == ds4Gen2ProductID);
    }

    bool isDualsense(RID_DEVICE_INFO_HID info)
    {
        const DWORD sonyVendorID = 0x054C;
        const DWORD dualsenseProductID = 0x0CE6;
        const DWORD dualsenseEdgeProductID = 0x0DF2;

        return info.dwVendorId == sonyVendorID && (info.dwProductId == dualsenseProductID || info.dwProductId == dualsenseEdgeProductID);
    }

    bool isSwitch(RID_DEVICE_INFO_HID info)
    {
        const DWORD nintendoVendorID = 3695;
        const DWORD switchProProductID = 392;

        return info.dwVendorId == nintendoVendorID && (info.dwProductId == switchProProductID);
    }

    bool isVJoy(RID_DEVICE_INFO_HID info)
    {
        const DWORD vjoyVendorID = 4660;
        const DWORD vjoyProductID = 48813;

        return info.dwVendorId == vjoyVendorID && (info.dwProductId == vjoyProductID);
    }

    bool isXBOX(RID_DEVICE_INFO_HID info)
    {
        const DWORD xbxVendorID = 1118;
        const DWORD xbxProductID = 767;

        return info.dwVendorId == xbxVendorID && (info.dwProductId == xbxProductID);

    }

	void storeRawInputData(const RAWINPUT& input);

	GamepadAPI inputAPI = GAMEPAD_API_RAWINPUT;
	GamepadAPI mouseInputAPI = GAMEPAD_API_SFML;

	std::map<int, XINPUT_STATE> xStates = {};

	struct RawState {
		HANDLE hDevice;
		int productID = 0;
		GamePadModel model = GAMEPAD_MODEL_XBOX;
		std::map<int, ULONG> maxAxes;
		std::map<int, float> axes;
		std::map<int, bool> buttons;
	};

	std::map<HANDLE, int> rawStateSFIDs;
	std::map<int, RawState> rawStates = {};

	std::vector<RAWINPUTDEVICE> registeredRIDs = {};

	bool initialized = false;

	HWND windowHandle;

	bool mouseRelativeTracking = false;
	sf::Vector2i mousePos = {};
	float mouseWheel = 0;
	std::map<sf::Mouse::Button, bool> mouseBtns = 
	{{sf::Mouse::Button::Left, false},
		{sf::Mouse::Button::Right, false},
		{sf::Mouse::Button::Middle, false},
		{sf::Mouse::Button::XButton1, false},
		{sf::Mouse::Button::XButton2, false}};

	int mouseTimeout = 500;
	int mouseEase = 500;
	ffwdClock mouseTimer;
	float returnSpeed = 4000;
	float deadZone = 2;

	//std::list<std::thread> updateThreads;

#else
    GamepadAPI inputAPI = GAMEPAD_API_SFML;
		GamepadAPI mouseInputAPI = GAMEPAD_API_SFML;
#endif

    AppConfig* appConfig = nullptr;

	int reConnectCountdown = 50;

	bool enabled = false;

  
};

namespace GamePadSingleton
{
	static std::unique_ptr<GamePadImpl> singleton;
}

class GamePad
{
public:

	static void setAPI(GamepadAPI api)
	{
		GetInstance().setAPI(api);
	}

	static void setMouseAPI(GamepadAPI api)
	{
		GetInstance().setMouseAPI(api);
	}

	static void init(void* wndHandle = 0, AppConfig* appcfg = nullptr, GamepadAPI api = (GamepadAPI)0, GamepadAPI mouseApi = GAMEPAD_API_SFML)
	{
		GetInstance().init(wndHandle, appcfg, api, mouseApi);
	}

	static void update()
	{
		GetInstance().update();
	}

	static float getAxisPosition(unsigned int gamepadID, sf::Joystick::Axis axis)
	{
		return GetInstance().getAxisPosition(gamepadID, axis);
	}

	static bool isButtonPressed(unsigned int gamepadID, unsigned int button)
	{
		return GetInstance().isButtonPressed(gamepadID, button);
	}

	static std::map<int, GamePadID>& enumerateGamePads()
	{
		return GetInstance().enumerateGamePads();
	}

	static std::map<int, GamePadID>& getGamePads()
	{
		return GetInstance().getGamePads();
	}

	static sf::Vector2i getMousePosition()
	{
		return GetInstance().getMousePosition();
	}

	static bool isMouseButtonPressed(sf::Mouse::Button btn)
	{
		return GetInstance().isMouseButtonPressed(btn);
	}

	static void setMouseRelativeTracking(bool rel)
	{
		GetInstance().setMouseRelativeTracking(rel);
	}

	static void SetMouseMovementOptions(int speed, float ease, float delay, float dzone)
	{
		GetInstance().SetMouseMovementOptions(speed, ease, delay, dzone);
	}

private:
	static GamePadImpl& GetInstance()
	{
		if (!GamePadSingleton::singleton)
			GamePadSingleton::singleton = std::make_unique<GamePadImpl>();

		return *GamePadSingleton::singleton;
	}
};


#endif
