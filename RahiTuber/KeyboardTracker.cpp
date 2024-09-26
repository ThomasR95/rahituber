#include "KeyboardTracker.h"
#include <functional>
#include "LayerManager.h"

std::function<void(PKBDLLHOOKSTRUCT, bool)> keyHandleFnc = nullptr;
std::function<void(int, bool)> mouseHandleFnc = nullptr;

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

LRESULT CALLBACK MouseCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (mouseHandleFnc != nullptr && nCode == HC_ACTION)
  {
    bool keydown = false;
    int button = -1;
    switch (wParam)
    {
    case WM_LBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
      keydown = true;
      __fallthrough;
    case WM_LBUTTONUP:
    case WM_NCLBUTTONUP:
      button = 0;
      break;

    case WM_RBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
      keydown = true;
      __fallthrough;
    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
      button = 1;
      break;

    case WM_MBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
      keydown = true;
      __fallthrough;
    case WM_MBUTTONUP:
    case WM_NCMBUTTONUP:
      button = 2;
      break;

    case WM_XBUTTONDOWN:
    case WM_NCXBUTTONDOWN:
      keydown = true;
      __fallthrough;
    case WM_XBUTTONUP:
    case WM_NCXBUTTONUP:
      MSLLHOOKSTRUCT p = *(MSLLHOOKSTRUCT*)lParam;
      WORD btn = p.mouseData >> 16;
      button = (btn == XBUTTON1) ? 3 : 4;
      break;
    }

    if (button != -1)
    {
      mouseHandleFnc(button, keydown);
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
    }
    _mouseHookHandle = SetWindowsHookEx(WH_MOUSE_LL, MouseCallback, GetModuleHandle(L"kernel32.dll"), 0);
    if (_mouseHookHandle != NULL)
    {
      mouseHandleFnc = std::bind(&KeyboardTracker::HandleMousePress, this, std::placeholders::_1, std::placeholders::_2);
    }

    _hookEnabled = true;
			
	}
	else if (!enable && _hookEnabled)
	{
    bool unhookKey = UnhookWindowsHookEx(_hookHandle);
    if (unhookKey)
    {
      keyHandleFnc = nullptr;
    }
    bool unhookMouse = UnhookWindowsHookEx(_mouseHookHandle);
    if (unhookMouse)
    {
      mouseHandleFnc = nullptr;
    }

    _hookEnabled = false;
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

void KeyboardTracker::HandleMousePress(int button, bool keyDown)
{
  sf::Event evt;

  evt.key.control = _keysPressed[sf::Keyboard::LControl] || _keysPressed[sf::Keyboard::RControl];
  evt.key.shift = _keysPressed[sf::Keyboard::LShift] || _keysPressed[sf::Keyboard::RShift];
  evt.key.alt = _keysPressed[sf::Keyboard::LAlt] || _keysPressed[sf::Keyboard::RAlt];

  if (keyDown)
    evt.type = sf::Event::MouseButtonPressed;
  else
    evt.type = sf::Event::MouseButtonReleased;

  evt.mouseButton.button = (sf::Mouse::Button)button;

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
