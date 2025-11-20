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
	GAMEPAD_MODEL_XB360 = 1281,
	GAMEPAD_MODEL_XBONE = 767,
	GAMEPAD_MODEL_XBX,
	GAMEPAD_MODEL_SWITCH = 392,
	GAMEPAD_MODEL_PS4,
	GAMEPAD_MODEL_PS5,
	GAMEPAD_MODEL_VJOY = 48813,

	GAMEPAD_MODEL_END
};

class GamePadImpl
{

public:

	void setAPI(GamepadAPI api)
	{
		inputAPI = api;
	}

	void init(void* wndHandle = 0, AppConfig* appcfg = nullptr, GamepadAPI api = (GamepadAPI)0);

	void update();

	float getAxisPosition(unsigned int gamepadID, sf::Joystick::Axis axis);

	bool isButtonPressed(unsigned int gamepadID, unsigned int button);

private:

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


#ifdef _WIN32
	void storeRawInputData(RAWINPUT* input);

	GamepadAPI inputAPI = GAMEPAD_API_RAWINPUT;

	std::map<int, XINPUT_STATE> xStates = {};

	struct RawState {
		HANDLE hDevice;
		int productID = 0;
		float maxValue = (float)INT32_MAX;
		GamePadModel model = GAMEPAD_MODEL_XB360;
		std::map<int, float> axes;
		std::map<int, bool> buttons;
	};

	std::map<HANDLE, int> rawStateSFIDs;
	std::map<int, RawState> rawStates = {};

	bool initialized = false;

	AppConfig* appConfig = nullptr;

	HWND windowHandle;
#else
    GamepadAPI inputAPI = GAMEPAD_API_SFML;
#endif

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

	static void init(void* wndHandle = 0, AppConfig* appcfg = nullptr, GamepadAPI api = (GamepadAPI)0)
	{
		GetInstance().init(wndHandle, appcfg, api);
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

private:
	static GamePadImpl& GetInstance()
	{
		if (!GamePadSingleton::singleton)
			GamePadSingleton::singleton = std::make_unique<GamePadImpl>();

		return *GamePadSingleton::singleton;
	}
};


#endif
