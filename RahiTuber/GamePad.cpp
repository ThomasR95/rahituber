#include "Config.h"

#include "Gamepad.h"


void GamePadImpl::init(void* wndHandle, AppConfig* appcfg, GamepadAPI api)
{
	inputAPI = api;
	appConfig = appcfg;
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
		if (RegisterRawInputDevices(deviceList, deviceCount, sizeof(RAWINPUTDEVICE)) == FALSE)
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

void GamePadImpl::update()
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
		//for (auto th = updateThreads.begin(); th != updateThreads.end(); th++)
		//{
		//	if (th->joinable())
		//	{
		//		th->join();
		//		updateThreads.erase(th);
		//	}
		//}

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
				//updateThreads.push_back(std::thread(&GamePadImpl::storeRawInputData, this, *nextInput));
				storeRawInputData(*nextInput);
				nextInput = NEXTRAWINPUTBLOCK(nextInput);
			}
		}

		free(rawInput);

		return;
	}
#endif;

	sf::Joystick::update();
}

float GamePadImpl::getAxisPosition(unsigned int gamepadID, sf::Joystick::Axis axis)
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
			RawState myState = rawStates[gamepadID];

			USAGE RAxis = HID_USAGE_GENERIC_RZ;
			USAGE UAxis = HID_USAGE_GENERIC_RX;
			USAGE VAxis = HID_USAGE_GENERIC_RY;

			if (myState.model == GAMEPAD_MODEL_SWITCH)
			{
				UAxis = HID_USAGE_GENERIC_Z;
				VAxis = HID_USAGE_GENERIC_RZ;
			}
			//else if (myState.model == GAMEPAD_MODEL_VJOY)
			//{
			//	RAxis = HID_USAGE_GENERIC_RZ;

			//	UAxis = HID_USAGE_GENERIC_RX;
			//	VAxis = HID_USAGE_GENERIC_RY;
			//}

			switch (axis)
			{
			case sf::Joystick::X:
				return myState.axes[HID_USAGE_GENERIC_X] * 100;
				break;
			case sf::Joystick::Y:
				return myState.axes[HID_USAGE_GENERIC_Y] * 100;
				break;
			case sf::Joystick::Z:
				return myState.axes[HID_USAGE_GENERIC_Z] * 100;
				break;
			case sf::Joystick::R:
				return myState.axes[RAxis] * 100;
				break;
			case sf::Joystick::U:
				return myState.axes[UAxis] * 100;
				break;
			case sf::Joystick::V:
				return myState.axes[VAxis] * 100;
				break;
			case sf::Joystick::PovX:
				if (myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 2 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 3 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 4)
					return 100;
				if (myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 6 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 7 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 8)
					return -100;
				break;
			case sf::Joystick::PovY:
				if (myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 8 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 1 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 2)
					return -100;
				if (myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 4 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 5 ||
					myState.axes[HID_USAGE_GENERIC_HATSWITCH] == 6)
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

bool GamePadImpl::isButtonPressed(unsigned int gamepadID, unsigned int button)
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
#if defined(DEBUG) || LOGRAWINPUT
			if (bPressed)
				logToFile(appConfig, "RAWINPUT button " + std::to_string(button) + " pressed \n");
#endif // DEBUG

			return bPressed;
		}
	}
#endif

	return sf::Joystick::isButtonPressed(gamepadID, button);
}

std::map<int, GamePadID>& GamePadImpl::enumerateGamePads()
{
	_gamePadList.clear();

	std::map<std::string, int> alikeCounts;

	for (int jStick = 0; jStick < sf::Joystick::Count; jStick++)
	{
		std::string name = sf::Joystick::getIdentification(jStick).name;
		auto& thisPad = _gamePadList[jStick];
		thisPad.name = name;
		thisPad.alikeIdx = alikeCounts[name]++;
	}

	return _gamePadList;
}

