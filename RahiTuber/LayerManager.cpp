
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2\tinyxml2.h"
#include <sstream>

#include "defines.h"

#include <iostream>

#include <windows.h>

#include <filesystem>
namespace fs = std::filesystem;

//#include <shellapi.h>

void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	if (_activeHotkeyIdx != -1 && _activeHotkeyIdx < _hotkeys.size())
	{
		HotkeyInfo& hkey = _hotkeys[_activeHotkeyIdx];
		if (hkey._useTimeout && _hotkeyTimer.getElapsedTime().asSeconds() > hkey._timeout)
			ResetHotkeys();
	}

	std::vector<LayerInfo*> calculateOrder;
	for (int l = _layers.size()-1; l >= 0; l--)
		calculateOrder.push_back(&_layers[l]);
	
	std::sort(calculateOrder.begin(), calculateOrder.end(), [](const LayerInfo* lhs, const LayerInfo* rhs)
	{
		return (lhs->_id == rhs->_motionParent);
	});

	for (auto layer : calculateOrder)
	{
		if(layer->_visible)
			layer->CalculateDraw(windowHeight, windowWidth, talkLevel, talkMax);
	}
		

	for (int l = _layers.size() - 1; l >= 0; l--)
	{
		LayerInfo& layer = _layers[l];
		if (layer._visible)
		{
			layer._idleSprite.Draw(target);
			layer._talkSprite.Draw(target);
			layer._blinkSprite.Draw(target);
			layer._talkBlinkSprite.Draw(target);
		}
	}
}

void LayerManager::DrawGUI(ImGuiStyle& style, float maxHeight)
{
	float topBarBegin = ImGui::GetCursorPosY();

	ImGui::PushID("layermanager");

	float frameW = ImGui::GetWindowWidth();
	float buttonW = (frameW / 3) - 12;

	if (ImGui::Button("Add Layer", { buttonW, 20 }))
		AddLayer();

	ImGui::SameLine();
	if (ImGui::Button("Remove All", { buttonW, 20 }))
		_layers.clear();

	ImGui::SameLine();
	ImGui::PushID("hotkeysBtn");
	if (ImGui::Button("Hotkeys", { buttonW, 20 }))
		_hotkeysMenuOpen = true;
	ImGui::PopID();

	ImGui::PushID("hotkeysPopup");
	DrawHotkeysGUI();
	ImGui::PopID();

	ImGui::PushItemWidth(200);
	float b4textY = ImGui::GetCursorPosY();
	ImGui::SetCursorPosY(b4textY + 3);
	ImGui::Text("Layer Set:");
	ImGui::SameLine();
	ImGui::SetCursorPosY(b4textY);
	ImGui::PushID("layersXMLInput");
	char inputStr[32] = " ";
	_loadedXML.copy(inputStr, 32);
	if (ImGui::InputText("", inputStr, 32, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll))
	{
		_loadedXML = inputStr;
		auto xmlPath = fs::current_path().append(_loadedXML + ".xml");
		_loadedXMLExists = fs::exists(xmlPath);
	}
	ImGui::PopID();
	ImGui::PopItemWidth();

	ImGui::SameLine();
	float textMargin = ImGui::GetCursorPosX();
	float buttonWidth = 0.5 * (frameW - textMargin) - 10;
	ImGui::PushID("saveXMLBtn");
	if (ImGui::Button(_loadedXMLExists ? "Overwrite" : "Save", { buttonWidth, 20 }) && !_loadedXML.empty())
	{
		SaveLayers(_loadedXML + ".xml");
		auto xmlPath = fs::current_path().append(_loadedXML + ".xml");
		_loadedXMLExists = fs::exists(xmlPath);
	}
	ImGui::PopID();

	if (_loadedXMLExists)
	{
		ImGui::SameLine();
		ImGui::PushID("loadXMLBtn");
		if (ImGui::Button("Load", { buttonWidth, 20 }) && _loadedXMLExists)
			LoadLayers(_loadedXML + ".xml");
		ImGui::PopID();
	}

	ImGui::Separator();

	float topBarHeight = ImGui::GetCursorPosY() - topBarBegin;

	ImGui::BeginChild(ImGuiID(10001), ImVec2(-1, maxHeight - topBarHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	for (int l = 0; l < _layers.size(); l++)
	{
		auto& layer = _layers[l];
		layer.DrawGUI(style, l);
	}

	ImGui::EndChild();

	ImGui::PopID();
}

void LayerManager::AddLayer()
{
	_layers.push_back(LayerInfo());

	LayerInfo& layer = _layers.back();

	layer._blinkTimer.restart();
	layer._isBlinking = false;
	layer._blinkVarDelay = GetRandom11() * layer._blinkVariation;
	layer._parent = this;
	layer._id = time(0);

}

void LayerManager::RemoveLayer(int toRemove)
{
	_layers.erase(_layers.begin() + toRemove);
}

void LayerManager::MoveLayerUp(int moveUp)
{
	if (moveUp <= 0)
		return;

	LayerInfo copy = _layers[moveUp];
	RemoveLayer(moveUp);
	int insertIdx = moveUp - 1;
	_layers.insert(_layers.begin() + insertIdx, copy);
}

void LayerManager::MoveLayerDown(int moveDown)
{
	if (moveDown >= _layers.size() - 1)
		return;

	LayerInfo copy = _layers[moveDown];
	RemoveLayer(moveDown);
	int insertIdx = moveDown + 1;
	_layers.insert(_layers.begin() + insertIdx, copy);
}

void LayerManager::RemoveLayer(LayerInfo* toRemove)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == toRemove)
			RemoveLayer(l);
	}
}

void LayerManager::MoveLayerUp(LayerInfo* moveUp)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == moveUp)
			MoveLayerUp(l);
	}
}

