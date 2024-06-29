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
  sf::Keyboard::Key keycode = sf::Keyboard::Unknown;

  int vkCode = kbdStruct->vkCode;
  if (g_key_codes.count(vkCode) != 0)
  {
    keycode = g_key_codes[vkCode];
  }
  else
  {
    wchar_t unicodeKey = GetCharFromKey(kbdStruct->vkCode);
    if (unicodeKey)
    {
      if (g_specialkey_codes.count(unicodeKey) != 0)
      {
        keycode = g_specialkey_codes[unicodeKey];
      }
    }
  }

  _keysPressed[keycode] = keyDown;

  bool isCtrl = (keycode == sf::Keyboard::LControl) || (keycode == sf::Keyboard::RControl);
  bool isShift = (keycode == sf::Keyboard::LShift) || (keycode == sf::Keyboard::RShift);
  bool isAlt = (keycode == sf::Keyboard::LAlt) || (keycode == sf::Keyboard::RAlt);

  bool isModifier = isCtrl || isShift || isAlt;

  if (isModifier)
    return;

  sf::Event evt;

  evt.key.control = _keysPressed[sf::Keyboard::LControl] || _keysPressed[sf::Keyboard::RControl];
  evt.key.shift = _keysPressed[sf::Keyboard::LShift] || _keysPressed[sf::Keyboard::RShift];
  evt.key.alt = _keysPressed[sf::Keyboard::LAlt] || _keysPressed[sf::Keyboard::RAlt];

  evt.key.code = keycode;

  if(keyDown)
    evt.type = sf::Event::KeyPressed;
  else
    evt.type = sf::Event::KeyReleased;


  if (_layerMan)
  {
    if (_layerMan->PendingHotkey() && keyDown)
    {
      _layerMan->SetHotkeys(evt);
    }
    else
    {
      _layerMan->HandleHotkey(evt, keyDown);
    }
  }
}
