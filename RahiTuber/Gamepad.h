#ifndef GAMEPAD__H__
#define GAMEPAD__H__

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

static const char* const g_gamepadAPINames[3] = {
	"RawInput", "XInput", "SFML"
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


class GamePadImpl
{

public:

	void setAPI(GamepadAPI api)
	{
		inputAPI = api;
	}

	void init(void* wndHandle = 0, GamepadAPI api = (GamepadAPI)0)
	{
		inputAPI = api;
#ifdef _WIN32

		windowHandle = (HWND)wndHandle;

		if (inputAPI == GAMEPAD_API_RAWINPUT)
		{
			RAWINPUTDEVICE deviceList[2] = { 0 };

			deviceList[0].usUsagePage = 0x01;
			deviceList[0].usUsage = 0x04;
			deviceList[0].dwFlags = RIDEV_INPUTSINK;
			deviceList[0].hwndTarget = windowHandle;

			deviceList[1].usUsagePage = 0x01;
			deviceList[1].usUsage = 0x05;
			deviceList[1].dwFlags = RIDEV_INPUTSINK;
			deviceList[1].hwndTarget = windowHandle;

			UINT deviceCount = sizeof(deviceList) / sizeof(*deviceList);
			if(RegisterRawInputDevices(deviceList, deviceCount, sizeof(RAWINPUTDEVICE)) == FALSE)
			{
				printf("RAWINPUT registration failed!");
				//registration failed. Call GetLastError for the cause of the error.

				//fallback to sfml 
				inputAPI = GAMEPAD_API_SFML;
			}
		}

		if (inputAPI == GAMEPAD_API_XINPUT)
		{
			//if (!enabled)
				//XInputEnable(TRUE);
		}

		initialized = true;
#endif
	}

	void update()
	{
#ifdef _WIN32
		if (initialized == false)
			return;

		if (inputAPI == GAMEPAD_API_XINPUT)
		{
			if (reConnectCountdown-- < 0)
			{
				// check connected controllers
				xStates.clear();
				for (int i = 0; i < 4; i++)
				{
					XINPUT_CAPABILITIES caps;
					DWORD res = XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps);
					if (res == 0)
					{
						XInputGetState(i, &xStates[i]);
					}
				}

				reConnectCountdown = 50;
			}
			else
			{
				for (auto& ctrlr : xStates)
					XInputGetState(ctrlr.first, &ctrlr.second);
			}


			return;
		}

		if (inputAPI == GAMEPAD_API_RAWINPUT)
		{
			UINT size;
			GetRawInputBuffer(0, &size, sizeof(RAWINPUTHEADER));
			size *= 8;
			RAWINPUT* rawInput = (RAWINPUT*)malloc(size);
			UINT inputCount = GetRawInputBuffer(rawInput, &size, sizeof(RAWINPUTHEADER));
			if (inputCount != -1)
			{
				RAWINPUT* nextInput = rawInput;
				for (UINT i = 0; i < inputCount; ++i)
				{
					storeRawInputData(nextInput);
					nextInput = NEXTRAWINPUTBLOCK(nextInput);
				}
			}
			free(rawInput);

			return;
		}
#endif;

		sf::Joystick::update();
	}

	float getAxisPosition(unsigned int gamepadID, sf::Joystick::Axis axis)
	{
#ifdef _WIN32
		if (inputAPI == GAMEPAD_API_XINPUT && gamepadID < 4)
		{
			if (xStates.count(gamepadID) == 0)
				return 0;

			auto& xState = xStates[gamepadID];
			switch (axis)
			{
			case sf::Joystick::X:
				return (double)xState.Gamepad.sThumbLX / 32767 * 100;
				break;
			case sf::Joystick::Y:
				return (double)xState.Gamepad.sThumbLY / 32767 * -100;
				break;
			case sf::Joystick::Z:
				return (double)xState.Gamepad.bLeftTrigger / 255 * 100
					+ (double)xState.Gamepad.bRightTrigger / 255 * -100;
				break;
			case sf::Joystick::R:
				break;
			case sf::Joystick::U:
				return (double)xState.Gamepad.sThumbRX / 32767 * 100;
				break;
			case sf::Joystick::V:
				return (double)xState.Gamepad.sThumbRY / 32767 * -100;
				break;
			case sf::Joystick::PovX:
				if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
					return -100;
				if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
					return 100;
				break;
			case sf::Joystick::PovY:
				if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
					return -100;
				if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
					return 100;
				break;
			default:
				break;
			}

			return 0;
		}

		if (inputAPI == GAMEPAD_API_RAWINPUT)
		{
			if (rawStates.count(gamepadID))
			{
				RawState& myState = rawStates[gamepadID];

				switch (axis)
				{
				case sf::Joystick::X:
					return myState.axes[1] * 100;
					break;
				case sf::Joystick::Y:
					return myState.axes[0] * 100;
					break;
				case sf::Joystick::Z:
					return myState.axes[4] * 100;
					break;
				case sf::Joystick::R:
					return 0;					
					break;
				case sf::Joystick::U:
					return myState.axes[3] * 100;
					break;
				case sf::Joystick::V:
					return myState.axes[2] * 100;
					break;
				case sf::Joystick::PovX:
					if (myState.axes[5] == 2 ||
						myState.axes[5] == 3 ||
						myState.axes[5] == 4)
						return 100;
					if (myState.axes[5] == 6 ||
						myState.axes[5] == 7 ||
						myState.axes[5] == 8)
						return -100;
					break;
				case sf::Joystick::PovY:
					if (myState.axes[5] == 8 ||
						myState.axes[5] == 1 ||
						myState.axes[5] == 2)
						return -100;
					if (myState.axes[5] == 4 ||
						myState.axes[5] == 5 ||
						myState.axes[5] == 6)
						return 100;
					break;
				default:
					break;
				}
			}
		}
#endif

		return sf::Joystick::getAxisPosition(gamepadID, axis);
	}

	bool isButtonPressed(unsigned int gamepadID, unsigned int button)
	{
#ifdef _WIN32
		if (inputAPI == GAMEPAD_API_XINPUT && gamepadID < 4)
		{
			if (xStates.count(gamepadID) == 0)
				return 0;

			auto& xState = xStates[gamepadID];

			bool bPressed = false;
			
			switch (button + 1)
			{
			case 1:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
				break;
			case 2:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_B;
				break;
			case 3:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_X;
				break;
			case 4:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_Y;
				break;
			case 5:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
				break;
			case 6:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
				break;
			case 7:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
				break;
			case 8:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_START;
				break;
			case 9:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
				break;
			case 10:
				bPressed = xState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
				break;
			}

#ifdef DEBUG
			if (bPressed)
				printf("XInput button %d pressed \n", button);
#endif // DEBUG

			return bPressed;
		}

		if (inputAPI == GAMEPAD_API_RAWINPUT)
		{
			if (rawStates.count(gamepadID))
			{
				RawState& myState = rawStates[gamepadID];
				bool bPressed = myState.buttons[button + 1];
#ifdef DEBUG
				if(bPressed)
					printf("RAWINPUT button %d pressed \n", button);
#endif // DEBUG

				return bPressed;
			}
		}
#endif

		return sf::Joystick::isButtonPressed(gamepadID, button);
	}

private:


#ifdef _WIN32
	void storeRawInputData(RAWINPUT* input)
	{
		RID_DEVICE_INFO devInfo;
		devInfo.cbSize = sizeof(RID_DEVICE_INFO);
		UINT idsize = devInfo.cbSize;
		GetRawInputDeviceInfoA(input->header.hDevice, RIDI_DEVICEINFO, &devInfo, &idsize);

		devInfo.hid.dwProductId;

		if (rawStateSFIDs.count(input->header.hDevice) == 0)
		{
			//find which one it matches in SFML
			for (int i = 0; i < 8; i++)
			{
				auto ident = sf::Joystick::getIdentification(i);
				if (ident.productId == devInfo.hid.dwProductId && ident.vendorId == devInfo.hid.dwVendorId)
				{
					rawStateSFIDs[input->header.hDevice] = i;
					printf("device %d match: %d \n", i, devInfo.hid.dwProductId);

					rawStates[i].hDevice = input->header.hDevice;
					rawStates[i].productID = devInfo.hid.dwProductId;
					break;
				}
			}
		}
		
		RawState& myState = rawStates[rawStateSFIDs[input->header.hDevice]];

		UINT size;
		GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, 0, &size);
		_HIDP_PREPARSED_DATA* data = (_HIDP_PREPARSED_DATA*)malloc(size);
		UINT bytesWritten = GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, data, &size);
		if (bytesWritten > 0)
		{
			HIDP_CAPS caps;
			HidP_GetCaps(data, &caps);

			//printf("Values: ");
			HIDP_VALUE_CAPS* valueCaps = (HIDP_VALUE_CAPS*)malloc(caps.NumberInputValueCaps * sizeof(HIDP_VALUE_CAPS));
			HidP_GetValueCaps(HidP_Input, valueCaps, &caps.NumberInputValueCaps, data);
			for (USHORT i = 0; i < caps.NumberInputValueCaps; ++i)
			{
				ULONG value;
				HidP_GetUsageValue(HidP_Input, valueCaps[i].UsagePage, 0, valueCaps[i].Range.UsageMin, &value, data, (PCHAR)input->data.hid.bRawData, input->data.hid.dwSizeHid);
				//printf("%d:%5d ", i, value);
				myState.axes[i] = ((double)value / 32767) - 1;
			}
			free(valueCaps);

			for (auto& b : myState.buttons)
				b.second = false;

			//printf("Buttons: ");
			HIDP_BUTTON_CAPS* buttonCaps = (HIDP_BUTTON_CAPS*)malloc(caps.NumberInputButtonCaps * sizeof(HIDP_BUTTON_CAPS));
			HidP_GetButtonCaps(HidP_Input, buttonCaps, &caps.NumberInputButtonCaps, data);
			for (USHORT i = 0; i < caps.NumberInputButtonCaps; ++i)
			{
				ULONG usageCount = buttonCaps->Range.UsageMax - buttonCaps->Range.UsageMin + 1;
				USAGE* usages = (USAGE*)malloc(sizeof(USAGE) * usageCount);
				HidP_GetUsages(HidP_Input, buttonCaps[i].UsagePage, 0, usages, &usageCount, data, (PCHAR)input->data.hid.bRawData, input->data.hid.dwSizeHid);
				for (ULONG usageIndex = 0; usageIndex < usageCount; ++usageIndex) {
					//printf("%d ", usages[usageIndex]);
					myState.buttons[usages[usageIndex]] = true;
				}
				free(usages);
			}
			free(buttonCaps);
			//printf("\n");
		}
		free(data);
	}


	GamepadAPI inputAPI = GAMEPAD_API_RAWINPUT;

	std::map<int, XINPUT_STATE> xStates = {};

	struct RawState {
		HANDLE hDevice;
		int productID;
		std::map<int, float> axes;
		std::map<int, bool> buttons;
	};

	std::map<HANDLE, int> rawStateSFIDs;
	std::map<int, RawState> rawStates = {};

	bool initialized = false;

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

	static void init(void* wndHandle = 0, GamepadAPI api = (GamepadAPI)0)
	{
		GetInstance().init(wndHandle, api);
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