void LayerManager::MoveLayerDown(LayerInfo* moveDown)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == moveDown)
			MoveLayerDown(l);
	}
}

bool LayerManager::SaveLayers(const std::string& settingsFileName)
{
	tinyxml2::XMLDocument doc;

	doc.LoadFile(settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root) 
		root = doc.InsertFirstChild(doc.NewElement("Config"))->ToElement();

	if (!root)
		return false;

	auto layers = root->FirstChildElement("layers");
	if (!layers) 
		layers = root->InsertFirstChild(doc.NewElement("layers"))->ToElement();

	if (!layers)
		return false;

	layers->DeleteChildren();

	ResetHotkeys();
	
	for (int l = 0; l < _layers.size(); l++)
	{
		auto thisLayer = layers->InsertEndChild(doc.NewElement("layer"))->ToElement();

		const auto& layer = _layers[l];

		thisLayer->SetAttribute("id", layer._id);

		thisLayer->SetAttribute("name", layer._name.c_str());
		thisLayer->SetAttribute("visible", layer._visible);

		thisLayer->SetAttribute("talking", layer._swapWhenTalking);
		thisLayer->SetAttribute("talkThreshold", layer._talkThreshold);

		thisLayer->SetAttribute("useBlink", layer._useBlinkFrame);
		thisLayer->SetAttribute("talkBlink", layer._blinkWhileTalking);
		thisLayer->SetAttribute("blinkTime", layer._blinkDelay);
		thisLayer->SetAttribute("blinkDur", layer._blinkDuration);
		thisLayer->SetAttribute("blinkVar", layer._blinkVariation);

		thisLayer->SetAttribute("bounceType", layer._bounceType);
		thisLayer->SetAttribute("bounceHeight", layer._bounceHeight);
		thisLayer->SetAttribute("bounceTime", layer._bounceFrequency);

		thisLayer->SetAttribute("breathing", layer._doBreathing);
		thisLayer->SetAttribute("breathHeight", layer._breathHeight);
		thisLayer->SetAttribute("breathTime", layer._breathFrequency);

		thisLayer->SetAttribute("idlePath", layer._idleImagePath.c_str());
		thisLayer->SetAttribute("talkPath", layer._talkImagePath.c_str());
		thisLayer->SetAttribute("blinkPath", layer._blinkImagePath.c_str());
		thisLayer->SetAttribute("talkBlinkPath", layer._talkBlinkImagePath.c_str());

		if (layer._idleSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "idleAnim", layer._idleSprite);

		if (layer._talkSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "talkAnim", layer._talkSprite);

		if (layer._blinkSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "blinkAnim", layer._blinkSprite);

		if (layer._talkBlinkSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "talkBlinkAnim", layer._talkBlinkSprite);

		thisLayer->SetAttribute("syncAnims", layer._animsSynced);

		SaveColor(thisLayer, &doc, "idleTint", layer._idleTint);
		SaveColor(thisLayer, &doc, "talkTint", layer._talkTint);
		SaveColor(thisLayer, &doc, "blinkTint", layer._blinkTint);
		SaveColor(thisLayer, &doc, "talkBlinkTint", layer._talkBlinkTint);

		thisLayer->SetAttribute("scaleX", layer._scale.x);
		thisLayer->SetAttribute("scaleY", layer._scale.y);
		thisLayer->SetAttribute("posX", layer._pos.x);
		thisLayer->SetAttribute("posY", layer._pos.y);
		thisLayer->SetAttribute("rot", layer._rot);

		thisLayer->SetAttribute("motionParent", layer._motionParent);
		thisLayer->SetAttribute("motionDelay", layer._motionDelay);
	}

	auto hotkeys = root->FirstChildElement("hotkeys");
	if (!hotkeys)
		hotkeys = root->InsertFirstChild(doc.NewElement("hotkeys"))->ToElement();

	if (!hotkeys)
		return false;

	hotkeys->DeleteChildren();

	for (int h = 0; h < _hotkeys.size(); h++)
	{
		auto thisHotkey = hotkeys->InsertEndChild(doc.NewElement("hotkey"))->ToElement();
		const auto& hkey = _hotkeys[h];

		thisHotkey->SetAttribute("key", (int)hkey._key);
		thisHotkey->SetAttribute("mod", (int)hkey._modifier);
		thisHotkey->SetAttribute("timeout", hkey._timeout);
		thisHotkey->SetAttribute("useTimeout", hkey._useTimeout);
		thisHotkey->SetAttribute("toggle", hkey._toggle);

		for (auto& state : hkey._layerStates)
		{
			auto thisState = thisHotkey->InsertEndChild(doc.NewElement("state"))->ToElement();
			thisState->SetAttribute("id", state.first);
			thisState->SetAttribute("visible", state.second);
		}
	}


	doc.SaveFile(settingsFileName.c_str());
	_lastSavedLocation = settingsFileName;

	return true;
}

