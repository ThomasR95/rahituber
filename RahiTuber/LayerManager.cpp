
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2\tinyxml2.h"
#include <sstream>

#include "defines.h"

#include <iostream>

#include "imgui/misc/single_file/imgui_single_file.h"

#include <windows.h>

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
			sf::RenderStates state = sf::RenderStates::Default;
			state.blendMode = layer._blendMode;

			state.transform.translate(_globalPos);
			state.transform.translate(0.5 * target->getSize().x, 0.5 * target->getSize().y);
			state.transform.scale(_globalScale);
			state.transform.rotate(_globalRot);
			state.transform.translate(-0.5 * target->getSize().x, -0.5 * target->getSize().y);

			if (layer._blendMode == g_blendmodes["Multiply"] ||
				layer._blendMode == g_blendmodes["Lighten"] ||
				layer._blendMode == g_blendmodes["Darken"])
			{
				
				auto rtSize = layer._activeSprite->Size();
				auto scale = layer._activeSprite->getScale();
				rtSize = { rtSize.x * scale.x, rtSize.y * scale.y };
				_blendingRT.create(rtSize.x, rtSize.y);

				if(layer._blendMode == g_blendmodes["Lighten"])
					_blendingRT.clear(sf::Color{ 0,0,0,255 });
				else
					_blendingRT.clear(sf::Color{255,255,255,255});

				auto pos = layer._activeSprite->getPosition();
				auto rot = layer._activeSprite->getRotation();
				
				sf::RenderStates tmpState = sf::RenderStates::Default;
				tmpState.blendMode = g_blendmodes["Normal"];

				layer._activeSprite->setPosition({ rtSize.x/2, rtSize.y/2});
				layer._activeSprite->setRotation(0);

				layer._idleSprite.Draw(&_blendingRT, tmpState);
				layer._talkSprite.Draw(&_blendingRT, tmpState);
				layer._blinkSprite.Draw(&_blendingRT, tmpState);
				layer._talkBlinkSprite.Draw(&_blendingRT, tmpState);

				_blendingRT.display();

				auto rtPlane = sf::RectangleShape(rtSize);
				rtPlane.setTexture(&_blendingRT.getTexture(), true);

				rtPlane.setOrigin({ rtSize.x / 2, rtSize.y / 2 });
				rtPlane.setPosition(pos);
				rtPlane.setRotation(rot);

				target->draw(rtPlane, state);

			}
			else
			{
				layer._idleSprite.Draw(target, state);
				layer._talkSprite.Draw(target, state);
				layer._blinkSprite.Draw(target, state);
				layer._talkBlinkSprite.Draw(target, state);
			}
		}
	}
}

