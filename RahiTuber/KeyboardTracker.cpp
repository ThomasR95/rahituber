#include "KeyboardTracker.h"
#include <functional>
#include "LayerManager.h"

std::function<void(PKBDLLHOOKSTRUCT, bool)> keyHandleFnc = nullptr;

LRESULT CALLBACK KeyboardCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (keyHandleFnc != nullptr && nCode == HC_ACTION)
  {
    switch (wParam)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
      if ((wParam == WM_KEYDOWN) || (wParam == WM_SYSKEYDOWN)) // Keydown
      {
        keyHandleFnc(p, true);
      }
      else if ((wParam == WM_KEYUP) || (wParam == WM_SYSKEYUP)) // Keyup
      {
        keyHandleFnc(p, false);
      }
      break;
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void KeyboardTracker::SetHook(bool enable)
{
	if (enable && !_hookEnabled)
	{
		_hookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardCallback, GetModuleHandle(L"kernel32.dll"), 0);
    if (_hookHandle != NULL)
    {
      keyHandleFnc = std::bind(&KeyboardTracker::HandleKeystroke, this, std::placeholders::_1, std::placeholders::_2);
      _hookEnabled = true;
    }
			
	}
	else if (!enable && _hookEnabled)
	{
    if (UnhookWindowsHookEx(_hookHandle))
    {
      keyHandleFnc = nullptr;
      _hookEnabled = false;
    }
	}
		
}

void KeyboardTracker::HandleKeystroke(PKBDLLHOOKSTRUCT kbdStruct, bool keyDown)
{
  sf::Keyboard::Key pressed = sf::Keyboard::Unknown;

  int vkCode = kbdStruct->vkCode;
  if (g_key_codes.count(vkCode) != 0)
  {
    pressed = g_key_codes[vkCode];
  }
  else
  {
    wchar_t unicodeKey = GetCharFromKey(kbdStruct->vkCode);
    if (unicodeKey)
    {
      if (g_specialkey_codes.count(unicodeKey) != 0)
      {
        pressed = g_specialkey_codes[unicodeKey];
      }
    }
  }

  _keysPressed[pressed] = keyDown;

  bool isCtrl = (pressed == sf::Keyboard::LControl) || (pressed == sf::Keyboard::RControl);
  bool isShift = (pressed == sf::Keyboard::LShift) || (pressed == sf::Keyboard::RShift);
  bool isAlt = (pressed == sf::Keyboard::LAlt) || (pressed == sf::Keyboard::RAlt);

  bool isModifier = isCtrl || isShift || isAlt;

  if (isModifier)
    return;

  bool ctrl = _keysPressed[sf::Keyboard::LControl] || _keysPressed[sf::Keyboard::RControl];
  bool shift = _keysPressed[sf::Keyboard::LShift] || _keysPressed[sf::Keyboard::RShift];
  bool alt = _keysPressed[sf::Keyboard::LAlt] || _keysPressed[sf::Keyboard::RAlt];

  if (keyDown && _layerMan)
  {
    if (_layerMan->PendingHotkey())
    {
      _layerMan->SetHotkeys(pressed, ctrl, shift, alt);
    }
    else
    {
      _layerMan->HandleHotkey(pressed, ctrl, shift, alt);
    }
  }
}