bool LayerManager::LoadLayers(const std::string& settingsFileName)
{
	tinyxml2::XMLDocument doc;

	doc.LoadFile(settingsFileName.c_str());

	auto root = doc.FirstChildElement("Config");
	if (!root)
		return false;

	auto layers = root->FirstChildElement("layers");
	if (!layers)
		return false;

	_layers.clear();

	_lastSavedLocation = settingsFileName;

	auto thisLayer = layers->FirstChildElement("layer");
	int layerCount = 0;
	while (thisLayer)
	{
		layerCount++;
		_layers.emplace_back(LayerInfo());

		LayerInfo& layer = _layers.back();

		layer._parent = this;

		thisLayer->QueryAttribute("id", &layer._id);
		if (layer._id == 0)
			layer._id = time(0) + layerCount;

		const char* name = thisLayer->Attribute("name");
		if (!name)
			break;

		layer._name = name;
		thisLayer->QueryAttribute("visible", &layer._visible);

		thisLayer->QueryAttribute("talking", &layer._swapWhenTalking);
		thisLayer->QueryAttribute("talkThreshold", &layer._talkThreshold);

		thisLayer->QueryAttribute("useBlink", &layer._useBlinkFrame);
		thisLayer->QueryAttribute("talkBlink", &layer._blinkWhileTalking);
		thisLayer->QueryAttribute("blinkTime", &layer._blinkDelay);
		thisLayer->QueryAttribute("blinkDur", &layer._blinkDuration);
		thisLayer->QueryAttribute("blinkVar", &layer._blinkVariation);

		int bobtype = 0;
		thisLayer->QueryIntAttribute("bounceType", &bobtype);
		layer._bounceType = (LayerInfo::BounceType)bobtype;
		thisLayer->QueryAttribute("bounceHeight", &layer._bounceHeight);
		thisLayer->QueryAttribute("bounceTime", &layer._bounceFrequency);

		thisLayer->QueryAttribute("breathing", &layer._doBreathing);
		thisLayer->QueryAttribute("breathHeight", &layer._breathHeight);
		thisLayer->QueryAttribute("breathTime", &layer._breathFrequency);

		if(const char* idlePth = thisLayer->Attribute("idlePath"))
			layer._idleImagePath = idlePth;
		if (const char* talkPth = thisLayer->Attribute("talkPath"))
			layer._talkImagePath = talkPth;
		if (const char* blkPth = thisLayer->Attribute("blinkPath"))
			layer._blinkImagePath = blkPth;
		if (const char* talkBlkPth = thisLayer->Attribute("talkBlinkPath"))
			layer._talkBlinkImagePath = talkBlkPth;

		layer._idleImage = _textureMan.GetTexture(layer._idleImagePath);
		layer._talkImage = _textureMan.GetTexture(layer._talkImagePath);
		layer._blinkImage = _textureMan.GetTexture(layer._blinkImagePath);
		layer._talkBlinkImage = _textureMan.GetTexture(layer._talkBlinkImagePath);

		if(layer._idleImage)
			layer._idleSprite.LoadFromTexture(*layer._idleImage, 1, 1, 1, 1);
		if (layer._talkImage)
			layer._talkSprite.LoadFromTexture(*layer._talkImage, 1, 1, 1, 1);
		if(layer._blinkImage)
			layer._blinkSprite.LoadFromTexture(*layer._blinkImage, 1, 1, 1, 1);
		if (layer._talkBlinkImage)
			layer._talkBlinkSprite.LoadFromTexture(*layer._talkBlinkImage, 1, 1, 1, 1);

		LoadAnimInfo(thisLayer, &doc, "idleAnim", layer._idleSprite);
		LoadAnimInfo(thisLayer, &doc, "talkAnim", layer._talkSprite);
		LoadAnimInfo(thisLayer, &doc, "blinkAnim", layer._blinkSprite);
		LoadAnimInfo(thisLayer, &doc, "talkBlinkAnim", layer._talkBlinkSprite);

		thisLayer->QueryAttribute("syncAnims", &layer._animsSynced);

		if (layer._animsSynced)
			layer.SyncAnims(layer._animsSynced);

		LoadColor(thisLayer, &doc, "idleTint", layer._idleTint);
		LoadColor(thisLayer, &doc, "talkTint", layer._talkTint);
		LoadColor(thisLayer, &doc, "blinkTint", layer._blinkTint);
		LoadColor(thisLayer, &doc, "talkBlinkTint", layer._talkBlinkTint);

		layer._blinkTimer.restart();
		layer._isBlinking = false;
		layer._blinkVarDelay = GetRandom11() * layer._blinkVariation;

		thisLayer->QueryAttribute("scaleX", &layer._scale.x);
		thisLayer->QueryAttribute("scaleY", &layer._scale.y);
		thisLayer->QueryAttribute("posX", &layer._pos.x);
		thisLayer->QueryAttribute("posY", &layer._pos.y);
		thisLayer->QueryAttribute("rot", &layer._rot);

		thisLayer->QueryAttribute("motionParent", &layer._motionParent);
		thisLayer->QueryAttribute("motionDelay", &layer._motionDelay);
		thisLayer = thisLayer->NextSiblingElement("layer");

	}

	auto hotkeys = root->FirstChildElement("hotkeys");
	if (!hotkeys)
		return false;

	_hotkeys.clear();

	auto thisHotkey = hotkeys->FirstChildElement("hotkey");
	while (thisHotkey)
	{
		_hotkeys.emplace_back(HotkeyInfo());
		HotkeyInfo& hkey = _hotkeys.back();

		int key;
		int mod;
		thisHotkey->QueryAttribute("key", &key);
		hkey._key = (sf::Keyboard::Key)key;
		thisHotkey->QueryAttribute("mod", &mod);
		hkey._modifier = (sf::Keyboard::Key)mod;
		thisHotkey->QueryAttribute("timeout", &hkey._timeout);
		thisHotkey->QueryAttribute("useTimeout", &hkey._useTimeout);
		thisHotkey->QueryAttribute("toggle", &hkey._toggle);

		auto thisLayerState = thisHotkey->FirstChildElement("state");
		while (thisLayerState)
		{
			int id;
			bool vis;
			thisLayerState->QueryAttribute("id", &id);
			thisLayerState->QueryAttribute("visible", &vis);

			hkey._layerStates[id] = vis;

			thisLayerState = thisLayerState->NextSiblingElement("state");
		}

		thisHotkey = thisHotkey->NextSiblingElement("hotkey");
	}


	return true;
}