void LayerManager::DrawGUI(ImGuiStyle& style, float maxHeight)
{
	float topBarBegin = ImGui::GetCursorPosY();

	ImGui::PushID("layermanager");

	float frameW = ImGui::GetWindowWidth();

	ImGui::PushItemWidth(202);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("Layer Set:");
	ImGui::SameLine();
	ImGui::PushID("layersXMLInput");
	char inputStr[MAX_PATH] = " ";
	_loadedXML.copy(inputStr, MAX_PATH);
	if (ImGui::InputText("", inputStr, MAX_PATH, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll))
	{
		_loadedXML = inputStr;
		auto xmlPath = fs::current_path().append(_loadedXML);
		if (xmlPath.extension().string() != ".xml")
			xmlPath.replace_extension(".xml");

		_loadedXMLExists = fs::exists(xmlPath);
	}
	ImGui::PopID();
	ImGui::PopItemWidth();

	static imgui_ext::file_browser_modal fileBrowserXML("Load Layer Set");
	fileBrowserXML._acceptedExt = { ".xml" };
	ImGui::SameLine();
	_loadXMLOpen = ImGui::Button("...", { 30,20 });
	if (_loadXMLOpen)
		fileBrowserXML.SetStartingDir(_fullLoadedXMLPath);
	if (fileBrowserXML.render(_loadXMLOpen, _loadedXMLPath))
	{
		auto xmlPath = fs::path(_loadedXMLPath);
		_loadedXMLExists = fs::exists(xmlPath);
		_fullLoadedXMLPath = xmlPath.string();
		auto proximateXMLPath = fs::proximate(xmlPath, fs::current_path());
		_loadedXMLPath = proximateXMLPath.string();
		_loadedXML = proximateXMLPath.replace_extension("").string();
		LoadLayers(_loadedXMLPath);
	}

	ImGui::SameLine();
	float textMargin = ImGui::GetCursorPosX();
	float buttonWidth = 0.5 * (frameW - textMargin) - style.ItemSpacing.x*2;
	ImGui::PushID("saveXMLBtn");
	if (ImGui::Button(_loadedXMLExists ? "Overwrite" : "Save", { buttonWidth, 20 }) && !_loadedXML.empty())
	{
		auto xmlPath = fs::current_path().append(_loadedXML);
		if (xmlPath.extension().string() != ".xml")
			xmlPath.replace_extension(".xml");

		SaveLayers(xmlPath.string());

		_loadedXMLExists = fs::exists(xmlPath);
		_fullLoadedXMLPath = xmlPath.string();
		_loadedXMLPath = fs::proximate(xmlPath, fs::current_path()).string();
		
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

	float buttonW = (frameW / 3) - style.ItemSpacing.x*2.55;

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

	ImGui::Separator();


	if (ImGui::CollapsingHeader("Global Settings"))
	{
		AddResetButton("pos", _globalPos, sf::Vector2f(0.0, 0.0), &style);
		float pos[2] = { _globalPos.x, _globalPos.y };
		if (ImGui::SliderFloat2("Position", pos, -1000.0, 1000.f))
		{
			_globalPos.x = pos[0];
			_globalPos.y = pos[1];
		}

		AddResetButton("rot", _globalRot, 0.f, &style);
		ImGui::SliderFloat("Rotation", &_globalRot, -180.f, 180.f);

		AddResetButton("scale", _globalScale, sf::Vector2f(1.0, 1.0), &style);
		float scale[2] = { _globalScale.x, _globalScale.y };
		if (ImGui::SliderFloat2("Scale", scale, 0.0, 5.f))
		{
			if (!_globalKeepAspect)
			{
				_globalScale.x = scale[0];
				_globalScale.y = scale[1];
			}
			else if (scale[0] != _globalScale.x)
			{
				_globalScale = { scale[0] , scale[0] };
			}
			else if (scale[1] != _globalScale.y)
			{
				_globalScale = { scale[1] , scale[1] };
			}
		}
		ImGui::Checkbox("Constrain", &_globalKeepAspect);
	}

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

void LayerManager::AddLayer(const LayerInfo* toCopy)
{
	LayerInfo newLayer = LayerInfo();

	if (toCopy != nullptr)
	{
		newLayer = LayerInfo(*toCopy);
		newLayer._name += " Copy";
	}

	_layers.push_back(newLayer);

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

		std::string bmName = "Normal";
		for (auto& bm : g_blendmodes)
			if (bm.second == layer._blendMode)
				bmName = bm.first;

		thisLayer->SetAttribute("blendMode", bmName.c_str());

		thisLayer->SetAttribute("scaleFilter", layer._scaleFiltering);
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
		thisHotkey->SetAttribute("ctrl", hkey._ctrl);
		thisHotkey->SetAttribute("shift", hkey._shift);
		thisHotkey->SetAttribute("alt", hkey._alt);
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

		layer._blendMode = g_blendmodes["Normal"];
		if (const char* blend = thisLayer->Attribute("blendMode"))
		{
			if (g_blendmodes.find(blend) != g_blendmodes.end())
				layer._blendMode = g_blendmodes[blend];
		}

		thisLayer->QueryAttribute("scaleFilter", &layer._scaleFiltering);

		if (layer._idleImage)
			layer._idleImage->setSmooth(layer._scaleFiltering);
		if (layer._talkImage)
			layer._talkImage->setSmooth(layer._scaleFiltering);
		if (layer._blinkImage)
			layer._blinkImage->setSmooth(layer._scaleFiltering);
		if (layer._talkBlinkImage)
			layer._talkBlinkImage->setSmooth(layer._scaleFiltering);

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
		thisHotkey->QueryAttribute("ctrl", &hkey._ctrl);
		thisHotkey->QueryAttribute("shift", &hkey._shift);
		thisHotkey->QueryAttribute("alt", &hkey._alt);
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

void LayerManager::HandleHotkey(const sf::Keyboard::Key& key, bool ctrl, bool shift, bool alt)
{
	for (auto& l : _layers)
		if (l._renamePopupOpen)
			return;

	for (int h = 0; h < _hotkeys.size(); h++)
	{
		auto& hkey = _hotkeys[h];
		if (hkey._key == key && hkey._ctrl == ctrl && hkey._shift == shift && hkey._alt == alt)
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
					LayerInfo* layer = GetLayer(state.first);
					if(layer)
						layer->_visible = state.second;
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
			auto size = ImGui::GetWindowSize();
			ImGui::SetNextWindowPos({ _appConfig->_scrW / 2 - 200, _appConfig->_scrH / 2 - 200 });
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

			std::string name = g_key_names[hkeys._key];
			if (hkeys._key == sf::Keyboard::Unknown)
				name = "Not set";
			if (hkeys._alt)
				name = "Alt, " + name;
			if (hkeys._shift)
				name = "Shift, " + name;
			if (hkeys._ctrl)
				name = "Ctrl, " + name;
			ImVec2 headerTxtPos = { ImGui::GetCursorPosX() + 20, ImGui::GetCursorPosY() + 3 };
			ImVec2 delButtonPos = { ImGui::GetCursorPosX() + 330, ImGui::GetCursorPosY() };

			ImGui::PushID('hkey' + hkeyIdx);
			if (ImGui::CollapsingHeader("", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ImGui::Columns(3, 0, false);
				ImGui::SetColumnWidth(0, 150);
				ImGui::SetColumnWidth(1, 100);
				ImGui::SetColumnWidth(2, 300);
				std::string btnName = name;
				if (hkeys._key == sf::Keyboard::Unknown)
					btnName = " Click to\nrecord key";
				if (hkeys._awaitingHotkey)
					btnName = "(press a key)";
				ImGui::PushID("recordKeyBtn");
				if (ImGui::Button(btnName.c_str(), { 140,42 }) && !_waitingForHotkey)
				{
					_pendingKey = sf::Keyboard::Unknown;
					_pendingCtrl = false;
					_pendingShift = false;
					_pendingAlt = false;
					_waitingForHotkey = true;
					hkeys._awaitingHotkey = true;
				}
				ImGui::PopID();

				if (hkeys._awaitingHotkey && _waitingForHotkey && _pendingKey != sf::Keyboard::Unknown)
				{
					hkeys._key = _pendingKey;
					hkeys._ctrl = _pendingCtrl;
					hkeys._shift = _pendingShift;
					hkeys._alt = _pendingAlt;
					_waitingForHotkey = false;
					hkeys._awaitingHotkey = false;
				}

				ImGui::NextColumn();

				ImGui::Checkbox("Toggle", &hkeys._toggle);
				ImGui::Checkbox("Timeout", &hkeys._useTimeout);

				ImGui::NextColumn();

				ImGui::SameLine(10);
				if (hkeys._useTimeout)
				{
					ImGui::PushID("timeoutSlider");
					ImGui::SliderFloat("", &hkeys._timeout, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic);
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

			ImGui::SetCursorPos(headerTxtPos);
			ImGui::Text(name.c_str());

			ImGui::SetCursorPos(delButtonPos);
			auto style = ImGui::GetStyle();
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
			ImGui::PushStyleColor(ImGuiCol_Text, { 255.f / 255,200.f / 255,170.f / 255, 1.f });
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

	float talkFactor = 0;
	if (talkMax > 0)
	{
		talkFactor = talkLevel / talkMax;
		talkFactor = pow(talkFactor, 0.5);
		_lastTalkFactor = talkFactor;
	}


	bool talking = talkFactor > _talkThreshold;

	bool blinkAvailable = _blinkImage && !talking;
	bool talkBlinkAvailable = _blinkWhileTalking && _talkBlinkImage && talking;

	bool canStartBlinking = (talkBlinkAvailable || blinkAvailable) && !_isBlinking && _useBlinkFrame;

	if (canStartBlinking && _blinkTimer.getElapsedTime().asSeconds() > _blinkDelay + _blinkVarDelay)
	{
		_isBlinking = true;
		_blinkTimer.restart();
		if(!_blinkSprite.IsSynced())
			_blinkSprite.Restart();
		_blinkVarDelay = GetRandom11() * _blinkVariation;
	}

	if (_isBlinking)
	{
		if (talkBlinkAvailable)
		{
			_activeSprite = &_talkBlinkSprite;
			_talkBlinkSprite._visible = true;
			_blinkSprite._visible = false;
			_talkSprite._visible = false;
			_idleSprite._visible = false;
			_talkBlinkSprite.SetColor(_talkBlinkTint);
		}
		else if (blinkAvailable)
		{
			_activeSprite = &_blinkSprite;
			_blinkSprite._visible = true;
			_talkBlinkSprite._visible = false;
			_talkSprite._visible = false;
			_idleSprite._visible = false;
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
			if (floor(_motionDelay) == ceil(_motionDelay) && mp->_motionLinkData.size() > _motionDelay)
			{
				mpScale = mp->_motionLinkData[(size_t)_motionDelay]._scale;
				mpPos = mp->_motionLinkData[(size_t)_motionDelay]._pos;
				mpRot = mp->_motionLinkData[(size_t)_motionDelay]._rot;
			}
			else
			{
				size_t prev = floor(_motionDelay);
				size_t next = ceil(_motionDelay);

				if (mp->_motionLinkData.size() > next)
				{
					float fraction = _motionDelay - prev;
					mpScale = mp->_motionLinkData[prev]._scale + fraction * (mp->_motionLinkData[next]._scale - mp->_motionLinkData[prev]._scale);
					mpPos = mp->_motionLinkData[prev]._pos + fraction * (mp->_motionLinkData[next]._pos - mp->_motionLinkData[prev]._pos);
					mpRot = mp->_motionLinkData[prev]._rot + fraction * (mp->_motionLinkData[next]._rot - mp->_motionLinkData[prev]._rot);
				}
			}

			MotionLinkData thisFrame;
			thisFrame._pos = mpPos;
			thisFrame._scale = mpScale;
			thisFrame._rot = mpRot;
			_motionLinkData.push_front(thisFrame);
			if (_motionLinkData.size() > 11)
				_motionLinkData.pop_back();
			
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
		_animIcon = _textureMan.GetTexture("res/anim.png");

	if (_emptyIcon == nullptr)
		_emptyIcon = _textureMan.GetTexture("res/empty.png");

	if (_upIcon == nullptr)
		_upIcon = _textureMan.GetTexture("res/arrowup.png");

	if (_dnIcon == nullptr)
		_dnIcon = _textureMan.GetTexture("res/arrowdn.png");

	if (_editIcon == nullptr)
		_editIcon = _textureMan.GetTexture("res/edit.png");

	if (_delIcon == nullptr)
		_delIcon = _textureMan.GetTexture("res/delete.png");

	if (_dupeIcon == nullptr)
		_dupeIcon = _textureMan.GetTexture("res/duplicate.png");

	//_dupeIcon->setSmooth(true);
	_delIcon->setSmooth(true);
	_editIcon->setSmooth(true);
	_emptyIcon->setSmooth(true);

	ImVec4 col = style.Colors[ImGuiCol_Text];
	sf::Color btnColor = { sf::Uint8(255*col.x), sf::Uint8(255*col.y), sf::Uint8(255*col.z) };

	ImGui::PushID(_id);
	std::string name = "[" + std::to_string(layerID) + "] " + _name;
	sf::Vector2f headerBtnSize(17, 17);
	ImVec2 headerButtonsPos = { ImGui::GetWindowWidth() - headerBtnSize.x*8, ImGui::GetCursorPosY()};

	if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
	{
		static imgui_ext::file_browser_modal fileBrowserIdle("Import Idle Sprite");
		static imgui_ext::file_browser_modal fileBrowserTalk("Import Talk Sprite");
		static imgui_ext::file_browser_modal fileBrowserBlink("Import Blink Sprite");

		ImGui::Columns(3, "imagebuttons", false);

		ImGui::TextColored(style.Colors[ImGuiCol_Text], "Idle");
		ImGui::PushID("idleimport");

		float imgBtnWidth = 108;

		sf::Color idleCol = _idleImage == nullptr ? btnColor : sf::Color::White;
		sf::Texture* idleIcon = _idleImage == nullptr ? _emptyIcon : _idleImage;
		_importIdleOpen = ImGui::ImageButton(*idleIcon, { imgBtnWidth,imgBtnWidth }, -1, sf::Color::Transparent, idleCol);
		if (_importIdleOpen && _idleImage)
			fileBrowserIdle.SetStartingDir(_idleImagePath);
		if (fileBrowserIdle.render(_importIdleOpen, _idleImagePath))
		{
			if (_idleImage == nullptr)
				_idleImage = new sf::Texture();
			_idleImage = _textureMan.GetTexture(_idleImagePath);
			_idleImage->setSmooth(_scaleFiltering);
			_idleSprite.LoadFromTexture(*_idleImage, 1, 1, 1, 1);
		}

		ImGui::SameLine(imgBtnWidth + 16);
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

		ImGui::ColorEdit4("Tint", _idleTint, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);

		ImGui::PopID();

		ImGui::NextColumn();

		if (_swapWhenTalking)
		{
			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk");
			ImGui::PushID("talkimport");
			sf::Color talkCol = _talkImage == nullptr ? btnColor : sf::Color::White;
			sf::Texture* talkIcon = _talkImage == nullptr ? _emptyIcon : _talkImage;
			_importTalkOpen = ImGui::ImageButton(*talkIcon, { imgBtnWidth,imgBtnWidth }, -1, sf::Color::Transparent, talkCol);
			fileBrowserTalk.SetStartingDir(chosenDir);
			if (_talkImage)
				fileBrowserTalk.SetStartingDir(_talkImagePath);
			if (fileBrowserTalk.render(_importTalkOpen, _talkImagePath))
			{
				if (_talkImage == nullptr)
					_talkImage = new sf::Texture();
				_talkImage = _textureMan.GetTexture(_talkImagePath);
				_talkImage->setSmooth(_scaleFiltering);
				_talkSprite.LoadFromTexture(*_talkImage, 1, 1, 1, 1);
			}

			ImGui::SameLine(imgBtnWidth + 16);
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

			ImGui::ColorEdit4("Tint", _talkTint, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);

			ImGui::PopID();
		}

		ImGui::NextColumn();

		if (_useBlinkFrame)
		{
			sf::Vector2f blinkBtnSize = _blinkWhileTalking ? sf::Vector2f(48, 48) : sf::Vector2f(imgBtnWidth, imgBtnWidth);

			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Blink");
			ImGui::PushID("blinkimport");
			sf::Color blinkCol = _blinkImage == nullptr ? btnColor : sf::Color::White;
			sf::Texture* blinkIcon = _blinkImage == nullptr ? _emptyIcon : _blinkImage;
			ImVec2 tintPos = ImVec2(ImGui::GetCursorPosX() + blinkBtnSize.x + 8, ImGui::GetCursorPosY() + 20);
			_importBlinkOpen = ImGui::ImageButton(*blinkIcon, blinkBtnSize, -1, sf::Color::Transparent, blinkCol);
			fileBrowserBlink.SetStartingDir(chosenDir);
			if (_blinkImage)
				fileBrowserBlink.SetStartingDir(_blinkImagePath);
			if (fileBrowserBlink.render(_importBlinkOpen, _blinkImagePath))
			{
				if (_blinkImage == nullptr)
					_blinkImage = new sf::Texture();
				_blinkImage = _textureMan.GetTexture(_blinkImagePath);
				_blinkImage->setSmooth(_scaleFiltering);
				_blinkSprite.LoadFromTexture(*_blinkImage, 1, 1, 1, 1);
			}

			ImGui::SameLine(blinkBtnSize.x + 16);
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

			auto preTintPos = ImGui::GetCursorPos();
			if (_blinkWhileTalking)
				ImGui::SetCursorPos(tintPos);
			ImGui::ColorEdit4("Tint", _blinkTint, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
			ImGui::SetCursorPos(preTintPos);
			ImGui::PopID();

			if (_blinkWhileTalking)
			{
				//ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk Blink");
				ImGui::PushID("talkblinkimport");
				sf::Color talkblinkCol = _talkBlinkImage == nullptr ? btnColor : sf::Color::White;
				sf::Texture* talkblinkIcon = _talkBlinkImage == nullptr ? _emptyIcon : _talkBlinkImage;
				tintPos = ImVec2(ImGui::GetCursorPosX() + blinkBtnSize.x + 8, ImGui::GetCursorPosY() + 20);
				_importTalkBlinkOpen = ImGui::ImageButton(*talkblinkIcon, blinkBtnSize, -1, sf::Color::Transparent, talkblinkCol);
				fileBrowserBlink.SetStartingDir(chosenDir);
				if (_talkBlinkImage)
					fileBrowserBlink.SetStartingDir(_talkBlinkImagePath);
				if (fileBrowserBlink.render(_importTalkBlinkOpen, _talkBlinkImagePath))
				{
					if (_talkBlinkImage == nullptr)
						_talkBlinkImage = new sf::Texture();
					_talkBlinkImage = _textureMan.GetTexture(_talkBlinkImagePath);
					_talkBlinkImage->setSmooth(_scaleFiltering);
					_talkBlinkSprite.LoadFromTexture(*_talkBlinkImage, 1, 1, 1, 1);
				}

				ImGui::SameLine(blinkBtnSize.x + 16);
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

				preTintPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(tintPos);

				ImGui::ColorEdit4("Tint", _talkBlinkTint, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
				ImGui::SetCursorPos(preTintPos);
				ImGui::PopID();
			}
		}
		ImGui::Columns();

		sf::BlendMode oldBlendMode = _blendMode;
		std::string bmName = "";
		for (auto& bm : g_blendmodes)
			if (bm.second == oldBlendMode)
				bmName = bm.first;
		ImGui::PushItemWidth(240);
		if (ImGui::BeginCombo("Blend Mode", bmName.c_str()))
		{
			for (auto& bm : g_blendmodes)
			{
				if (ImGui::Selectable(bm.first.c_str(), bm.second == oldBlendMode))
				{
					_blendMode = bm.second;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Scale Filter", &_scaleFiltering))
		{
			if (_idleImage)
				_idleImage->setSmooth(_scaleFiltering);
			if (_talkImage)
				_talkImage->setSmooth(_scaleFiltering);
			if (_blinkImage)
				_blinkImage->setSmooth(_scaleFiltering);
			if (_talkBlinkImage)
				_talkBlinkImage->setSmooth(_scaleFiltering);
		}
		ImGui::PopItemWidth();
		
		ImGui::Separator();

		AddResetButton("talkThresh", _talkThreshold, 0.15f, &style);
		ImVec2 barPos = ImGui::GetCursorPos();
		ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::NewLine();

		sf::Color barHighlight(60, 140, 60, 255);
		sf::Color barBg(20, 60, 20, 255);
		if (_lastTalkFactor < 0.001 || _lastTalkFactor < _talkThreshold)
		{
			barHighlight = sf::Color(140, 60, 60, 255);
			barBg = sf::Color(60, 20, 20, 255);
		}

		sf::Vector2f topLeft = { barPos.x - 2, barPos.y };
		float barWidth = (ImGui::GetWindowWidth() - topLeft.x) - 142;
		float barHeight = 8;
		sf::FloatRect volumeBarBg({ topLeft.x, -18 }, { barWidth, barHeight });
		ImGui::DrawRectFilled(volumeBarBg, barBg, 3);
		float activeBarWidth = barWidth * powf(_lastTalkFactor, 0.5);
		sf::FloatRect volumeBar({ topLeft.x, -18 }, { activeBarWidth, barHeight });
		ImGui::DrawRectFilled(volumeBar, barHighlight, 3);
		float rootThresh = powf(_talkThreshold, 0.5);
		float thresholdPos = barWidth * rootThresh;
		sf::FloatRect thresholdBar({ topLeft.x + thresholdPos, -23 }, { 2, barHeight + 5 });
		ImGui::DrawRectFilled(thresholdBar, {200,150,80});
		
		ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);

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
			ImGui::SliderFloat("Motion Delay", &_motionDelay, 0.0, 10.0, "%.1f", ImGuiSliderFlags_Logarithmic);
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

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });

	ImGui::SameLine();
	ImGui::PushID("upbtn");
	if (ImGui::ImageButton(*_upIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerUp(this);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("dnbtn");
	if (ImGui::ImageButton(*_dnIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerDown(this);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("renameBtn");
	if (ImGui::ImageButton(*_editIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
	{
		ImGui::PopID();
		_renamingString = _name;
		_renamePopupOpen = true;
		ImGui::SetNextWindowSize({ 200,60 });
		ImGui::OpenPopup("Rename Layer");
	}
	else
		ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("duplicateBtn");
	if (ImGui::ImageButton(*_dupeIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
	{
		_parent->AddLayer(this);
	}
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
	ImGui::PushStyleColor(ImGuiCol_Text, { 255/255,200/255,170/255, 1 });
	ImGui::PushID("deleteBtn");
	if (ImGui::ImageButton(*_delIcon, headerBtnSize, 1, sf::Color::Transparent, sf::Color(255,200,170)))
		_parent->RemoveLayer(this);
	ImGui::PopID();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(1);

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
			ImGui::SetNextWindowPos({ _parent->_appConfig->_scrW / 2 - 200, _parent->_appConfig->_scrH / 2 - 120 });
			ImGui::SetNextWindowSize({ 400, 240 });
			ImGui::OpenPopup("Sprite Sheet Setup");

			auto gridSize = anim.GridSize();
			_animGrid = { gridSize.x, gridSize.y };
			_animFCount = anim.FrameCount();
			_animFPS = anim.FPS();
			_animFrameSize = { anim.Size().x, anim.Size().y };
		}
		else
		{
			//closed
			SyncAnims(_animsSynced);
		}
	}

	if(ImGui::BeginPopupModal("Sprite Sheet Setup", &open))
	{
		ImGui::Columns(2, 0, false);
		ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
		ImGui::TextWrapped("If you need help creating a sprite sheet, here's a free tool:");
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
			ImGui::InputFloat("FPS", &_animFPS, 1,1, "%.1f");
		else
		{
			std::stringstream ss;
			ss << _animFPS;
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
			ImGui::InputText("FPS", ss.str().data(), ss.str().length(), ImGuiInputTextFlags_ReadOnly );
			ImGui::PopStyleColor();
		}

		AddResetButton("framereset", _animFrameSize, { -1, -1 });
		ImGui::InputFloat2("Frame Size (auto = [-1,-1])", _animFrameSize.data());

		bool sync = _animsSynced;
		if (ImGui::Checkbox("Sync Playback", &sync))
		{
			_animsSynced = sync;
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