#ifdef _WIN32
void GamePadImpl::storeRawInputData(const RAWINPUT& input)
{
	RID_DEVICE_INFO devInfo;
	devInfo.cbSize = sizeof(RID_DEVICE_INFO);
	UINT idsize = devInfo.cbSize;
	GetRawInputDeviceInfoA(input.header.hDevice, RIDI_DEVICEINFO, &devInfo, &idsize);

	bool logValues = false;

	if (rawStateSFIDs.count(input.header.hDevice) == 0)
	{
		bool foundSFMLMatch = false;
		//find which one it matches in SFML
		for (int i = 0; i < 8; i++)
		{
			auto ident = sf::Joystick::getIdentification(i);
			if (ident.productId == devInfo.hid.dwProductId && ident.vendorId == devInfo.hid.dwVendorId)
			{
				rawStateSFIDs[input.header.hDevice] = i;
				logToFile(appConfig, "GamePad device " + std::to_string(i) + " match: Product " + std::to_string(devInfo.hid.dwProductId) + " Vendor " + std::to_string(devInfo.hid.dwVendorId));

				rawStates[i].hDevice = input.header.hDevice;
				rawStates[i].productID = devInfo.hid.dwProductId;
				foundSFMLMatch = true;
				break;
			}
		}

		if (foundSFMLMatch == false)
		{
			logToFile(appConfig, "SFML did not map GamePad device: Product " + std::to_string(devInfo.hid.dwProductId) + " Vendor " + std::to_string(devInfo.hid.dwVendorId));

			rawStateSFIDs[input.header.hDevice] = (int)input.header.hDevice;
			rawStates[(int)input.header.hDevice].hDevice = input.header.hDevice;
			rawStates[(int)input.header.hDevice].productID = devInfo.hid.dwProductId;
		}

		logValues = true;
	}

#if LOGRAWINPUT
	logValues = true;
#endif

	RawState& myState = rawStates[rawStateSFIDs[input.header.hDevice]];

	if (isDualshock4(devInfo.hid))
		myState.model = GAMEPAD_MODEL_PS4;
	else if (isDualsense(devInfo.hid))
		myState.model = GAMEPAD_MODEL_PS5;
	else if (isSwitch(devInfo.hid))
		myState.model = GAMEPAD_MODEL_SWITCH;
	else if (isVJoy(devInfo.hid))
		myState.model = GAMEPAD_MODEL_VJOY;
	else if (isXBX(devInfo.hid))
		myState.model = GAMEPAD_MODEL_XBX;

	UINT size;
	GetRawInputDeviceInfo(input.header.hDevice, RIDI_PREPARSEDDATA, 0, &size);
	_HIDP_PREPARSED_DATA* data = (_HIDP_PREPARSED_DATA*)malloc(size);
	UINT bytesWritten = GetRawInputDeviceInfo(input.header.hDevice, RIDI_PREPARSEDDATA, data, &size);
	if (bytesWritten > 0)
	{
		HIDP_CAPS caps;
		HidP_GetCaps(data, &caps);

		char logbuf[1024] = "\0";

		if (logValues) snprintf(logbuf, 1024, "%s Axis Values: \n", g_gamepad_model_names[myState.model]);

		HIDP_VALUE_CAPS* valueCaps = (HIDP_VALUE_CAPS*)malloc(caps.NumberInputValueCaps * sizeof(HIDP_VALUE_CAPS));
		HidP_GetValueCaps(HidP_Input, valueCaps, &caps.NumberInputValueCaps, data);
		for (USHORT i = 0; i < caps.NumberInputValueCaps; ++i)
		{
			ULONG& maxAxis = myState.maxAxes[valueCaps[i].Range.UsageMin];
			if (maxAxis == 0)
				maxAxis = 15; // smallest maximum i've seen

			ULONG value;
			HidP_GetUsageValue(HidP_Input, valueCaps[i].UsagePage, 0, valueCaps[i].Range.UsageMin, &value, data, (PCHAR)input.data.hid.bRawData, input.data.hid.dwSizeHid);
			
			if (valueCaps[i].LogicalMax > 0)
			{
				if(myState.model == GAMEPAD_MODEL_VJOY)
					maxAxis = valueCaps[i].LogicalMax;
				else
					maxAxis = ((ULONG)valueCaps[i].LogicalMax * 2) - 1;
			}
			else if (valueCaps[i].BitSize > 0)
				maxAxis = (1l << valueCaps[i].BitSize) - 1;
			else
				maxAxis = Max(maxAxis, value);

			if(logValues) snprintf(logbuf, 1024, "%d:%8u, bitsize: %u, logical max (signed): %8d, max %8u, usage: %#010x\n", i, value, valueCaps[i].BitSize, valueCaps[i].LogicalMax, maxAxis, valueCaps[i].Range.UsageMin);
			
			myState.axes[valueCaps[i].Range.UsageMin] = ((double)value*2 / maxAxis) - 1;
		}
		free(valueCaps);

		for (auto& b : myState.buttons)
			b.second = false;

		if (logValues) snprintf(logbuf, 1024, "Buttons: \n");

		HIDP_BUTTON_CAPS* buttonCaps = (HIDP_BUTTON_CAPS*)malloc(caps.NumberInputButtonCaps * sizeof(HIDP_BUTTON_CAPS));
		HidP_GetButtonCaps(HidP_Input, buttonCaps, &caps.NumberInputButtonCaps, data);
		for (USHORT i = 0; i < caps.NumberInputButtonCaps; ++i)
		{
			ULONG usageCount = buttonCaps->Range.UsageMax - buttonCaps->Range.UsageMin + 1;
			USAGE* usages = (USAGE*)malloc(sizeof(USAGE) * usageCount);
			HidP_GetUsages(HidP_Input, buttonCaps[i].UsagePage, 0, usages, &usageCount, data, (PCHAR)input.data.hid.bRawData, input.data.hid.dwSizeHid);
			for (ULONG usageIndex = 0; usageIndex < usageCount; ++usageIndex) {
				
				if (logValues) snprintf(logbuf, 1024, "%d : %d \n", i, usages[usageIndex]);
				
				myState.buttons[usages[usageIndex]] = true;
			}
			free(usages);
		}
		free(buttonCaps);

		if (logValues) logToFile(appConfig, logbuf);
	}
	free(data);

}
#endif //_WIN32