void LayerManager::HandleHotkey(const sf::Keyboard::Key& key, const sf::Keyboard::Key& mod)
{
	for (auto& l : _layers)
		if (l._renamePopupOpen)
			return;

	for (int h = 0; h < _hotkeys.size(); h++)
	{
		auto& hkey = _hotkeys[h];
		if (hkey._key == key && hkey._modifier == mod)
		{
			if (_activeHotkeyIdx == h && hkey._toggle)
			{
					ResetHotkeys();
			}
			else if (_activeHotkeyIdx == -1)
			{
				_defaultLayerStates.clear();
				for (auto& l : _layers)
				{
					_defaultLayerStates[l._id] = l._visible;
				}

				for (auto& state : hkey._layerStates)
				{
					GetLayer(state.first)->_visible = state.second;
				}
				_activeHotkeyIdx = h;
				_hotkeyTimer.restart();
			}
			break;
		}
	}

	return;
}

void LayerManager::ResetHotkeys()
{
	if (_activeHotkeyIdx == -1)
		return;

	for (auto& l : _layers)
	{
		if(_defaultLayerStates.count(l._id))
			l._visible = _defaultLayerStates[l._id];
	}
	_activeHotkeyIdx = -1;
}

void LayerManager::DrawHotkeysGUI()
{
	if (_hotkeysMenuOpen != _oldHotkeysMenuOpen)
	{
		_oldHotkeysMenuOpen = _hotkeysMenuOpen;

		if (_hotkeysMenuOpen)
		{
			ImGui::SetNextWindowPosCenter();
			ImGui::SetNextWindowSize({ 400, 400 });
			ImGui::OpenPopup("Hotkey Setup");
		}
	}

	if (ImGui::BeginPopupModal("Hotkey Setup", &_hotkeysMenuOpen, ImGuiWindowFlags_NoResize))
	{
		ImGui::TextWrapped("Use Hotkeys to instantly set the visibility of multiple layers");

		if (ImGui::Button("Add"))
		{
			_hotkeys.push_back(HotkeyInfo());
			for (auto& l : _layers)
			{
				_hotkeys.back()._layerStates[l._id] = l._visible;
			}
		}

		int hkeyIdx = 0;
		while (hkeyIdx < _hotkeys.size())
		{
			auto& hkeys = _hotkeys[hkeyIdx];
			ImGui::PushID(hkeyIdx);

			std::string name = g_key_names[hkeys._key];
			if (hkeys._key == sf::Keyboard::Unknown)
				name = "Not set";
			if(hkeys._modifier != sf::Keyboard::Unknown)
				name = g_key_names[hkeys._modifier] + "," + g_key_names[hkeys._key];
			ImVec2 delButtonPos = { ImGui::GetCursorPosX() + 330, ImGui::GetCursorPosY() };
			if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ImGui::Columns(3, 0, false);
				ImGui::SetColumnWidth(0, 150);
				ImGui::SetColumnWidth(1, 100);
				ImGui::SetColumnWidth(2, 300);
				std::string btnName = name;
				if (hkeys._key == sf::Keyboard::Unknown)
					btnName = " Click to\nrecord key";
				if (_waitingForHotkey) 
					btnName = "(press a key)";
				ImGui::PushID("recordKeyBtn");
				if (ImGui::Button(btnName.c_str(), { 140,42 }))
				{
					_pendingKey = sf::Keyboard::Unknown;
					_pendingMod = sf::Keyboard::Unknown;
					_waitingForHotkey = true;
				}
				ImGui::PopID();

				if (_waitingForHotkey && _pendingKey != sf::Keyboard::Unknown)
				{
					hkeys._key = _pendingKey;
					hkeys._modifier = _pendingMod;
					_waitingForHotkey = false;
				}

				ImGui::NextColumn();

				ImGui::Checkbox("Toggle", &hkeys._toggle);
				ImGui::Checkbox("Timeout", &hkeys._useTimeout);

				ImGui::NextColumn();

				ImGui::SameLine(10);
				if (hkeys._useTimeout)
				{
					ImGui::PushID("timeoutSlider");
					ImGui::SliderFloat("", &hkeys._timeout, 0.0, 30.0, "%.1f s", 2.f);
					ImGui::PopID();
				}

				ImGui::Columns();

				ImGui::Separator();

				for (auto& l : _layers)
				{
					ImGui::Checkbox(l._name.c_str(), &hkeys._layerStates[l._id]);
				}
			}
			ImVec2 endHeaderPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(delButtonPos);
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
			ImGui::PushStyleColor(ImGuiCol_Text, { 1,1,1,1 });
			if (ImGui::Button("Delete"))
			{
				_hotkeys.erase(_hotkeys.begin() + hkeyIdx);
			}
			ImGui::PopStyleColor(4);
			ImGui::PopID();

			ImGui::SetCursorPos(endHeaderPos);

			if (hkeyIdx >= _hotkeys.size())
				break;

			hkeyIdx++;
		}

		ImGui::EndPopup();
	}
}

