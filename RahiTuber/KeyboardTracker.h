#pragma once

#include <Windows.h>
#include <map>
#include <thread>
#include "defines.h"

class LayerManager;

class KeyboardTracker
{
public:
	void SetHook(bool enable);
	bool IsHooked() { return _hookEnabled; }

	void HandleKeystroke(PKBDLLHOOKSTRUCT kbdStruct, bool keyDown);

  void HandleMousePress(int button, bool keyDown);

  LayerManager* _layerMan = nullptr;

private:

	bool _hookEnabled = false;

	HHOOK _hookHandle = NULL;

  HHOOK _mouseHookHandle = NULL;

	std::map<sf::Keyboard::Key, bool> _keysPressed;

  wchar_t GetCharFromKey(int vkCode)
  {
    wchar_t output;
    BYTE keyboardState[256] = {};

    //GetKeyboardState(keyboardState);

    int success = ToUnicode(vkCode, MapVirtualKey(vkCode, MAPVK_VK_TO_VSC), keyboardState, &output, 1, 0);
    return output;
  }
};