void LayerManager::LayerInfo::CalculateDraw(float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	_activeSprite = nullptr;

	if (!_idleImage)
		return;
	
	_idleSprite._visible = true;
	_blinkSprite._visible = false;
	_talkSprite._visible = false;
	_talkBlinkSprite._visible = false;

	_activeSprite = &_idleSprite;
	_idleSprite.SetColor(_idleTint);

	float talkFactor = talkLevel / talkMax;
	talkFactor = pow(talkFactor, 0.5);

	bool talking = talkFactor > _talkThreshold;

	bool canStartBlinking = (_blinkWhileTalking || !talking) && !_isBlinking;

	if (canStartBlinking && _blinkTimer.getElapsedTime().asSeconds() > _blinkDelay + _blinkVarDelay)
	{
		_isBlinking = true;
		_blinkTimer.restart();
		_blinkSprite.Restart();
		_blinkVarDelay = GetRandom11() * _blinkVariation;
	}

	if (_isBlinking)
	{
		_talkSprite._visible = false;
		_idleSprite._visible = false;
		if (talking && _blinkWhileTalking && _talkBlinkImage)
		{
			_activeSprite = &_talkBlinkSprite;
			_talkBlinkSprite._visible = true;
			_blinkSprite._visible = false;
			_talkBlinkSprite.SetColor(_talkBlinkTint);
		}
		else if(_blinkImage)
		{
			_activeSprite = &_blinkSprite;
			_blinkSprite._visible = true;
			_talkBlinkSprite._visible = false;
			_blinkSprite.SetColor(_blinkTint);
		}

		if (_blinkTimer.getElapsedTime().asSeconds() > _blinkDuration)
			_isBlinking = false;
	}

	if (_talkImage && !_isBlinking && _swapWhenTalking && talking)
	{
		_activeSprite = &_talkSprite;
		_idleSprite._visible = false;
		_blinkSprite._visible = false;
		_talkSprite._visible = true;
		_talkSprite.SetColor(_talkTint);
	}

	if (_motionParent == -1)
	{

		sf::Vector2f pos;

		float newMotionHeight = 0;

		switch (_bounceType)
		{
		case LayerManager::LayerInfo::BounceNone:
			break;
		case LayerManager::LayerInfo::BounceLoudness:
			_isBouncing = false;
			if (talking)
			{
				_isBouncing = true;
				newMotionHeight += _bounceHeight * std::fmax(0.f, (talkFactor - _talkThreshold) / (1.0 - _talkThreshold));
			}
			break;
		case LayerManager::LayerInfo::BounceRegular:
			if (talking && _bounceFrequency > 0)
			{
				if (!_isBouncing)
				{
					_isBouncing = true;
					_motionTimer.restart();
				}

				float motionTime = _motionTimer.getElapsedTime().asSeconds();
				motionTime -= floor(motionTime / _bounceFrequency) * _bounceFrequency;
				float phase = (motionTime / _bounceFrequency) * 2.0 * PI;
				newMotionHeight = (-0.5 * cos(phase) + 0.5) * _bounceHeight;
			}
			else
				_isBouncing = false;

			break;
		default:
			break;
		}

		float breathScale = 1.0;

		if (_doBreathing)
		{
			_breathAmount *= 0.95;

			bool talkActive = talking && _swapWhenTalking;

			if ( !talkActive && !_isBouncing && _breathFrequency > 0)
			{
				if (!_isBreathing)
				{
					_motionTimer.restart();
					_isBreathing = true;
				}

				float motionTime = _motionTimer.getElapsedTime().asSeconds();
				motionTime -= floor(motionTime / _breathFrequency) * _breathFrequency;
				float phase = (motionTime / _breathFrequency) * 2.0 * PI;
				_breathAmount = (-0.5 * cos(phase) + 0.5);
			}
			else
			{
				_isBreathing = false;
			}

			// breathing is half movement, half scale
			newMotionHeight += _breathAmount * (_breathHeight / 2);

			float origHeight = _activeSprite->Size().y * _scale.y;
			float breathHeight = origHeight + _breathAmount * (_breathHeight / 2);

			breathScale = breathHeight / origHeight;
		}

		_motionHeight += (newMotionHeight - _motionHeight) * 0.3;

		pos.y -= _motionHeight;

		_activeSprite->setOrigin({ 0.5f * _activeSprite->Size().x, 0.5f * _activeSprite->Size().y });
		_activeSprite->setScale(_scale * breathScale);
		_activeSprite->setPosition({ windowWidth / 2 + _pos.x + pos.x, windowHeight / 2 + _pos.y + pos.y });
		_activeSprite->setRotation(_rot);

		MotionLinkData thisFrame;
		thisFrame._pos = pos;
		thisFrame._scale = { breathScale, breathScale };
		thisFrame._rot = _rot;
		_motionLinkData.push_front(thisFrame);
		if (_motionLinkData.size() > 11)
			_motionLinkData.pop_back();
	}
	else
	{
		LayerInfo* mp = _parent->GetLayer(_motionParent);
		if (mp)
		{
			if (_motionDelay < 0) 
				_motionDelay = 0;

			size_t maxDelay = _motionLinkData.size() - 1;
			if (_motionDelay > maxDelay)
				_motionDelay = maxDelay;

			sf::Vector2f mpScale;
			sf::Vector2f mpPos;
			float mpRot = 0;
			if (floor(_motionDelay) == ceil(_motionDelay))
			{
				mpScale = mp->_motionLinkData[(size_t)_motionDelay]._scale;
				mpPos = mp->_motionLinkData[(size_t)_motionDelay]._pos;
				mpRot = mp->_motionLinkData[(size_t)_motionDelay]._rot;
			}
			else
			{
				size_t prev = floor(_motionDelay);
				size_t next = ceil(_motionDelay);
				float fraction = _motionDelay - prev;
				mpScale = mp->_motionLinkData[prev]._scale + fraction*(mp->_motionLinkData[next]._scale - mp->_motionLinkData[prev]._scale);
				mpPos = mp->_motionLinkData[prev]._pos + fraction * (mp->_motionLinkData[next]._pos - mp->_motionLinkData[prev]._pos);
				mpRot = mp->_motionLinkData[prev]._rot + fraction * (mp->_motionLinkData[next]._rot - mp->_motionLinkData[prev]._rot);
			}
			
			_activeSprite->setOrigin({ 0.5f * _activeSprite->Size().x, 0.5f * _activeSprite->Size().y });
			_activeSprite->setScale({ _scale.x * mpScale.x, _scale.y * mpScale.y });
			_activeSprite->setPosition({ windowWidth / 2 + _pos.x + mpPos.x, windowHeight / 2 + _pos.y + mpPos.y });
			_activeSprite->setRotation(_rot + mpRot);
		}
	}

}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{

	if (_animIcon == nullptr)
		_animIcon = _textureMan.GetTexture("anim.png");

	if (_emptyIcon == nullptr)
		_emptyIcon = _textureMan.GetTexture("empty.png");

	if (_upIcon == nullptr)
		_upIcon = _textureMan.GetTexture("arrowup.png");

	if (_dnIcon == nullptr)
		_dnIcon = _textureMan.GetTexture("arrowdn.png");

	sf::Color btnColor = style.Colors[ImGuiCol_Text];

	ImGui::PushID(_id);
	std::string name = "[" + std::to_string(layerID) + "] " + _name;
	ImVec2 headerButtonsPos = { ImGui::GetCursorPosX() + 240, ImGui::GetCursorPosY() };

	if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
	{
		static imgui_ext::file_browser_modal fileBrowserIdle("Import Idle Sprite");
		static imgui_ext::file_browser_modal fileBrowserTalk("Import Talk Sprite");
		static imgui_ext::file_browser_modal fileBrowserBlink("Import Blink Sprite");

		ImGui::Columns(3, "imagebuttons", false);

		ImGui::TextColored(style.Colors[ImGuiCol_Text], "Idle");
		ImGui::PushID("idleimport");

		sf::Color idleCol = _idleImage == nullptr ? btnColor : sf::Color::White;
		sf::Texture* idleIcon = _idleImage == nullptr ? _emptyIcon : _idleImage;
		_importIdleOpen = ImGui::ImageButton(*idleIcon, { 100,100 }, -1, sf::Color::Transparent, idleCol);
		if (fileBrowserIdle.render(_importIdleOpen, _idleImagePath))
		{
			if (_idleImage == nullptr)
				_idleImage = new sf::Texture();
			_idleImage = _textureMan.GetTexture(_idleImagePath);
			_idleSprite.LoadFromTexture(*_idleImage, 1, 1, 1, 1);
		}

		ImGui::SameLine(116);
		ImGui::PushID("idleanimbtn");
		_spriteIdleOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
		AnimPopup(_idleSprite, _spriteIdleOpen, _oldSpriteIdleOpen);
		ImGui::PopID();

		fs::path chosenDir = fileBrowserIdle.GetLastChosenDir();

		ImGui::PushID("idleimportfile");
		char idlebuf[256] = "                           ";
		_idleImagePath.copy(idlebuf, 256);
		if (ImGui::InputText("", idlebuf, 256, ImGuiInputTextFlags_AutoSelectAll))
		{
			_idleImagePath = idlebuf;
		}
		ImGui::PopID();

		ImGui::ColorEdit4("Tint", _idleTint, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_NoInputs);

		ImGui::PopID();

		ImGui::NextColumn();

		if (_swapWhenTalking)
		{
			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk");
			ImGui::PushID("talkimport");
			sf::Color talkCol = _talkImage == nullptr ? btnColor : sf::Color::White;
			sf::Texture* talkIcon = _talkImage == nullptr ? _emptyIcon : _talkImage;
			_importTalkOpen = ImGui::ImageButton(*talkIcon, { 100,100 }, -1, sf::Color::Transparent, talkCol);
			fileBrowserTalk.SetStartingDir(chosenDir);
			if (fileBrowserTalk.render(_importTalkOpen, _talkImagePath))
			{
				if (_talkImage == nullptr)
					_talkImage = new sf::Texture();
				_talkImage = _textureMan.GetTexture(_talkImagePath);
				_talkSprite.LoadFromTexture(*_talkImage, 1, 1, 1, 1);
			}
			
			ImGui::SameLine(116);
			ImGui::PushID("talkanimbtn");
			_spriteTalkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			AnimPopup(_talkSprite, _spriteTalkOpen, _oldSpriteTalkOpen);
			ImGui::PopID();

			ImGui::PushID("talkimportfile");
			char talkbuf[256] = "                           ";
			_talkImagePath.copy(talkbuf, 256);
			if (ImGui::InputText("", talkbuf, 256, ImGuiInputTextFlags_AutoSelectAll))
			{
				_talkImagePath = talkbuf;
			}
			ImGui::PopID();

			ImGui::ColorEdit4("Tint", _talkTint, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_NoInputs);

			ImGui::PopID();
		}

		ImGui::NextColumn();
		
		if (_useBlinkFrame)
		{
			ImVec2 blinkBtnSize = _blinkWhileTalking ? ImVec2(50, 50) : ImVec2(100, 100);

			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Blink");
			ImGui::PushID("blinkimport");
			sf::Color blinkCol = _blinkImage == nullptr ? btnColor : sf::Color::White;
			sf::Texture* blinkIcon = _blinkImage == nullptr ? _emptyIcon : _blinkImage;
			_importBlinkOpen = ImGui::ImageButton(*blinkIcon, blinkBtnSize, -1, sf::Color::Transparent, blinkCol);
			fileBrowserBlink.SetStartingDir(chosenDir);
			if (fileBrowserBlink.render(_importBlinkOpen, _blinkImagePath))
			{
				if (_blinkImage == nullptr)
					_blinkImage = new sf::Texture();
				_blinkImage = _textureMan.GetTexture(_blinkImagePath);
				_blinkSprite.LoadFromTexture(*_blinkImage, 1, 1, 1, 1);
			}

			ImGui::SameLine(116);
			ImGui::PushID("blinkanimbtn");
			_spriteBlinkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			AnimPopup(_blinkSprite, _spriteBlinkOpen, _oldSpriteBlinkOpen);
			ImGui::PopID();
			
			ImGui::PushID("blinkimportfile");
			char blinkbuf[256] = "                           ";
			_blinkImagePath.copy(blinkbuf, 256);
			if (ImGui::InputText("", blinkbuf, 256, ImGuiInputTextFlags_AutoSelectAll))
			{
				_blinkImagePath = blinkbuf;
			}
			ImGui::PopID();

			ImGui::ColorEdit4("Tint", _blinkTint, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_NoInputs);

			ImGui::PopID();

			if (_blinkWhileTalking)
			{
				ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk Blink");
				ImGui::PushID("talkblinkimport");
				sf::Color talkblinkCol = _talkBlinkImage == nullptr ? btnColor : sf::Color::White;
				sf::Texture* talkblinkIcon = _talkBlinkImage == nullptr ? _emptyIcon : _talkBlinkImage;
				_importTalkBlinkOpen = ImGui::ImageButton(*talkblinkIcon, blinkBtnSize, -1, sf::Color::Transparent, talkblinkCol);
				fileBrowserBlink.SetStartingDir(chosenDir);
				if (fileBrowserBlink.render(_importTalkBlinkOpen, _talkBlinkImagePath))
				{
					if (_talkBlinkImage == nullptr)
						_talkBlinkImage = new sf::Texture();
					_talkBlinkImage = _textureMan.GetTexture(_talkBlinkImagePath);
					_talkBlinkSprite.LoadFromTexture(*_talkBlinkImage, 1, 1, 1, 1);
				}

				ImGui::SameLine(116);
				ImGui::PushID("talkblinkanimbtn");
				_spriteTalkBlinkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
				AnimPopup(_talkBlinkSprite, _spriteTalkBlinkOpen, _oldSpriteTalkBlinkOpen);
				ImGui::PopID();

				ImGui::PushID("talkblinkimportfile");
				char talkblinkbuf[256] = "                           ";
				_talkBlinkImagePath.copy(talkblinkbuf, 256);
				if (ImGui::InputText("", talkblinkbuf, 256, ImGuiInputTextFlags_AutoSelectAll))
				{
					_talkBlinkImagePath = talkblinkbuf;
				}
				ImGui::PopID();

				ImGui::ColorEdit4("Tint", _talkBlinkTint, ImGuiColorEditFlags_RGB | ImGuiColorEditFlags_NoInputs);

				ImGui::PopID();


			}
			
		}
		
		ImGui::Columns();

		ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);
		if (_swapWhenTalking)
		{
			AddResetButton("talkThresh", _talkThreshold, 0.15f, &style);
			ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0, "%.3f", 2.f);
		}
		ImGui::Separator();

		ImGui::Checkbox("Blinking", &_useBlinkFrame);
		if (_useBlinkFrame)
		{
			ImGui::Checkbox("Blink While Talking", &_blinkWhileTalking);
			AddResetButton("blinkdur", _blinkDuration, 0.2f, &style);
			ImGui::SliderFloat("Blink Duration", &_blinkDuration, 0.0, 10.0, "%.2f s");
			AddResetButton("blinkdelay", _blinkDelay, 6.f, &style);
			ImGui::SliderFloat("Blink Delay", &_blinkDelay, 0.0, 10.0, "%.2f s");
			AddResetButton("blinkvar", _blinkVariation, 4.f, &style);
			ImGui::SliderFloat("Variation", &_blinkVariation, 0.0, 5.0, "%.2f s");
		}
		ImGui::Separator();

		LayerInfo* oldMp = _parent->GetLayer(_motionParent);
		std::string mpName = oldMp ? oldMp->_name : "Off";
		if (ImGui::BeginCombo("Motion Inherit", mpName.c_str()))
		{
			if (ImGui::Selectable("Off", _motionParent == -1))
				_motionParent = -1;
			for (auto& layer : _parent->GetLayers())
			{
				if (layer._id != _id && layer._motionParent != _id)
					if (ImGui::Selectable(layer._name.c_str(), _motionParent == layer._id))
					{
						_motionParent = layer._id;
					}
			}
			ImGui::EndCombo();
		}

		if (_motionParent != -1)
		{
			ImGui::SliderFloat("Motion Delay", &_motionDelay, 0.0, 10.0, "%.1f", 2.f);
		}

		ImGui::Separator();

		if (_motionParent == -1)
		{
			std::vector<const char*> bobOptions = { "None", "Loudness", "Regular" };
			if (ImGui::BeginCombo("Bouncing", bobOptions[_bounceType]))
			{
				if (ImGui::Selectable("None", _bounceType == BounceNone))
					_bounceType = BounceNone;
				if (ImGui::Selectable("Loudness", _bounceType == BounceLoudness))
					_bounceType = BounceLoudness;
				if (ImGui::Selectable("Regular", _bounceType == BounceRegular))
					_bounceType = BounceRegular;
				ImGui::EndCombo();
			}
			if (_bounceType != BounceNone)
			{
				AddResetButton("bobheight", _bounceHeight, 80.f, &style);
				ImGui::SliderFloat("Bounce height", &_bounceHeight, 0.0, 500.0);
				if (_bounceType == BounceRegular)
				{
					AddResetButton("bobtime", _bounceFrequency, 0.333f, &style);
					ImGui::SliderFloat("Bounce time", &_bounceFrequency, 0.0, 2.0, "%.2f s");
				}
			}
			ImGui::Separator();

			ImGui::Checkbox("Breathing", &_doBreathing);
			if (_doBreathing)
			{
				AddResetButton("breathheight", _breathHeight, 30.f, &style);
				ImGui::SliderFloat("Breath Height", &_breathHeight, 0.0, 500.0);
				AddResetButton("breathfreq", _breathFrequency, 4.f, &style);
				ImGui::SliderFloat("Breath Time", &_breathFrequency, 0.0, 10.f, "%.2f s");
			}
			ImGui::Separator();
		}

		AddResetButton("pos", _pos, sf::Vector2f(0.0, 0.0), &style);
		float pos[2] = { _pos.x, _pos.y };
		if (ImGui::SliderFloat2("Position", pos, -1000.0, 1000.f))
		{
			_pos.x = pos[0];
			_pos.y = pos[1];
		}

		AddResetButton("rot", _rot, 0.f, &style);
		ImGui::SliderFloat("Rotation", &_rot, -180.f, 180.f);

		AddResetButton("scale", _scale, sf::Vector2f(1.0, 1.0), &style);
		float scale[2] = {_scale.x, _scale.y};
		if (ImGui::SliderFloat2("Scale", scale, 0.0, 5.f))
		{
			if (!_keepAspect)
			{
				_scale.x = scale[0];
				_scale.y = scale[1];
			}
			else if (scale[0] != _scale.x)
			{
				_scale = { scale[0] , scale[0] };
			}
			else if (scale[1] != _scale.y)
			{
				_scale = { scale[1] , scale[1] };
			}
		}
		ImGui::Checkbox("Constrain", &_keepAspect);

		ImGui::Separator();
	}

	auto oldCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(headerButtonsPos);

	ImGui::PushID("visible");
	ImGui::Checkbox("", &_visible);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("upbtn");
	if (ImGui::ImageButton(*_upIcon, {16,16}, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerUp(this);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("dnbtn");
	if (ImGui::ImageButton(*_dnIcon, {16, 16}, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerDown(this);
	ImGui::PopID();
	ImGui::SameLine();
	if (ImGui::Button("Rename"))
	{
		_renamingString = _name;
		_renamePopupOpen = true;
		ImGui::SetNextWindowSize({ 200,60 });
		ImGui::OpenPopup("Rename Layer");
	}
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
	ImGui::PushStyleColor(ImGuiCol_Text, { 1,1,1,1 });
	if (ImGui::Button("Delete"))
		_parent->RemoveLayer(this);
	ImGui::PopStyleColor(4);

	if (ImGui::BeginPopupModal("Rename Layer", &_renamePopupOpen, ImGuiWindowFlags_NoResize))
	{
		char inputStr[32] = " ";
		_renamingString.copy(inputStr, 32);
		if (ImGui::InputText("", inputStr, 32, ImGuiInputTextFlags_AutoSelectAll))
		{
			_renamingString = inputStr;
		}
		ImGui::SameLine();

		if (ImGui::Button("Save"))
		{
			_renamePopupOpen = false;
			_name = _renamingString;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}


	ImGui::SetCursorPos(oldCursorPos);

	ImGui::PopID();
}

void LayerManager::LayerInfo::AnimPopup(SpriteSheet& anim, bool& open, bool& oldOpen)
{
	if (open != oldOpen)
	{
		oldOpen = open;

		if (open)
		{
			ImGui::SetNextWindowPosCenter();
			ImGui::SetNextWindowSize({ 400, 240 });
			ImGui::OpenPopup("Sprite Sheet Setup");

			auto gridSize = anim.GridSize();
			_animGrid = { gridSize.x, gridSize.y };
			_animFCount = anim.FrameCount();
			_animFPS = 12;
			_animFrameSize = { anim.Size().x, anim.Size().y };
		}
	}

	if(ImGui::BeginPopupModal("Sprite Sheet Setup", &open))
	{
		ImGui::Columns(2, 0, false);
		ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
		ImGui::TextWrapped("If you need help creating a sprite sheet, here's a free tool (Use Padding 0):");
		ImGui::PopStyleColor();
		ImGui::NextColumn();
		if (ImGui::Button("Leshy SpriteSheet\nTool (web link)"))
		{
			OsOpenInShell("https://www.leshylabs.com/apps/sstool/");
		}
		ImGui::Columns();

		ImGui::Separator();

		ImGui::PushItemWidth(120);

		AddResetButton("gridreset", _animGrid, {anim.GridSize().x, anim.GridSize().y});
		if (ImGui::InputInt2("Sheet Columns/Rows", _animGrid.data()))
		{
			if (_animGrid[0] != anim.GridSize().x || _animGrid[1] != anim.GridSize().y)
			{
				_animFCount = _animGrid[0] * _animGrid[1];
				_animFrameSize = { -1,-1 };
			}
		}

		AddResetButton("fcountreset", _animFCount, anim.FrameCount());
		ImGui::InputInt("Frame Count", &_animFCount, 0, 0);

		AddResetButton("fpsreset", _animFPS, anim.FPS(), nullptr, !anim.IsSynced());
		if(!anim.IsSynced())
			ImGui::InputFloat("FPS", &_animFPS, 1,1,1);
		else
		{
			std::stringstream ss;
			ss << _animFPS;
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
			ImGui::InputText("FPS", ss.str().data(), ss.str().length(), ImGuiInputTextFlags_ReadOnly );
			ImGui::PopStyleColor();
		}

		AddResetButton("framereset", _animFrameSize, { -1, -1 });
		ImGui::InputInt2("Frame Size (auto = [-1,-1])", _animFrameSize.data());

		bool sync = _animsSynced;
		if (ImGui::Checkbox("Sync Playback", &sync))
		{
			if (sync != _animsSynced)
			{
				_animsSynced = sync;
				SyncAnims(_animsSynced);
			}
		}

		ImGui::PushStyleColor(ImGuiCol_Button, { 0.1,0.5,0.1,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.2,0.8,0.2,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.4,0.8,0.4,1.0 });
		ImGui::PushStyleColor(ImGuiCol_Text, { 1,1,1,1 });
		if (ImGui::Button("Save"))
		{
			anim.SetAttributes(_animFCount, _animGrid[0], _animGrid[1], _animFPS, { _animFrameSize[0], _animFrameSize[1] });
			open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor(4);

		ImGui::PopItemWidth();
	
		ImGui::EndPopup();
	}
}

void LayerManager::LayerInfo::SyncAnims(bool sync)
{
	_idleSprite.ClearSync();
	if (sync)
	{
		_idleSprite.AddSync(&_talkSprite);
		_idleSprite.AddSync(&_blinkSprite);
		_idleSprite.AddSync(&_talkBlinkSprite);
		_idleSprite.Restart();
	}
}
