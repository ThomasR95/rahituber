
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2\tinyxml2.h"
#include <sstream>

#include "defines.h"

#include <iostream>

#include "imgui/misc/single_file/imgui_single_file.h"

#include <windows.h>

// For UUID
#include <Rpc.h>
#pragma comment(lib, "Rpcrt4.lib")

//#include <shellapi.h>

void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}

LayerManager::~LayerManager()
{
	_textureMan->Reset();
	//_chatReader.Cleanup();
}

const std::vector<std::string> g_activeTypeNames
{
	"Toggle",
	"While Held",
	"Permanent"
};

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	// reset to default states
	if (_statesDirty)
	{
		for (auto& l : _layers)
		{
			if (_defaultLayerStates.count(l._id))
				l._visible = _defaultLayerStates[l._id];
		}
		_statesDirty = false;
	}

	// activate timed states
	for (UINT stateIdx = 0; stateIdx < _states.size(); stateIdx++)
	{
		auto& state = _states[stateIdx];
		if (!state._active && state._schedule && state._timer.getElapsedTime().asSeconds() > state._currentIntervalTime)
		{
			SaveDefaultStates();

			state._active = true;
			state._currentIntervalTime = state._intervalTime + GetRandom11() * state._intervalVariation;
			state._timer.restart();
			_statesOrder.push_back(&_states[stateIdx]);
		}
	}

	// progressively display the states in order of activation
	auto statesOrderCopy = _statesOrder;
	for (auto state : statesOrderCopy)
	{
		if (state->_active && (state->_useTimeout || state->_schedule) && state->_timer.getElapsedTime().asSeconds() >= state->_timeout)
		{
			state->_active = false;
			RemoveStateFromOrder(state);
			state->_timer.restart();
		}

		if (state->_active)
		{
			_statesDirty = true;
			for (auto& st : state->_layerStates)
			{
				LayerInfo* layer = GetLayer(st.first);
				if (layer && st.second != StatesInfo::NoChange)
				{
					layer->_visible = st.second;
				}
			}
		}
	}


	std::vector<LayerInfo*> calculateOrder;
	for (int l = _layers.size() - 1; l >= 0; l--)
		calculateOrder.push_back(&_layers[l]);

	std::sort(calculateOrder.begin(), calculateOrder.end(), [](const LayerInfo* lhs, const LayerInfo* rhs)
		{
			return (lhs->_id == rhs->_motionParent);
		});

	for (auto layer : calculateOrder)
	{
		bool calculate = layer->_visible;

		if (!calculate)
		{
			for (int l = _layers.size() - 1; l >= 0; l--)
			{
				LayerInfo& checkLayer = _layers[l];
				//check if any layer relies on it as a parent
				if (!checkLayer._visible)
					continue;
				if (checkLayer._motionParent != layer->_id)
					continue;
				if (checkLayer._hideWithParent)
					continue;

				// layer relies on this one to move
				calculate = true;
			}
		}

		if (calculate)
			layer->CalculateDraw(windowHeight, windowWidth, talkLevel, talkMax);
	}


	for (int l = _layers.size() - 1; l >= 0; l--)
	{
		LayerInfo& layer = _layers[l];

		bool visible = layer._visible;
		if (layer._hideWithParent)
		{
			LayerInfo* mp = GetLayer(layer._motionParent);
			if (mp)
				visible &= mp->_visible;
		}

		if (visible)
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

				auto size = layer._activeSprite->Size();
				auto rtSize = size;
				auto scale = layer._activeSprite->getScale();
				rtSize = { rtSize.x * scale.x, rtSize.y * scale.y };
				_blendingRT.create(rtSize.x, rtSize.y);

				if (layer._blendMode == g_blendmodes["Lighten"])
					_blendingRT.clear(sf::Color{ 0,0,0,255 });
				else
					_blendingRT.clear(sf::Color{ 255,255,255,255 });

				auto pos = layer._activeSprite->getPosition();
				auto rot = layer._activeSprite->getRotation();
				auto origin = layer._activeSprite->getOrigin();

				sf::RenderStates tmpState = sf::RenderStates::Default;
				tmpState.blendMode = g_blendmodes["Normal"];

				layer._activeSprite->setPosition({ rtSize.x / 2, rtSize.y / 2 });
				layer._activeSprite->setOrigin({ size.x / 2, size.y / 2 });
				layer._activeSprite->setRotation(0);

				layer._idleSprite->Draw(&_blendingRT, tmpState);
				layer._talkSprite->Draw(&_blendingRT, tmpState);
				layer._blinkSprite->Draw(&_blendingRT, tmpState);
				layer._talkBlinkSprite->Draw(&_blendingRT, tmpState);
				layer._screamSprite->Draw(&_blendingRT, tmpState);

				_blendingRT.display();

				auto rtPlane = sf::RectangleShape(rtSize);
				rtPlane.setTexture(&_blendingRT.getTexture(), true);

				rtPlane.setOrigin({ origin.x * scale.x, origin.y * scale.y});
				rtPlane.setPosition(pos);
				rtPlane.setRotation(rot);

				target->draw(rtPlane, state);

				layer._activeSprite->setPosition(pos);
				layer._activeSprite->setRotation(rot);
				layer._activeSprite->setOrigin(origin);

			}
			else
			{
				layer._idleSprite->Draw(target, state);
				layer._talkSprite->Draw(target, state);
				layer._blinkSprite->Draw(target, state);
				layer._talkBlinkSprite->Draw(target, state);
				layer._screamSprite->Draw(target, state);
			}
		}

		layer._oldVisible = visible;
	}

	if (_uiConfig->_menuShowing && _uiConfig->_showLayerBounds)
	{
		for (int l = _layers.size() - 1; l >= 0; l--)
		{
			LayerInfo& layer = _layers[l];

			bool visible = layer._visible;
			if (layer._hideWithParent)
			{
				LayerInfo* mp = GetLayer(layer._motionParent);
				if (mp)
					visible &= mp->_visible;
			}

			if (visible)
			{
				// Draw sprite borders

				sf::Vector2f origin = layer._activeSprite->getOrigin();
				sf::Vector2f pos = layer._activeSprite->getPosition();
				sf::Vector2f scale = layer._activeSprite->getScale();
				sf::Vector2f size = layer._activeSprite->Size();
				float rot = layer._activeSprite->getRotation();

				sf::Vector2f boxSize = { size.x * scale.x, size.y * scale.y };
				sf::Vector2f boxOrigin = { origin.x * scale.x, origin.y * scale.y };

				auto box = sf::RectangleShape(boxSize);
				box.setOrigin(boxOrigin);
				box.setPosition(pos);
				box.setRotation(rot);
				box.setOutlineColor(toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)));
				box.setOutlineThickness(1);
				sf::Color fill = toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
				fill.a = 0.5;
				box.setFillColor(fill);

				auto circle = sf::CircleShape(3, 8);
				circle.setPosition(pos);
				circle.setFillColor(toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text)));
				circle.setOutlineColor(toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Button)));
				circle.setOutlineThickness(2);

				target->draw(box, sf::RenderStates::Default);
				target->draw(circle, sf::RenderStates::Default);
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
		fs::path appFolder = _appConfig->_appLocation;
		fs::path xmlPath = appFolder.append(_loadedXML);
		if (xmlPath.extension().string() != ".xml")
			xmlPath.replace_extension(".xml");

		_loadedXMLExists = fs::exists(xmlPath);
	}
	ImGui::PopID();
	ImGui::PopItemWidth();
	ToolTip("Enter the name of a layer set to load.", &_appConfig->_hoverTimer);

	static imgui_ext::file_browser_modal fileBrowserXML("Load Layer Set");
	fileBrowserXML._acceptedExt = { ".xml" };
	ImGui::SameLine();
	_loadXMLOpen = ImGui::Button("...", { 30,20 });
	if (_loadXMLOpen)
		fileBrowserXML.SetStartingDir(_fullLoadedXMLPath);
	if (fileBrowserXML.render(_loadXMLOpen, _loadedXMLPath))
	{
		fs::path appFolder = _appConfig->_appLocation;
		fs::path xmlPath = fs::path(_loadedXMLPath);
		_loadedXMLExists = fs::exists(xmlPath);
		_fullLoadedXMLPath = xmlPath.string();
		fs::path proximateXMLPath = fs::proximate(xmlPath, appFolder);
		_loadedXMLPath = proximateXMLPath.string();
		_loadedXML = proximateXMLPath.replace_extension("").string();
		LoadLayers(_loadedXMLPath);
	}
	ToolTip("Browse for a layer set (.xml) file.", &_appConfig->_hoverTimer);

	ImGui::SameLine();
	float textMargin = ImGui::GetCursorPosX();
	float buttonWidth = 0.5f * (frameW - textMargin) - style.ItemSpacing.x*2.f;
	ImGui::PushID("saveXMLBtn");
	if (ImGui::Button(_loadedXMLExists ? "Overwrite" : "Save", { buttonWidth, 20 }) && !_loadedXML.empty())
	{
		fs::path appFolder = _appConfig->_appLocation;
		fs::path xmlPath = appFolder.append(_loadedXML);
		if (xmlPath.extension().string() != ".xml")
			xmlPath.replace_extension(".xml");

		SaveLayers(xmlPath.string());

		_loadedXMLExists = fs::exists(xmlPath);
		_fullLoadedXMLPath = xmlPath.string();
		_loadedXMLPath = fs::proximate(xmlPath, appFolder).string();
		
	}
	ImGui::PopID();
	ToolTip("Save the current layer set.", &_appConfig->_hoverTimer);

	if (_loadedXMLExists)
	{
		ImGui::SameLine();
		ImGui::PushID("loadXMLBtn");
		if (ImGui::Button("Load", { buttonWidth, 20 }) && _loadedXMLExists)
			LoadLayers(_loadedXML + ".xml");
		ImGui::PopID();
	}
	ToolTip("Load the specified layer set.", &_appConfig->_hoverTimer);

	if (_errorMessage.empty() == false)
	{
		ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), _errorMessage.c_str());
	}

	float buttonW = (frameW / 3) - style.ItemSpacing.x*2.55;

	if (ImGui::Button("Add Layer", { buttonW, 20 }))
		AddLayer();
	ToolTip("Adds a new layer to the list.", &_appConfig->_hoverTimer);

	ImGui::SameLine();
	if (ImGui::Button("Remove All", { buttonW, 20 }))
		_layers.clear();
	ToolTip("Removes all layers from the list.", &_appConfig->_hoverTimer);

	ImGui::SameLine();
	ImGui::PushID("statesBtn");
	if (ImGui::Button("States", { buttonW, 20 }))
		_statesMenuOpen = true;
	ImGui::PopID();
	ToolTip("Opens the States setup menu.", &_appConfig->_hoverTimer);

	ImGui::PushID("statesPopup");
	DrawStatesGUI();
	ImGui::PopID();

	ImGui::Separator();


	if (ImGui::CollapsingHeader("Global Settings"))
	{
		AddResetButton("pos", _globalPos, sf::Vector2f(0.0, 0.0), _appConfig, &style);
		float pos[2] = { _globalPos.x, _globalPos.y };
		if (ImGui::SliderFloat2("Position", pos, -1000.0, 1000.f))
		{
			_globalPos.x = pos[0];
			_globalPos.y = pos[1];
		}

		AddResetButton("rot", _globalRot, 0.f, _appConfig, &style);
		ImGui::SliderFloat("Rotation", &_globalRot, -180.f, 180.f);

		AddResetButton("scale", _globalScale, sf::Vector2f(1.0, 1.0), _appConfig, &style);
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
		ToolTip("Keeps the Scale x/y values the same.", &_appConfig->_hoverTimer);
	}
	ToolTip("Global position/scale/rotation settings\n(to change the size/position of the whole scene).", &_appConfig->_hoverTimer);

	ImGui::Separator();

	float topBarHeight = ImGui::GetCursorPosY() - topBarBegin;

	ImGui::BeginChild(ImGuiID(10001), ImVec2(-1, maxHeight - topBarHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

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

	LayerInfo* layer = nullptr;

	if (toCopy != nullptr)
	{
		newLayer = LayerInfo(*toCopy);
		newLayer._name += " Copy";
		newLayer._blinkSprite = std::make_shared<SpriteSheet>(*newLayer._blinkSprite.get());
		newLayer._talkBlinkSprite = std::make_shared<SpriteSheet>(*newLayer._talkBlinkSprite.get());
		newLayer._talkSprite = std::make_shared<SpriteSheet>(*newLayer._talkSprite.get());
		newLayer._screamSprite = std::make_shared<SpriteSheet>(*newLayer._screamSprite.get());
		newLayer._idleSprite = std::make_shared<SpriteSheet>(*newLayer._idleSprite.get());

		newLayer.SyncAnims(newLayer._animsSynced);
		
		int idx = 0;
		for (auto lit = _layers.begin(); lit < _layers.end(); lit++, idx++)
		{
			if (&(*lit) == toCopy)
			{
				_layers.insert(lit, newLayer);
				layer = &_layers[idx];
				break;
			}
		}
	}
	else
	{
		_layers.push_back(newLayer);
		layer = &_layers.back();
	}

	UUID uuid = { 0 };
	std::string guid;

	// Create uuid or load from a string by UuidFromString() function
	::UuidCreate(&uuid);

	// If you want to convert uuid to string, use UuidToString() function
	RPC_CSTR szUuid = NULL;
	if (::UuidToStringA(&uuid, &szUuid) == RPC_S_OK)
	{
		guid = (char*)szUuid;
		::RpcStringFreeA(&szUuid);
	}

	layer->_blinkTimer.restart();
	layer->_isBlinking = false;
	layer->_blinkVarDelay = GetRandom11() * layer->_blinkVariation;
	layer->_parent = this;
	layer->_id = guid;
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
	_errorMessage = "";

	tinyxml2::XMLDocument doc;

	doc.LoadFile(settingsFileName.c_str());

	if (doc.Error())
	{
		doc.LoadFile((_appConfig->_appLocation + settingsFileName).c_str());
	}

	auto root = doc.FirstChildElement("Config");
	if (!root) 
		root = doc.InsertFirstChild(doc.NewElement("Config"))->ToElement();

	if (!root)
	{
		_errorMessage = "Could not save config element: " + settingsFileName;
		return false;
	}

	auto layers = root->FirstChildElement("layers");
	if (!layers) 
		layers = root->InsertFirstChild(doc.NewElement("layers"))->ToElement();

	if (!layers)
	{
		_errorMessage = "Could not save layers element: " + settingsFileName;
		return false;
	}

	layers->DeleteChildren();

	ResetStates();
	
	for (int l = 0; l < _layers.size(); l++)
	{
		auto thisLayer = layers->InsertEndChild(doc.NewElement("layer"))->ToElement();

		const auto& layer = _layers[l];

		thisLayer->SetAttribute("id", layer._id.c_str());

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
		thisLayer->SetAttribute("breathHeight", layer._breathMove.y);
		thisLayer->SetAttribute("breathTime", layer._breathFrequency);
		thisLayer->SetAttribute("breathMoveX", layer._breathMove.x);
		thisLayer->SetAttribute("breathScaleX", layer._breathScale.x);
		thisLayer->SetAttribute("breathScaleY", layer._breathScale.y);
		thisLayer->SetAttribute("breathCircular", layer._breathCircular);
		thisLayer->SetAttribute("breatheWhileTalking", layer._breatheWhileTalking);

		thisLayer->SetAttribute("screaming", layer._scream);
		thisLayer->SetAttribute("screamThreshold", layer._screamThreshold);
		thisLayer->SetAttribute("screamVibrate", layer._screamVibrate);
		thisLayer->SetAttribute("screamVibrateAmount", layer._screamVibrateAmount);

		thisLayer->SetAttribute("restartAnimsOnVisible", layer._restartAnimsOnVisible);

		thisLayer->SetAttribute("idlePath", layer._idleImagePath.c_str());
		thisLayer->SetAttribute("talkPath", layer._talkImagePath.c_str());
		thisLayer->SetAttribute("blinkPath", layer._blinkImagePath.c_str());
		thisLayer->SetAttribute("talkBlinkPath", layer._talkBlinkImagePath.c_str());
		thisLayer->SetAttribute("screamPath", layer._screamImagePath.c_str());

		if (layer._idleSprite->FrameCount() > 1 || layer._idleSprite->GridSize() != sf::Vector2i(1,1))
			SaveAnimInfo(thisLayer, &doc, "idleAnim", *layer._idleSprite);

		if (layer._talkSprite->FrameCount() > 1 || layer._talkSprite->GridSize() != sf::Vector2i(1, 1))
			SaveAnimInfo(thisLayer, &doc, "talkAnim", *layer._talkSprite);

		if (layer._blinkSprite->FrameCount() > 1 || layer._blinkSprite->GridSize() != sf::Vector2i(1, 1))
			SaveAnimInfo(thisLayer, &doc, "blinkAnim", *layer._blinkSprite);

		if (layer._talkBlinkSprite->FrameCount() > 1 || layer._talkBlinkSprite->GridSize() != sf::Vector2i(1, 1))
			SaveAnimInfo(thisLayer, &doc, "talkBlinkAnim", *layer._talkBlinkSprite);

		if (layer._screamSprite->FrameCount() > 1 || layer._screamSprite->GridSize() != sf::Vector2i(1, 1))
			SaveAnimInfo(thisLayer, &doc, "screamAnim", *layer._screamSprite);

		thisLayer->SetAttribute("syncAnims", layer._animsSynced);

		SaveColor(thisLayer, &doc, "idleTint", layer._idleTint);
		SaveColor(thisLayer, &doc, "talkTint", layer._talkTint);
		SaveColor(thisLayer, &doc, "blinkTint", layer._blinkTint);
		SaveColor(thisLayer, &doc, "talkBlinkTint", layer._talkBlinkTint);
		SaveColor(thisLayer, &doc, "screamTint", layer._screamTint);

		thisLayer->SetAttribute("scaleX", layer._scale.x);
		thisLayer->SetAttribute("scaleY", layer._scale.y);
		thisLayer->SetAttribute("posX", layer._pos.x);
		thisLayer->SetAttribute("posY", layer._pos.y);
		thisLayer->SetAttribute("rot", layer._rot);

		thisLayer->SetAttribute("pivotX", layer._pivot.x);
		thisLayer->SetAttribute("pivotY", layer._pivot.y);

		thisLayer->SetAttribute("motionParent", layer._motionParent.c_str());
		thisLayer->SetAttribute("motionDelayTime", layer._motionDelay);
		thisLayer->SetAttribute("hideWithParent", layer._hideWithParent);
		thisLayer->SetAttribute("motionDrag", layer._motionDrag);
		thisLayer->SetAttribute("motionSpring", layer._motionSpring);
		thisLayer->SetAttribute("distanceLimit", layer._distanceLimit);
		thisLayer->SetAttribute("rotationEffect", layer._rotationEffect);

		std::string bmName = "Normal";
		for (auto& bm : g_blendmodes)
			if (bm.second == layer._blendMode)
				bmName = bm.first;

		thisLayer->SetAttribute("blendMode", bmName.c_str());

		thisLayer->SetAttribute("scaleFilter", layer._scaleFiltering);
	}

	root->SetAttribute("globalScaleX", _globalScale.x);
	root->SetAttribute("globalScaleY", _globalScale.y);
	root->SetAttribute("globalPosX", _globalPos.x);
	root->SetAttribute("globalPosY", _globalPos.y);
	root->SetAttribute("globalRot", _globalRot);

	auto hotkeys = root->FirstChildElement("hotkeys");
	if (!hotkeys)
		hotkeys = root->InsertFirstChild(doc.NewElement("hotkeys"))->ToElement();

	if (!hotkeys)
	{
		_errorMessage = "Could not save hotkeys element: " + settingsFileName;
		return false;
	}

	hotkeys->DeleteChildren();

	for (int h = 0; h < _states.size(); h++)
	{
		auto thisHotkey = hotkeys->InsertEndChild(doc.NewElement("hotkey"))->ToElement();
		const auto& hkey = _states[h];

		thisHotkey->SetAttribute("key", (int)hkey._key);
		thisHotkey->SetAttribute("ctrl", hkey._ctrl);
		thisHotkey->SetAttribute("shift", hkey._shift);
		thisHotkey->SetAttribute("alt", hkey._alt);
		thisHotkey->SetAttribute("timeout", hkey._timeout);
		thisHotkey->SetAttribute("useTimeout", hkey._useTimeout);
		thisHotkey->SetAttribute("activeType", hkey._activeType);

		thisHotkey->DeleteAttribute("toggle");

		thisHotkey->SetAttribute("schedule", hkey._schedule);
		thisHotkey->SetAttribute("interval", hkey._intervalTime);
		thisHotkey->SetAttribute("variation", hkey._intervalVariation);

		for (auto& state : hkey._layerStates)
		{
			auto thisState = thisHotkey->InsertEndChild(doc.NewElement("state"))->ToElement();
			thisState->SetAttribute("id", state.first.c_str());
			thisState->SetAttribute("state", state.second);
		}
	}

	std::string outFile = settingsFileName;
	if (outFile.find("/") == std::string::npos && outFile.find("\\") == std::string::npos)
	{
		outFile = _appConfig->_appLocation + outFile;
	}

	doc.SaveFile(outFile.c_str());

	if (doc.Error())
	{
		_errorMessage = "Failed to save document: " + outFile;
		return false;
	}

	_lastSavedLocation = outFile;

	return true;
}

bool LayerManager::LoadLayers(const std::string& settingsFileName)
{
	_errorMessage = "";
	tinyxml2::XMLDocument doc;

	//_chatReader.Cleanup();
	//_chatReader.Init();
	//_chatReader.SetUsername("rahisaurus");

	//_chatReader.Refresh();

	doc.LoadFile(settingsFileName.c_str());

	if (doc.Error())
	{
		doc.LoadFile((_appConfig->_appLocation + settingsFileName).c_str());

		if (doc.Error())
		{
			_errorMessage = "Could not read document: " + settingsFileName;
			return false;
		}
	}

	auto root = doc.FirstChildElement("Config");
	if (!root)
	{
		_errorMessage = "Invalid config element: " + settingsFileName;
		return false;
	}

	auto layers = root->FirstChildElement("layers");
	if (!layers)
	{
		_errorMessage = "Invalid layers element: " + settingsFileName;
		return false;
	}

	_statesOrder.clear();
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

		const char* guid = thisLayer->Attribute("id");
		if (!guid)
			break;
		layer._id = guid;

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
		thisLayer->QueryAttribute("breathHeight", &layer._breathMove.y);
		thisLayer->QueryAttribute("breathTime", &layer._breathFrequency);
		thisLayer->QueryAttribute("breathMoveX", &layer._breathMove.x);
		thisLayer->QueryAttribute("breathScaleX", &layer._breathScale.x);
		thisLayer->QueryAttribute("breathScaleY", &layer._breathScale.y);
		if (layer._breathScale.x != layer._breathScale.y)
			layer._breathScaleConstrain = false;

		thisLayer->QueryAttribute("breathCircular", &layer._breathCircular);
		thisLayer->QueryAttribute("breatheWhileTalking", &layer._breatheWhileTalking);

		thisLayer->QueryAttribute("screaming", &layer._scream);
		thisLayer->QueryAttribute("screamThreshold", &layer._screamThreshold);
		thisLayer->QueryAttribute("screamVibrate", &layer._screamVibrate);
		thisLayer->QueryAttribute("screamVibrateAmount", &layer._screamVibrateAmount);

		thisLayer->QueryAttribute("restartAnimsOnVisible", &layer._restartAnimsOnVisible);

		if(const char* idlePth = thisLayer->Attribute("idlePath"))
			layer._idleImagePath = idlePth;
		if (const char* talkPth = thisLayer->Attribute("talkPath"))
			layer._talkImagePath = talkPth;
		if (const char* blkPth = thisLayer->Attribute("blinkPath"))
			layer._blinkImagePath = blkPth;
		if (const char* talkBlkPth = thisLayer->Attribute("talkBlinkPath"))
			layer._talkBlinkImagePath = talkBlkPth;
		if (const char* screamPth = thisLayer->Attribute("screamPath"))
			layer._screamImagePath = screamPth;

		layer._idleImage = _textureMan->GetTexture(layer._idleImagePath, &_errorMessage);
		layer._talkImage = _textureMan->GetTexture(layer._talkImagePath, &_errorMessage);
		layer._blinkImage = _textureMan->GetTexture(layer._blinkImagePath, &_errorMessage);
		layer._talkBlinkImage = _textureMan->GetTexture(layer._talkBlinkImagePath, &_errorMessage);
		layer._screamImage = _textureMan->GetTexture(layer._screamImagePath, &_errorMessage);

		if(layer._idleImage)
			layer._idleSprite->LoadFromTexture(*layer._idleImage, 1, 1, 1, 1);
		if (layer._talkImage)
			layer._talkSprite->LoadFromTexture(*layer._talkImage, 1, 1, 1, 1);
		if(layer._blinkImage)
			layer._blinkSprite->LoadFromTexture(*layer._blinkImage, 1, 1, 1, 1);
		if (layer._talkBlinkImage)
			layer._talkBlinkSprite->LoadFromTexture(*layer._talkBlinkImage, 1, 1, 1, 1);
		if (layer._screamImage)
			layer._screamSprite->LoadFromTexture(*layer._screamImage, 1, 1, 1, 1);

		LoadAnimInfo(thisLayer, &doc, "idleAnim", *layer._idleSprite);
		LoadAnimInfo(thisLayer, &doc, "talkAnim", *layer._talkSprite);
		LoadAnimInfo(thisLayer, &doc, "blinkAnim", *layer._blinkSprite);
		LoadAnimInfo(thisLayer, &doc, "talkBlinkAnim", *layer._talkBlinkSprite);
		LoadAnimInfo(thisLayer, &doc, "screamAnim", *layer._screamSprite);

		thisLayer->QueryAttribute("syncAnims", &layer._animsSynced);

		if (layer._animsSynced)
			layer.SyncAnims(layer._animsSynced);

		LoadColor(thisLayer, &doc, "idleTint", layer._idleTint);
		LoadColor(thisLayer, &doc, "talkTint", layer._talkTint);
		LoadColor(thisLayer, &doc, "blinkTint", layer._blinkTint);
		LoadColor(thisLayer, &doc, "talkBlinkTint", layer._talkBlinkTint);
		LoadColor(thisLayer, &doc, "screamTint", layer._screamTint);

		layer._blinkTimer.restart();
		layer._isBlinking = false;
		layer._blinkVarDelay = GetRandom11() * layer._blinkVariation;

		thisLayer->QueryAttribute("scaleX", &layer._scale.x);
		thisLayer->QueryAttribute("scaleY", &layer._scale.y);
		thisLayer->QueryAttribute("posX", &layer._pos.x);
		thisLayer->QueryAttribute("posY", &layer._pos.y);
		thisLayer->QueryAttribute("rot", &layer._rot);

		thisLayer->QueryAttribute("pivotX", &layer._pivot.x);
		thisLayer->QueryAttribute("pivotY", &layer._pivot.y);

		const char* mpguid = thisLayer->Attribute("motionParent");
		if (mpguid)
			layer._motionParent = mpguid;

		float motionDelayFrames = 0;
		if(thisLayer->QueryAttribute("motionDelay", &motionDelayFrames) == tinyxml2::XML_SUCCESS)
		{
			layer._motionDelay = motionDelayFrames * (1.0 / 60.0);
		}

		thisLayer->QueryAttribute("motionDelayTime", &layer._motionDelay);
		thisLayer->QueryAttribute("hideWithParent", &layer._hideWithParent);
		thisLayer->QueryAttribute("motionDrag", &layer._motionDrag);
		thisLayer->QueryAttribute("motionSpring", &layer._motionSpring);
		thisLayer->QueryAttribute("distanceLimit",&layer._distanceLimit);
		thisLayer->QueryAttribute("rotationEffect", &layer._rotationEffect);

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

	root->QueryAttribute("globalScaleX", &_globalScale.x);
	root->QueryAttribute("globalScaleY", &_globalScale.y);
	root->QueryAttribute("globalPosX", &_globalPos.x);
	root->QueryAttribute("globalPosY", &_globalPos.y);
	root->QueryAttribute("globalRot", &_globalRot);

	auto hotkeys = root->FirstChildElement("hotkeys");
	if (!hotkeys)
	{
		_errorMessage = "Invalid hotkeys element: " + settingsFileName;
		return false;
	}

	_states.clear();

	auto thisHotkey = hotkeys->FirstChildElement("hotkey");
	while (thisHotkey)
	{
		_states.emplace_back(StatesInfo());
		StatesInfo& hkey = _states.back();

		int key;
		thisHotkey->QueryAttribute("key", &key);
		hkey._key = (sf::Keyboard::Key)key;
		thisHotkey->QueryAttribute("ctrl", &hkey._ctrl);
		thisHotkey->QueryAttribute("shift", &hkey._shift);
		thisHotkey->QueryAttribute("alt", &hkey._alt);
		thisHotkey->QueryAttribute("timeout", &hkey._timeout);
		thisHotkey->QueryAttribute("useTimeout", &hkey._useTimeout);

		hkey._activeType = StatesInfo::ActiveType::Toggle;
		bool toggle = true;
		thisHotkey->QueryAttribute("toggle", &toggle);
		if(!toggle)
			hkey._activeType = StatesInfo::ActiveType::Held;
			
		thisHotkey->QueryIntAttribute("activeType", (int*)(& hkey._activeType));

		thisHotkey->QueryAttribute("schedule", &hkey._schedule);
		thisHotkey->QueryAttribute("interval", &hkey._intervalTime);
		thisHotkey->QueryAttribute("variation", &hkey._intervalVariation);

		auto thisLayerState = thisHotkey->FirstChildElement("state");
		while (thisLayerState)
		{
			std::string id;
			bool vis = true;
			int state = StatesInfo::NoChange;

			const char* stateguid = thisLayerState->Attribute("id");
			if (stateguid)
				id = stateguid;

			if (thisLayerState->QueryAttribute("visible", &vis) == tinyxml2::XML_SUCCESS)
			{
				state = (int)vis;
			}

			thisLayerState->QueryIntAttribute("state", &state);

			hkey._layerStates[id] = (StatesInfo::State)state;

			thisLayerState = thisLayerState->NextSiblingElement("state");
		}

		thisHotkey = thisHotkey->NextSiblingElement("hotkey");
	}


	return true;
}

void LayerManager::HandleHotkey(const sf::Keyboard::Key& key, bool keyDown, bool ctrl, bool shift, bool alt)
{
	for (auto& l : _layers)
		if (l._renamePopupOpen)
			return;
	
	for (int h = 0; h < _states.size(); h++)
	{
		auto& hkey = _states[h];
		if (hkey._key == key && hkey._ctrl == ctrl && hkey._shift == shift && hkey._alt == alt)
		{
			if (hkey._active && ((hkey._activeType == StatesInfo::Toggle && keyDown) || (hkey._activeType == StatesInfo::Held && !keyDown)))
			{
				// deactivate
				hkey._active = false;
				RemoveStateFromOrder(&hkey);
			}
			else if (!hkey._active && keyDown)
			{
				if (hkey._activeType == StatesInfo::Permanent)
				{
					// activate immediately & alter the default states
					for (auto& state : hkey._layerStates)
					{
						LayerInfo* layer = GetLayer(state.first);
						if (layer && state.second != StatesInfo::NoChange)
						{
							layer->_visible = state.second;
							const std::string& layerId = layer->_id;
							if (_defaultLayerStates.count(layerId))
								_defaultLayerStates[layerId] = state.second;
						}
					}
					// never "activates" because it can't be undone
					hkey._active = false;
				}
				else
				{
					// activate and add to stack
					SaveDefaultStates();

					for (auto& state : hkey._layerStates)
					{
						LayerInfo* layer = GetLayer(state.first);
						if (layer && state.second != StatesInfo::NoChange)
						{
							layer->_visible = state.second;
						}
					}
					AppendStateToOrder(&hkey);
					hkey._timer.restart();
					hkey._active = true;
					_statesTimer.restart();
				}
			}
			break;
		}
	}

	return;
}

void LayerManager::ResetStates()
{
	if (!AnyStateActive())
		return;

	for (auto& hkey : _states)
		hkey._active = false;

	for (auto& l : _layers)
	{
		if(_defaultLayerStates.count(l._id))
			l._visible = _defaultLayerStates[l._id];
	}
}

void LayerManager::DrawStatesGUI()
{
	if (_statesMenuOpen != _oldStatesMenuOpen)
	{
		_oldStatesMenuOpen = _statesMenuOpen;

		if (_statesMenuOpen)
		{
			auto size = ImGui::GetWindowSize();
			ImGui::SetNextWindowPos({ _appConfig->_scrW / 2 - 200, _appConfig->_scrH / 2 - 200 });
			ImGui::SetNextWindowSize({ 400, 400 });
			ImGui::OpenPopup("States Setup");
		}
	}

	if (ImGui::BeginPopupModal("States Setup", &_statesMenuOpen, ImGuiWindowFlags_NoResize))
	{
		ImGui::TextWrapped("Use Hotkeys or Timers to instantly set the visibility of multiple layers.");

		if(_appConfig->_useKeyboardHooks == false)
			ImGui::TextWrapped("Keyboard Hook is disabled. Hotkeys will not work if Rahituber is not in focus.");

		if (ImGui::Button("Add"))
		{
			_states.push_back(StatesInfo());
			for (auto& l : _layers)
			{
				_states.back()._layerStates[l._id] = StatesInfo::NoChange;
			}
		}
		ToolTip("Add a new state", &_appConfig->_hoverTimer);

		int stateIdx = 0;
		while (stateIdx < _states.size())
		{
			auto& state = _states[stateIdx];

			std::string name = g_key_names[state._key];
			if (state._key == sf::Keyboard::Unknown)
			{
				if (state._schedule)
					name = "";
				else
					name = "No hotkey";
			}				
			if (state._alt)
				name = "Alt, " + name;
			if (state._shift)
				name = "Shift, " + name;
			if (state._ctrl)
				name = "Ctrl, " + name;

			std::string keyName = name;

			if (state._schedule)
			{
				std::stringstream ss;

				if (name != "")
					ss << ", ";

				if (state._intervalVariation > 0)
					ss << std::fixed << std::setprecision(1) << state._intervalTime - state._intervalVariation
					<< " - " << state._intervalTime + state._intervalVariation << "s interval";
				else
					ss << std::fixed << std::setprecision(1) << state._intervalTime << "s interval";

				name += ss.str();
			}

			float contentWidth = ImGui::GetWindowContentRegionMax().x;

			ImVec2 headerTxtPos = { ImGui::GetCursorPosX() + 20, ImGui::GetCursorPosY() + 3 };
			ImVec2 delButtonPos = { ImGui::GetCursorPosX() + (contentWidth-60), ImGui::GetCursorPosY() };

			ImGui::PushID('id' + stateIdx);

			auto col = ImGui::GetStyleColorVec4(ImGuiCol_Header);
			if(state._active)
				col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
			ImGui::PushStyleColor(ImGuiCol_Header, col);
			if (ImGui::CollapsingHeader("", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ImGui::Columns(3, 0, false);
				ImGui::SetColumnWidth(0, 150);
				ImGui::SetColumnWidth(1, 100);
				ImGui::SetColumnWidth(2, 300);
				std::string btnName = keyName;
				if (state._key == sf::Keyboard::Unknown)
					btnName = " Click to\nrecord key";
				if (state._awaitingHotkey)
					btnName = "(press a key)";
				ImGui::PushID("recordKeyBtn");
				if (ImGui::Button(btnName.c_str(), { 140,42 }) && !_waitingForHotkey)
				{
					_pendingKey = sf::Keyboard::Unknown;
					_pendingCtrl = false;
					_pendingShift = false;
					_pendingAlt = false;
					_waitingForHotkey = true;
					state._awaitingHotkey = true;
				}
				ToolTip("Set a hotkey", &_appConfig->_hoverTimer);

				if (ImGui::Button("Clear", {140,20}))
				{
					state._key = sf::Keyboard::Unknown;
					_waitingForHotkey = false;
					state._awaitingHotkey = false;
				}
				ToolTip("Clear the hotkey", &_appConfig->_hoverTimer);

				ImGui::PopID();

				if (state._awaitingHotkey && _waitingForHotkey && _pendingKey != sf::Keyboard::Unknown)
				{
					state._key = _pendingKey;
					state._ctrl = _pendingCtrl;
					state._shift = _pendingShift;
					state._alt = _pendingAlt;
					_waitingForHotkey = false;
					state._awaitingHotkey = false;
				}

				ImGui::NextColumn();

				ImGui::AlignTextToFramePadding();
				ImGui::Text("Active Type:");

				float timeoutpos = ImGui::GetCursorPosY();
				bool whileHeld = state._activeType == StatesInfo::Held;
				bool timeoutActive = (state._useTimeout && !whileHeld) || state._schedule;
				if (state._activeType != StatesInfo::Permanent)
				{
					if (whileHeld)
						ImGui::BeginDisabled();
					if (ImGui::Checkbox("Timeout", &timeoutActive) && !state._schedule && !whileHeld)
					{
						state._useTimeout = timeoutActive;
					}
					ToolTip("Deactivate the state after some time", &_appConfig->_hoverTimer);

					if (whileHeld)
						ImGui::EndDisabled();
					ImGui::Checkbox("Schedule", &state._schedule);
					ToolTip("Automatically enable the state on a timer", &_appConfig->_hoverTimer);
				}
				
				ImGui::NextColumn();
				ImGui::PushItemWidth(100);
				if (ImGui::BeginCombo("##ActiveType", g_activeTypeNames[state._activeType].c_str()))
				{
					for (int atype = 0; atype < g_activeTypeNames.size(); atype++)
					{
						if (ImGui::Selectable(g_activeTypeNames[atype].c_str(), state._activeType == atype))
							state._activeType = (StatesInfo::ActiveType)atype;
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				ToolTip("Change how the state is activated:\
\n- Toggle: The state will switch between active and\n    inactive each time the hotkey is pressed\
\n- While Held: The state will be active while the\n    hotkey is held, and inactive otherwise\
\n- Permanent: The effects of this state will be\n    permanently applied when the hotkey is pressed", &_appConfig->_hoverTimer);

				if (state._activeType != StatesInfo::Permanent)
				{
					ImGui::SetCursorPosY(timeoutpos);
					if (timeoutActive)
					{
						ImGui::PushID("timeoutSlider");
						ImGui::SliderFloat("", &state._timeout, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic);
						ToolTip("How long the state stays active for", &_appConfig->_hoverTimer);
						ImGui::PopID();
					}
				}

				ImGui::Columns();

				ImGui::Columns(2, 0, false);
				ImGui::SetColumnWidth(0, 150);
				ImGui::SetColumnWidth(1, 400);
				ImGui::NextColumn();
				if (state._schedule && state._activeType != StatesInfo::Permanent)
				{
					ImGui::SliderFloat("Interval", &state._intervalTime, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic);
					ToolTip("Sets how long the state will be inactive before reactivating", &_appConfig->_hoverTimer);
					ImGui::SliderFloat("Variation", &state._intervalVariation, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic);
					ToolTip("Adds a random variation to the Interval.\nThis sets the maximum variation.", &_appConfig->_hoverTimer);
				}
				ImGui::Columns();

				ImGui::Separator();

				ImGui::Columns(4, "hotkeystates", true);

				int layerIdx = 0;
				for (auto& l : _layers)
				{
					ImGui::PushID(l._id.c_str());
					ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_BorderShadow);

					if (++layerIdx % 2)
						ImGui::DrawRectFilled(sf::FloatRect(0, 0, 400, 20), toSFColor(col));

					ImGui::AlignTextToFramePadding();
					ImGui::Text(l._name.c_str());

					if (state._layerStates.count(l._id) == 0)
						state._layerStates[l._id] = StatesInfo::NoChange;

					ImGui::NextColumn();
					if (layerIdx % 2)
						ImGui::DrawRectFilled(sf::FloatRect(0, 0, 400, 20), toSFColor(col));
					ImGui::RadioButton("Show", (int*)&state._layerStates[l._id], (int)StatesInfo::Show);
					ToolTip("Show this layer when the state is activated", &_appConfig->_hoverTimer);
					ImGui::NextColumn();
					if (layerIdx % 2)
						ImGui::DrawRectFilled(sf::FloatRect(0, 0, 400, 20), toSFColor(col));
					ImGui::RadioButton("Hide", (int*)&state._layerStates[l._id], (int)StatesInfo::Hide);
					ToolTip("Hide this layer when the state is activated", &_appConfig->_hoverTimer);
					ImGui::NextColumn();
					if (layerIdx % 2)
						ImGui::DrawRectFilled(sf::FloatRect(0, 0, 400, 20), toSFColor(col));
					ImGui::RadioButton("No Change", (int*)&state._layerStates[l._id], (int)StatesInfo::NoChange);
					ToolTip("Do not affect this layer when the state is activated", &_appConfig->_hoverTimer);
					ImGui::NextColumn();
					ImGui::PopID();
				}

				ImGui::Columns();
			}
			ImGui::PopStyleColor();

			ImVec2 endHeaderPos = ImGui::GetCursorPos();

			ImGui::SetCursorPos(headerTxtPos);
			ImGui::Text(name.c_str());

			ImGui::SetCursorPos(delButtonPos);
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
			ImGui::PushStyleColor(ImGuiCol_Text, { 255.f / 255,200.f / 255,170.f / 255, 1.f });
			if (ImGui::Button("Delete"))
			{
				RemoveStateFromOrder(&state);
				_states.erase(_states.begin() + stateIdx);
			}
			ImGui::PopStyleColor(4);
			ToolTip("Delete this state", &_appConfig->_hoverTimer);
			ImGui::PopID();

			ImGui::SetCursorPos(endHeaderPos);

			if (stateIdx >= _states.size())
				break;

			stateIdx++;
		}

		ImGui::EndPopup();
	}
}

void LayerManager::LayerInfo::CalculateDraw(float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	sf::Time frameTime = _frameTimer.restart();
	float fps = 1.0 / frameTime.asSeconds();

	SpriteSheet* lastActiveSprite = _activeSprite;

	_activeSprite = nullptr;

	if (!_idleSprite)
		return;
	
	_idleSprite->_visible = true;
	_blinkSprite->_visible = false;
	_talkSprite->_visible = false;
	_talkBlinkSprite->_visible = false;
	_screamSprite->_visible = false;

	_activeSprite = _idleSprite.get();
	_idleSprite->SetColor(_idleTint);

	float talkFactor = 0;
	if (talkMax > 0)
	{
		talkFactor = talkLevel / talkMax;
		talkFactor = pow(talkFactor, 0.5);
		_lastTalkFactor = talkFactor;
	}

	bool reallyVisible = _visible;
	if (_hideWithParent)
	{
		LayerInfo* mp = _parent->GetLayer(_motionParent);
		if (mp)
			reallyVisible &= mp->_visible;
	}

	bool becameVisible = (reallyVisible == true)&&(_oldVisible == false);

	if (becameVisible && _restartAnimsOnVisible)
	{
		if (_talkBlinkSprite)
			_talkBlinkSprite->Restart();
		if(_blinkSprite)
			_blinkSprite->Restart();
		if(_talkSprite)
			_talkSprite->Restart();
		if(_idleSprite)
			_idleSprite->Restart();
		if(_screamSprite)
			_screamSprite->Restart();
	}
	_oldVisible = reallyVisible;

	bool screaming = _scream && talkFactor > _screamThreshold;
	bool talking = !screaming && talkFactor > _talkThreshold;

	bool blinkAvailable = _blinkImage && !talking && !screaming;
	bool talkBlinkAvailable = _blinkWhileTalking && _talkBlinkImage && talking && !screaming;

	bool canStartBlinking = (talkBlinkAvailable || blinkAvailable) && !_isBlinking && _useBlinkFrame;

	if (canStartBlinking && _blinkTimer.getElapsedTime().asSeconds() > _blinkDelay + _blinkVarDelay)
	{
		_isBlinking = true;
		_blinkTimer.restart();
		if(!_blinkSprite->IsSynced())
			_blinkSprite->Restart();
		_blinkVarDelay = GetRandom11() * _blinkVariation;
	}

	if (_isBlinking)
	{
		if (talkBlinkAvailable)
		{
			_activeSprite = _talkBlinkSprite.get();
			_talkBlinkSprite->_visible = true;
			_blinkSprite->_visible = false;
			_talkSprite->_visible = false;
			_idleSprite->_visible = false;
			_screamSprite->_visible = false;
			_talkBlinkSprite->SetColor(_talkBlinkTint);
		}
		else if (blinkAvailable)
		{
			_activeSprite = _blinkSprite.get();
			_blinkSprite->_visible = true;
			_talkBlinkSprite->_visible = false;
			_talkSprite->_visible = false;
			_idleSprite->_visible = false;
			_screamSprite->_visible = false;
			_blinkSprite->SetColor(_blinkTint);
		}

		if (_blinkTimer.getElapsedTime().asSeconds() > _blinkDuration)
			_isBlinking = false;
	}

	if (_talkImage && !_isBlinking && _swapWhenTalking && talking)
	{
		_activeSprite = _talkSprite.get();
		_screamSprite->_visible = false;
		_idleSprite->_visible = false;
		_blinkSprite->_visible = false;
		_talkSprite->_visible = true;
		_talkSprite->SetColor(_talkTint);
	}
	else if (_screamImage && screaming)
	{
		_activeSprite = _screamSprite.get();
		_screamSprite->_visible = true;
		_idleSprite->_visible = false;
		_blinkSprite->_visible = false;
		_talkSprite->_visible = false;
		_screamSprite->SetColor(_screamTint);
	}

	if (_motionParent == "" || _motionParent == "-1")
	{

		sf::Vector2f pos;

		float newMotionY = 0;
		float newMotionX = 0;

		switch (_bounceType)
		{
		case LayerManager::LayerInfo::BounceNone:
			break;
		case LayerManager::LayerInfo::BounceLoudness:
			_isBouncing = false;
			if (talking || screaming)
			{
				_isBouncing = true;
				newMotionY += _bounceHeight * std::fmax(0.f, (talkFactor - _talkThreshold) / (1.0f - _talkThreshold));
			}
			break;
		case LayerManager::LayerInfo::BounceRegular:
			if ((talking || screaming) && _bounceFrequency > 0)
			{
				if (!_isBouncing)
				{
					_isBouncing = true;
					_motionTimer.restart();
				}

				float motionTime = _motionTimer.getElapsedTime().asSeconds();
				motionTime -= floor(motionTime / _bounceFrequency) * _bounceFrequency;
				float phase = (motionTime / _bounceFrequency) * 2.0 * PI;
				newMotionY = (-0.5 * cos(phase) + 0.5) * _bounceHeight;
			}
			else
				_isBouncing = false;

			break;
		default:
			break;
		}

		sf::Vector2f breathScale = {1.0,1.0};

		if (_doBreathing)
		{
			bool talkActive = (talking && _swapWhenTalking || _isBouncing) && !_breatheWhileTalking;

			if ( !talkActive && _breathFrequency > 0)
			{
				if (!_isBreathing)
				{
					_motionTimer.restart();
					_isBreathing = true;
				}
				float motionTime = _motionTimer.getElapsedTime().asSeconds();

				bool coolingDown = motionTime < _breathFrequency && !_breatheWhileTalking;
				float coolDownFactor = (_breathFrequency - motionTime) / _breathFrequency;

				motionTime = fmod(motionTime , _breathFrequency);
				float phase = (motionTime / _breathFrequency) * 2.0 * PI;
				if (coolingDown)
				{
					_breathAmount.x = std::max(0.0f, _breathAmount.x * coolDownFactor);
					_breathAmount.x = std::max(_breathAmount.x, (-0.5f * cos(phase) + 0.5f));
					_breathAmount.y = std::max(0.0f, _breathAmount.y * coolDownFactor);
					if(_breathCircular)
						_breathAmount.y = std::max(_breathAmount.y, (-0.5f * sin(phase) + 0.5f));
					else
						_breathAmount.y = std::max(_breathAmount.y, (-0.5f * cos(phase) + 0.5f));
				}
				else
				{
					_breathAmount.x = (-0.5f * cos(phase) + 0.5f);
					if(_breathCircular)
						_breathAmount.y = (-0.5f * sin(phase) + 0.5f);
					else
						_breathAmount.y = (-0.5f * cos(phase) + 0.5f);
				}
			}
			else
			{
				if (_isBreathing)
				{
					_motionTimer.restart();
					_isBreathing = false;
				}
				float motionTime = _motionTimer.getElapsedTime().asSeconds();
				float coolDownFactor = (_breathFrequency - motionTime) / _breathFrequency;

				_breathAmount.x = std::max(0.0f, _breathAmount.x * coolDownFactor);
				_breathAmount.y = std::max(0.0f, _breathAmount.y * coolDownFactor);
			}

			newMotionX += _breathAmount.x * _breathMove.x;
			newMotionY += _breathAmount.y * _breathMove.y;

			breathScale = sf::Vector2f(1.0, 1.0) + _breathAmount.y * _breathScale;
		}

		_motionY += (newMotionY - _motionY) * 0.3f;
		_motionX += (newMotionX - _motionX) * 0.3f;

		pos.x += _motionX;
		pos.y -= _motionY;

		if (screaming && _screamVibrate)
		{
			if (!_isScreaming)
			{
				_motionTimer.restart();
				_isScreaming = true;
			}

			float motionTime = _motionTimer.getElapsedTime().asSeconds();
			pos.y += sin(motionTime / 0.02) * _screamVibrateAmount;
			pos.x += sin(motionTime / 0.05) * _screamVibrateAmount;
		}
		else
		{
			_isScreaming = false;
		}

		_activeSprite->setOrigin({ _pivot.x * _activeSprite->Size().x, _pivot.y * _activeSprite->Size().y });
		_activeSprite->setScale({ _scale.x * breathScale.x, _scale.y * breathScale.y });
		_activeSprite->setPosition({ windowWidth / 2 + _pos.x + pos.x, windowHeight / 2 + _pos.y + pos.y });
		_activeSprite->setRotation(_rot);

		MotionLinkData thisFrame;
		thisFrame._frameTime = frameTime;
		thisFrame._pos = pos;
		thisFrame._scale = breathScale;
		thisFrame._rot = _rot;
		_motionLinkData.push_front(thisFrame);

		sf::Time totalMotionStoredTime;
		for (auto& frame : _motionLinkData)
			totalMotionStoredTime += frame._frameTime;

		while (totalMotionStoredTime > sf::seconds(1.1))
		{
			totalMotionStoredTime -= _motionLinkData.back()._frameTime;
			_motionLinkData.pop_back();
		}
	}
	else
	{
		LayerInfo* mp = _parent->GetLayer(_motionParent);
		if (mp)
		{
			float motionDelayNow = _motionDelay;
			if (motionDelayNow < 0)
				motionDelayNow = 0;

			sf::Vector2f mpScale = { 1.0,1.0 };
			sf::Vector2f mpPos;
			float mpRot = 0;

			if (motionDelayNow > 0)
			{
				sf::Time totalParentStoredTime;
				for (auto& frame : mp->_motionLinkData)
					totalParentStoredTime += frame._frameTime;

				if (motionDelayNow > totalParentStoredTime.asSeconds())
					motionDelayNow = totalParentStoredTime.asSeconds();

				size_t prev = 0;
				size_t next = 0;
				sf::Time cumulativeTime;
				sf::Time prevCumulativeTime;
				size_t idx = 0;
				for (auto& frame : mp->_motionLinkData)
				{
					if (cumulativeTime.asSeconds() > motionDelayNow)
					{
						next = idx;
						break;
					}

					if (cumulativeTime.asSeconds() <= motionDelayNow)
						prev = idx;

					prevCumulativeTime = cumulativeTime;
					cumulativeTime += frame._frameTime;
					idx++;
				}

				if (mp->_motionLinkData.size() > next)
				{
					float frameDuration = mp->_motionLinkData[next]._frameTime.asSeconds();
					float framePosition = motionDelayNow - prevCumulativeTime.asSeconds();
					float fraction = framePosition / frameDuration;
					mpScale = mp->_motionLinkData[prev]._scale + fraction * (mp->_motionLinkData[next]._scale - mp->_motionLinkData[prev]._scale);
					mpPos = mp->_motionLinkData[prev]._pos + fraction * (mp->_motionLinkData[next]._pos - mp->_motionLinkData[prev]._pos);
					mpRot = mp->_motionLinkData[prev]._rot + fraction * (mp->_motionLinkData[next]._rot - mp->_motionLinkData[prev]._rot);
				}
			}
			else if (mp->_motionLinkData.size() > 0)
			{
				mpScale = mp->_motionLinkData[0]._scale;
				mpPos = mp->_motionLinkData[0]._pos;
				mpRot = mp->_motionLinkData[0]._rot;
			}

			// transform this layer's position by the parent position
			sf::Vector2f originalOffset = _pos - mp->_pos;
			sf::Vector2f originalOffsetRotated = Rotate(originalOffset, Deg2Rad(mpRot));
			sf::Vector2f offsetScaled = { originalOffsetRotated.x * mpScale.x, originalOffsetRotated.y * mpScale.y };
			sf::Vector2f originMove = offsetScaled - originalOffset;

			mpPos += originMove;
			
			sf::Vector2f pivot = { _pivot.x * _activeSprite->Size().x, _pivot.y * _activeSprite->Size().y };

			bool physics = (_motionDrag > 0 || _motionSpring > 0);
			sf::Vector2f physicsPos = mpPos;

			if (physics && becameVisible)
			{
				_lastAccel = { 0.f, 0.f };
				_physicsTimer.restart();
			}
			else if (physics && lastActiveSprite != nullptr && _motionLinkData.size() > 0)
			{
				float motionDrag = _motionDrag;
				float motionSpring = _motionSpring;
				float fadeIn = _physicsTimer.getElapsedTime().asSeconds() / 0.5;
				if (fadeIn < 1.0)
				{
					motionDrag = _motionDrag * fadeIn;
					motionSpring = _motionSpring* fadeIn;
				}

				auto lastFrame = _motionLinkData.front();
				sf::Vector2f oldScale = lastFrame._scale;
				sf::Vector2f oldPos = lastFrame._physicsPos;
				float oldRot = lastFrame._rot;

				sf::Vector2f idealAccel = mpPos - oldPos;
				sf::Vector2f accel = _lastAccel + (idealAccel - _lastAccel) * (1.0f - motionSpring);
				sf::Vector2f newMpPos = oldPos + (1.0f - motionDrag) * accel;
				_lastAccel = accel;

				sf::Vector2f offset = mpPos - newMpPos;
				float dist = Length(offset);
				
				sf::Vector2f pivotDiff = _pivot - sf::Vector2f(.5f, .5f);
				pivotDiff = Rotate(pivotDiff, Deg2Rad(mpRot));
				float lenPivot = Length(pivotDiff);
				if (lenPivot > 0 && dist > 0 && _rotationEffect > 0)
				{
					float rotMult = Dot(offset, pivotDiff) / (dist * lenPivot);

					mpRot += _rotationEffect * rotMult * (dist * lenPivot);
				}

				physicsPos = newMpPos;

				if (_distanceLimit == 0.f)
				{
					newMpPos = mpPos;
				}
				else if (_distanceLimit > 0.f && dist > _distanceLimit)
				{
					sf::Vector2f offsetNorm = offset / dist;
					newMpPos = mpPos + offsetNorm * _distanceLimit;
				}

				mpPos = newMpPos;
			}

			MotionLinkData thisFrame;
			thisFrame._frameTime = frameTime;
			thisFrame._pos = mpPos;
			thisFrame._physicsPos = physicsPos;
			thisFrame._scale = mpScale;
			thisFrame._rot = mpRot;
			_motionLinkData.push_front(thisFrame);
			
			sf::Time totalMotionStoredTime;
			for (auto& frame : _motionLinkData)
				totalMotionStoredTime += frame._frameTime;

			while (totalMotionStoredTime > sf::seconds(1.1))
			{
				totalMotionStoredTime -= _motionLinkData.back()._frameTime;
				_motionLinkData.pop_back();
			}
			
			_activeSprite->setOrigin(pivot);
			_activeSprite->setScale({ _scale.x * mpScale.x, _scale.y * mpScale.y });
			_activeSprite->setPosition({ windowWidth / 2 + _pos.x + mpPos.x, windowHeight / 2 + _pos.y + mpPos.y });
			_activeSprite->setRotation(_rot + mpRot);
		}
	}

}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{

	if (_animIcon == nullptr)
		_animIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/anim.png");

	if (_emptyIcon == nullptr)
		_emptyIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/empty.png");

	if (_upIcon == nullptr)
		_upIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/arrowup.png");

	if (_dnIcon == nullptr)
		_dnIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/arrowdn.png");

	if (_editIcon == nullptr)
		_editIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/edit.png");

	if (_delIcon == nullptr)
		_delIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/delete.png");

	if (_dupeIcon == nullptr)
		_dupeIcon = _parent->_textureMan->GetTexture(_parent->_appConfig->_appLocation + "res/duplicate.png");

	//_dupeIcon->setSmooth(true);
	_delIcon->setSmooth(true);
	_editIcon->setSmooth(true);
	_emptyIcon->setSmooth(true);

	ImVec4 col = style.Colors[ImGuiCol_Text];
	sf::Color btnColor = { sf::Uint8(255*col.x), sf::Uint8(255*col.y), sf::Uint8(255*col.z) };

	ImGui::PushID(_id.c_str());
	std::string name = "[" + std::to_string(layerID) + "] " + _name;
	sf::Vector2f headerBtnSize(17, 17);
	ImVec2 headerButtonsPos = { ImGui::GetWindowWidth() - headerBtnSize.x*8, ImGui::GetCursorPosY()};

	float indentSize = 8;

	if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowItemOverlap))
	{
		ImGui::Indent(indentSize);

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
		ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
		if (_importIdleOpen && _idleImage)
			fileBrowserIdle.SetStartingDir(_idleImagePath);
		if (fileBrowserIdle.render(_importIdleOpen, _idleImagePath))
		{
			_idleImage = _parent->_textureMan->GetTexture(_idleImagePath, &_parent->_errorMessage);
			_idleImage->setSmooth(_scaleFiltering);
			_idleSprite->LoadFromTexture(*_idleImage, 1, 1, 1, 1);
		}

		ImGui::SameLine(imgBtnWidth + 16);
		ImGui::PushID("idleanimbtn");
		_spriteIdleOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
		ToolTip("Animation settings", &_parent->_appConfig->_hoverTimer);
		AnimPopup(*_idleSprite, _spriteIdleOpen, _oldSpriteIdleOpen);
		ImGui::PopID();

		fs::path chosenDir = fileBrowserIdle.GetLastChosenDir();

		ImGui::PushID("idleimportfile");
		char idlebuf[256] = "                           ";
		_idleImagePath.copy(idlebuf, 256);
		if (ImGui::InputText("", idlebuf, 256, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
		{
			_idleImagePath = idlebuf;
			_idleImage = _parent->_textureMan->GetTexture(_idleImagePath, &_parent->_errorMessage);
			if (_idleImage)
			{
				_idleImage->setSmooth(_scaleFiltering);
				_idleSprite->LoadFromTexture(*_idleImage, 1, 1, 1, 1);
			}
		}
		ImGui::PopID();
		ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);
		

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
			ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
			fileBrowserTalk.SetStartingDir(chosenDir);
			if (_talkImage)
				fileBrowserTalk.SetStartingDir(_talkImagePath);
			if (fileBrowserTalk.render(_importTalkOpen, _talkImagePath))
			{
				_talkImage = _parent->_textureMan->GetTexture(_talkImagePath, &_parent->_errorMessage);
				if (_talkImage)
				{
					_talkImage->setSmooth(_scaleFiltering);
					_talkSprite->LoadFromTexture(*_talkImage, 1, 1, 1, 1);
				}
			}

			ImGui::SameLine(imgBtnWidth + 16);
			ImGui::PushID("talkanimbtn");
			_spriteTalkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
			AnimPopup(*_talkSprite, _spriteTalkOpen, _oldSpriteTalkOpen);
			ImGui::PopID();

			ImGui::PushID("talkimportfile");
			char talkbuf[256] = "                           ";
			_talkImagePath.copy(talkbuf, 256);
			if (ImGui::InputText("", talkbuf, 256, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				_talkImagePath = talkbuf;
				_talkImage = _parent->_textureMan->GetTexture(_talkImagePath, &_parent->_errorMessage);
				if (_talkImage)
				{
					_talkImage->setSmooth(_scaleFiltering);
					_talkSprite->LoadFromTexture(*_talkImage, 1, 1, 1, 1);
				}
			}
			ImGui::PopID();
			ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

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
			ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
			fileBrowserBlink.SetStartingDir(chosenDir);
			if (_blinkImage)
				fileBrowserBlink.SetStartingDir(_blinkImagePath);
			if (fileBrowserBlink.render(_importBlinkOpen, _blinkImagePath))
			{
				_blinkImage = _parent->_textureMan->GetTexture(_blinkImagePath, &_parent->_errorMessage);
				if (_blinkImage)
				{
					_blinkImage->setSmooth(_scaleFiltering);
					_blinkSprite->LoadFromTexture(*_blinkImage, 1, 1, 1, 1);
				}
			}

			ImGui::SameLine(blinkBtnSize.x + 16);
			ImGui::PushID("blinkanimbtn");
			_spriteBlinkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
			AnimPopup(*_blinkSprite, _spriteBlinkOpen, _oldSpriteBlinkOpen);
			ImGui::PopID();

			ImGui::PushID("blinkimportfile");
			char blinkbuf[256] = "                           ";
			_blinkImagePath.copy(blinkbuf, 256);
			if (ImGui::InputText("", blinkbuf, 256, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				_blinkImagePath = blinkbuf;
				_blinkImage = _parent->_textureMan->GetTexture(_blinkImagePath, &_parent->_errorMessage);
				if (_blinkImage)
				{
					_blinkImage->setSmooth(_scaleFiltering);
					_blinkSprite->LoadFromTexture(*_blinkImage, 1, 1, 1, 1);
				}
			}
			ImGui::PopID();
			ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

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
				ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
				fileBrowserBlink.SetStartingDir(chosenDir);
				if (_talkBlinkImage)
					fileBrowserBlink.SetStartingDir(_talkBlinkImagePath);
				if (fileBrowserBlink.render(_importTalkBlinkOpen, _talkBlinkImagePath))
				{
					_talkBlinkImage = _parent->_textureMan->GetTexture(_talkBlinkImagePath, &_parent->_errorMessage);
					if (_talkBlinkImage)
					{
						_talkBlinkImage->setSmooth(_scaleFiltering);
						_talkBlinkSprite->LoadFromTexture(*_talkBlinkImage, 1, 1, 1, 1);
					}
				}

				ImGui::SameLine(blinkBtnSize.x + 16);
				ImGui::PushID("talkblinkanimbtn");
				_spriteTalkBlinkOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
				ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
				AnimPopup(*_talkBlinkSprite, _spriteTalkBlinkOpen, _oldSpriteTalkBlinkOpen);
				ImGui::PopID();

				ImGui::PushID("talkblinkimportfile");
				char talkblinkbuf[256] = "                           ";
				_talkBlinkImagePath.copy(talkblinkbuf, 256);
				if (ImGui::InputText("", talkblinkbuf, 256, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
				{
					_talkBlinkImagePath = talkblinkbuf;
					_talkBlinkImage = _parent->_textureMan->GetTexture(_talkBlinkImagePath, &_parent->_errorMessage);
					if (_talkBlinkImage)
					{
						_talkBlinkImage->setSmooth(_scaleFiltering);
						_talkBlinkSprite->LoadFromTexture(*_talkBlinkImage, 1, 1, 1, 1);
					}
				}
				ImGui::PopID();
				ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

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
			if (_screamImage)
				_screamImage->setSmooth(_scaleFiltering);
		}
		ImGui::PopItemWidth();
		ToolTip("On: Smooth pixel interpolation when the image is not actual size\nOff: Nearest-neighbour interpolation, sharp edges at any size", &_parent->_appConfig->_hoverTimer);

		ImGui::Checkbox("Restart anims on becoming visible", &_restartAnimsOnVisible);
		
		ImGui::Separator();

		AddResetButton("talkThresh", _talkThreshold, 0.15f, _parent->_appConfig, &style);
		ImVec2 barPos = ImGui::GetCursorPos();
		ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0, "%.3f");
		ImGui::NewLine();
		ToolTip("The audio level needed to trigger the talking state", &_parent->_appConfig->_hoverTimer);

		sf::Color barHighlight(60, 140, 60, 255);
		sf::Color barBg(20, 60, 20, 255);
		if (_lastTalkFactor < 0.001 || _lastTalkFactor < _talkThreshold)
		{
			barHighlight = sf::Color(140, 60, 60, 255);
			barBg = sf::Color(60, 20, 20, 255);
		}

		sf::Vector2f topLeft = { barPos.x, barPos.y };
		float barWidth = (ImGui::GetWindowWidth() - topLeft.x) - 148;
		float barHeight = 10;
		sf::FloatRect volumeBarBg({ topLeft.x, -18 }, { barWidth, barHeight });
		ImGui::DrawRectFilled(volumeBarBg, barBg, 3);
		float activeBarWidth = barWidth * _lastTalkFactor;
		sf::FloatRect volumeBar({ topLeft.x, -18 }, { activeBarWidth, barHeight });
		ImGui::DrawRectFilled(volumeBar, barHighlight, 3);
		float rootThresh = _talkThreshold;
		float thresholdPos = barWidth * rootThresh;
		sf::FloatRect thresholdBar({ topLeft.x + thresholdPos, -23 }, { 2, barHeight + 5 });
		ImGui::DrawRectFilled(thresholdBar, {200,150,80});
		
		ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);
		ToolTip("Swap to the 'talk' sprite when Talk Threshold is reached", &_parent->_appConfig->_hoverTimer);

		ImVec2 subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
		if (ImGui::CollapsingHeader("Screaming", ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			ImGui::Indent(indentSize);

			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Scream");
			ImGui::PushID("screamimport");
			sf::Color screamCol = _screamImage == nullptr ? btnColor : sf::Color::White;
			sf::Texture* talkIcon = _screamImage == nullptr ? _emptyIcon : _screamImage;
			_importScreamOpen = ImGui::ImageButton(*talkIcon, { imgBtnWidth,imgBtnWidth }, -1, sf::Color::Transparent, screamCol);
			ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
			fileBrowserTalk.SetStartingDir(chosenDir);
			if (_screamImage)
				fileBrowserTalk.SetStartingDir(_screamImagePath);
			if (fileBrowserTalk.render(_importScreamOpen, _screamImagePath))
			{
				_screamImage = _parent->_textureMan->GetTexture(_screamImagePath);
				if (_screamImage)
				{
					_screamImage->setSmooth(_scaleFiltering);
					_screamSprite->LoadFromTexture(*_screamImage, 1, 1, 1, 1);
				}
			}

			ImGui::SameLine(imgBtnWidth + 16);
			ImGui::PushID("screamanimbtn");
			_spriteScreamOpen |= ImGui::ImageButton(*_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
			AnimPopup(*_screamSprite, _spriteScreamOpen, _oldSpriteScreamOpen);
			ImGui::PopID();

			ImGui::PushID("screamimportfile");
			char screambuf[256] = "                           ";
			_screamImagePath.copy(screambuf, 256);
			if (ImGui::InputText("", screambuf, 256, ImGuiInputTextFlags_AutoSelectAll))
			{
				_screamImagePath = screambuf;
				_screamImage = _parent->_textureMan->GetTexture(_screamImagePath);
				if (_screamImage)
				{
					_screamImage->setSmooth(_scaleFiltering);
					_screamSprite->LoadFromTexture(*_screamImage, 1, 1, 1, 1);
				}
			}
			ImGui::PopID();
			ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

			ImGui::ColorEdit4("Tint", _screamTint, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
			ToolTip("Tint the sprite a different color, or change its opacity (alpha value)", &_parent->_appConfig->_hoverTimer);
			ImGui::PopID();

			float resetW = 22;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			ImGui::Columns(2, "scrm", false);
			ImGui::SetColumnWidth(0, resetW);
			ImGui::PopStyleVar(2);
			AddResetButton("screamThresh", _screamThreshold, 0.15f, _parent->_appConfig, &style);
			ImGui::NextColumn();
			ImVec2 barPos = ImGui::GetCursorPos();
			ImGui::SliderFloat("Scream Threshold", &_screamThreshold, 0.0, 1.0, "%.3f");
			ToolTip("The audio level needed to trigger the screaming state", &_parent->_appConfig->_hoverTimer);
			ImGui::Columns(1);
			ImGui::NewLine();

			sf::Color barHighlight(60, 140, 60, 255);
			sf::Color barBg(20, 60, 20, 255);
			if (_lastTalkFactor < 0.001 || _lastTalkFactor < _screamThreshold)
			{
				barHighlight = sf::Color(140, 60, 60, 255);
				barBg = sf::Color(60, 20, 20, 255);
			}

			sf::Vector2f topLeft = { barPos.x - 9, barPos.y };
			float barWidth = (ImGui::GetColumnWidth() - topLeft.x) - (150);
			float barHeight = 10;
			sf::FloatRect volumeBarBg({ topLeft.x, -18 }, { barWidth, barHeight });
			ImGui::DrawRectFilled(volumeBarBg, barBg, 3);
			float activeBarWidth = barWidth * _lastTalkFactor;
			sf::FloatRect volumeBar({ topLeft.x, -18 }, { activeBarWidth, barHeight });
			ImGui::DrawRectFilled(volumeBar, barHighlight, 3);
			float rootThresh = _screamThreshold;
			float thresholdPos = barWidth * rootThresh;
			sf::FloatRect thresholdBar({ topLeft.x + thresholdPos, -23 }, { 2, barHeight + 5 });
			ImGui::DrawRectFilled(thresholdBar, { 200,150,80 });

			ImGui::Checkbox("Vibrate", &_screamVibrate);
			ToolTip("Randomly shake the sprite whilst screaming", &_parent->_appConfig->_hoverTimer);
			ImGui::SliderFloat("Vibrate Amount", &_screamVibrateAmount, 0.0, 50.0, "%.1f px");
			ToolTip("The distance of the vibration", &_parent->_appConfig->_hoverTimer);

			ImGui::Unindent(indentSize);
		}
		ToolTip("Swap to a different sprite when reaching a second audio threshold", &_parent->_appConfig->_hoverTimer);
		auto oldCursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(subHeaderBtnPos);
		ImGui::Checkbox("##Scream", &_scream);
		ToolTip("Swap to a different sprite when reaching a second audio threshold", &_parent->_appConfig->_hoverTimer);
		ImGui::SetCursorPos(oldCursorPos);

		subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
		if (ImGui::CollapsingHeader("Blinking", ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			ImGui::Indent(indentSize);
			ImGui::Checkbox("Blink While Talking", &_blinkWhileTalking);
			ToolTip("Show another blinking sprite whilst talking", &_parent->_appConfig->_hoverTimer);
			AddResetButton("blinkdur", _blinkDuration, 0.2f, _parent->_appConfig, &style);
			ImGui::SliderFloat("Blink Duration", &_blinkDuration, 0.0, 10.0, "%.2f s");
			ToolTip("The amount of time to show the blinking sprite", &_parent->_appConfig->_hoverTimer);
			AddResetButton("blinkdelay", _blinkDelay, 6.f, _parent->_appConfig, &style);
			ImGui::SliderFloat("Blink Delay", &_blinkDelay, 0.0, 10.0, "%.2f s");
			ToolTip("The amount of time between blinks", &_parent->_appConfig->_hoverTimer);
			AddResetButton("blinkvar", _blinkVariation, 4.f, _parent->_appConfig, &style);
			ImGui::SliderFloat("Variation", &_blinkVariation, 0.0, 5.0, "%.2f s");
			ToolTip("Adds a random variation to the Blink Delay.\nThis sets the maximum variation allowed.", &_parent->_appConfig->_hoverTimer);
			ImGui::Unindent(indentSize);
		}
		ToolTip("Show a blinking sprite at random intervals", &_parent->_appConfig->_hoverTimer);
		oldCursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(subHeaderBtnPos);
		ImGui::Checkbox("##Blink", &_useBlinkFrame);
		ToolTip("Show a blinking sprite at random intervals", &_parent->_appConfig->_hoverTimer);
		ImGui::SetCursorPos(oldCursorPos);

		subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
		if (ImGui::CollapsingHeader("Motion Inherit", ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			ImGui::Indent(indentSize);
			if (_motionParent != "")
			{
				float md = _motionDelay;
				AddResetButton("motionDelay", _motionDelay, 0.f, _parent->_appConfig, &style);
				if (ImGui::SliderFloat("Delay", &md, 0.0, 1.0, "%.2f s"))
					_motionDelay = Clamp(md, 0.0, 1.0);

				ToolTip("The time before this layer follows the parent's motion", &_parent->_appConfig->_hoverTimer);

				AddResetButton("motionDrag", _motionDrag, 0.f, _parent->_appConfig, &style);
				if(ImGui::SliderFloat("Drag", &_motionDrag, 0.f, .999f, "%.2f"))
					_motionDrag = Clamp(_motionDrag, 0.f, .999f);
				ToolTip("Makes the layer slower to reach its target position", &_parent->_appConfig->_hoverTimer);

				AddResetButton("motionSpring", _motionSpring, 0.f, _parent->_appConfig, &style);
				if(ImGui::SliderFloat("Spring", &_motionSpring, 0.f, .999f, "%.2f"))
					_motionSpring = Clamp(_motionSpring, 0.f, .999f);
				ToolTip("Makes the layer slower to change direction", &_parent->_appConfig->_hoverTimer);

				AddResetButton("motionDistLimit", _distanceLimit, -1.f, _parent->_appConfig, &style);
				ImGui::SliderFloat("Distance limit", &_distanceLimit, -1.0, 500.f, "%.1f");
				ToolTip("Limits how far this layer can stray from the parent's position\n(Set to -1 for no limit)", &_parent->_appConfig->_hoverTimer);

				AddResetButton("rotationEffect", _rotationEffect, 0.f, _parent->_appConfig, &style);
				ImGui::SliderFloat("Rotation effect", &_rotationEffect, 0.f, 2.f, "%.2f");
				ToolTip("The amount of rotation to apply\n(based on the pivot point's distance from the layer's center)", &_parent->_appConfig->_hoverTimer);
			}

			ImGui::Checkbox("Hide with Parent", &_hideWithParent);
			ToolTip("Hide this layer when the parent is hidden.", &_parent->_appConfig->_hoverTimer);
			ImGui::Unindent(indentSize);
		}
		ToolTip("Copy the motion of another layer", &_parent->_appConfig->_hoverTimer);
		oldCursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos(subHeaderBtnPos);
		LayerInfo* oldMp = _parent->GetLayer(_motionParent);
		std::string mpName = oldMp ? oldMp->_name : "Off";
		ImGui::PushItemWidth(headerBtnSize.x * 7);
		if (ImGui::BeginCombo("##MotionInherit", mpName.c_str()))
		{
			if (ImGui::Selectable("Off", _motionParent == "" || _motionParent == "-1"))
				_motionParent = "-1";
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
		ToolTip("Select a layer to copy the motion from", &_parent->_appConfig->_hoverTimer);
		ImGui::SetCursorPos(oldCursorPos);
		ImGui::PopItemWidth();

		if (_motionParent == "" || _motionParent == "-1")
		{
			subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
			if (ImGui::CollapsingHeader("Bouncing", ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ImGui::Indent(indentSize);
				if (_bounceType != BounceNone)
				{
					AddResetButton("bobheight", _bounceHeight, 80.f, _parent->_appConfig, &style);
					ImGui::SliderFloat("Bounce height", &_bounceHeight, 0.0, 500.0, "%.0f px");
					ToolTip("The maximum height of the bouncing animation", &_parent->_appConfig->_hoverTimer);
					if (_bounceType == BounceRegular)
					{
						AddResetButton("bobtime", _bounceFrequency, 0.333f, _parent->_appConfig, &style);
						ImGui::SliderFloat("Bounce time", &_bounceFrequency, 0.0, 2.0, "%.2f s");
						ToolTip("The time between each bounce", &_parent->_appConfig->_hoverTimer);
					}
				}
				ImGui::Unindent(indentSize);
			}
			ToolTip("Bounce the sprite whilst talking", &_parent->_appConfig->_hoverTimer);
			oldCursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(subHeaderBtnPos);
			std::vector<const char*> bobOptions = { "None", "Loudness", "Regular" };
			ImGui::PushItemWidth(headerBtnSize.x * 7);
			if (ImGui::BeginCombo("##BounceType", bobOptions[_bounceType]))
			{
				if (ImGui::Selectable("None", _bounceType == BounceNone))
					_bounceType = BounceNone;
				ToolTip("No bouncing", &_parent->_appConfig->_hoverTimer);
				if (ImGui::Selectable("Loudness", _bounceType == BounceLoudness))
					_bounceType = BounceLoudness;
				ToolTip("Bounce height is determined by the audio level", &_parent->_appConfig->_hoverTimer);
				if (ImGui::Selectable("Regular", _bounceType == BounceRegular))
					_bounceType = BounceRegular;
				ToolTip("Fixed bounce height, on a regular time interval", &_parent->_appConfig->_hoverTimer);
				ImGui::EndCombo();
			}
			ToolTip("Select the bouncing mode", &_parent->_appConfig->_hoverTimer);
			ImGui::SetCursorPos(oldCursorPos);
			ImGui::PopItemWidth();

			subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
			if (ImGui::CollapsingHeader("Breathing", ImGuiTreeNodeFlags_AllowItemOverlap))
			{
				ImGui::Indent(indentSize);
				if (_doBreathing)
				{
					AddResetButton("breathmove", _breathMove, {0.0, 30.0}, _parent->_appConfig, &style);
					float data[2] = {_breathMove.x, _breathMove.y};
					if (ImGui::SliderFloat2("Breath Move", data, -50, 50, "%.2f"))
						_breathMove = { data[0], data[1] };
					ToolTip("The max distance the sprite will move", &_parent->_appConfig->_hoverTimer);

					AddResetButton("breathscale", _breathScale, { 0.1, 0.1 }, _parent->_appConfig, &style);
					float data2[2] = {_breathScale.x, _breathScale.y};
					if (ImGui::SliderFloat2("Breath Scale", data2, -1, 1, "%.2f"))
					{
						if (!_breathScaleConstrain)
						{
							_breathScale.x = data2[0];
							_breathScale.y = data2[1];
						}
						else if (data2[0] != _breathScale.x)
						{
							_breathScale = { data2[0] , data2[0] };
						}
						else if (data2[1] != _breathScale.y)
						{
							_breathScale = { data2[1] , data2[1] };
						}
					}
					ToolTip("The amout added to the sprite's scale", &_parent->_appConfig->_hoverTimer);

					ImGui::PushID("BreathScaleConstrain");
					ImGui::Checkbox("Constrain", &_breathScaleConstrain);
					ToolTip("Keep the X / Y scale the same", &_parent->_appConfig->_hoverTimer);
					ImGui::PopID();

					ImGui::Checkbox("Circular Motion", &_breathCircular);
					ToolTip("Move the sprite in a circle instead of a line", &_parent->_appConfig->_hoverTimer);

					ImGui::Checkbox("Breathe Whilst Talking", &_breatheWhileTalking);
					ToolTip("Breathing animation continues whilst talking", &_parent->_appConfig->_hoverTimer);

					AddResetButton("breathfreq", _breathFrequency, 4.f, _parent->_appConfig, &style);
					ImGui::SliderFloat("Breath Time", &_breathFrequency, 0.0, 10.f, "%.2f s");
				}
				ImGui::Unindent(indentSize);
			}
			oldCursorPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(subHeaderBtnPos);
			ImGui::Checkbox("##Breathing", &_doBreathing);
			ImGui::SetCursorPos(oldCursorPos);
		}

		if (ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			ImGui::Indent(indentSize);
			AddResetButton("pos", _pos, sf::Vector2f(0.0, 0.0), _parent->_appConfig, &style);
			float pos[2] = { _pos.x, _pos.y };
			if (ImGui::SliderFloat2("Position", pos, -1000.0, 1000.f, "%.1f px"))
			{
				_pos.x = pos[0];
				_pos.y = pos[1];
			}

			AddResetButton("rot", _rot, 0.f, _parent->_appConfig, &style);
			ImGui::SliderFloat("Rotation", &_rot, -180.f, 180.f, "%.1f");

			AddResetButton("scale", _scale, sf::Vector2f(1.0, 1.0), _parent->_appConfig, &style);
			float scale[2] = { _scale.x, _scale.y };
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
			ToolTip("Keeps the X and Y scale values the same", &_parent->_appConfig->_hoverTimer);

			AddResetButton("pivot", _pivot, sf::Vector2f(0.5, 0.5), _parent->_appConfig, &style);
			float pivot[2] = { _pivot.x, _pivot.y };
			if (ImGui::SliderFloat2("Pivot Point", pivot, 0.0, 1.f))
			{
				_pivot.x = Clamp(pivot[0], 0.0, 1.0);
				_pivot.y = Clamp(pivot[1], 0.0, 1.0);
			}
			ToolTip("Sets the pivot point (range 0 - 1. 0 = top/left, 1 =  bottom/right)", &_parent->_appConfig->_hoverTimer);

			ImGui::Unindent(indentSize);
		}
		ImGui::Separator();

		ImGui::Unindent(indentSize);
	}

	auto oldCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(headerButtonsPos);

	if (ImGui::Checkbox("##visible", &_visible))
	{
		bool safe = true;
		// if any active state changes this layer's visibility, it's not safe to update the default
		for (auto& state : _parent->_states)
		{
			if (state._active == false)
				continue;

			if (state._layerStates.count(_id) == 0u)
				continue;

			if (state._layerStates[_id] != StatesInfo::NoChange)
			{
				safe = false;
				break;
			}
		}
		if(safe)
			_parent->_defaultLayerStates[_id] = _visible;
	}
	ToolTip("Show or hide the layer", &_parent->_appConfig->_hoverTimer);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });

	ImGui::SameLine();
	ImGui::PushID("upbtn");
	if (ImGui::ImageButton(*_upIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerUp(this);
	ToolTip("Move the layer up", &_parent->_appConfig->_hoverTimer);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushID("dnbtn");
	if (ImGui::ImageButton(*_dnIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
		_parent->MoveLayerDown(this);
	ToolTip("Move the layer down", &_parent->_appConfig->_hoverTimer);
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
	ToolTip("Rename the layer", &_parent->_appConfig->_hoverTimer);
	ImGui::SameLine();
	ImGui::PushID("duplicateBtn");
	if (ImGui::ImageButton(*_dupeIcon, headerBtnSize, 1, sf::Color::Transparent, btnColor))
		_parent->AddLayer(this);
	ToolTip("Duplicate the layer", &_parent->_appConfig->_hoverTimer);
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
	ImGui::PushStyleColor(ImGuiCol_Text, { 255/255,200/255,170/255, 1 });
	ImGui::PushID("deleteBtn");
	if (ImGui::ImageButton(*_delIcon, headerBtnSize, 1, sf::Color::Transparent, sf::Color(255,200,170)))
		_parent->RemoveLayer(this);
	ToolTip("Delete the layer", &_parent->_appConfig->_hoverTimer);
	ImGui::PopID();
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(1);

	if (ImGui::BeginPopupModal("Rename Layer", &_renamePopupOpen, ImGuiWindowFlags_NoResize))
	{
		char inputStr[32] = " ";
		_renamingString.copy(inputStr, 32);
		if (ImGui::InputText("##renamebox", inputStr, 32, ImGuiInputTextFlags_AutoSelectAll))
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
			_animLoop = anim._loop;
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

		AddResetButton("gridreset", _animGrid, {anim.GridSize().x, anim.GridSize().y}, _parent->_appConfig);
		if (ImGui::InputInt2("Sheet Columns/Rows", _animGrid.data()))
		{
			if (_animGrid[0] != anim.GridSize().x || _animGrid[1] != anim.GridSize().y)
			{
				_animFCount = _animGrid[0] * _animGrid[1];
				_animFrameSize = { -1,-1 };
			}
		}
		ToolTip("The number of rows & columns in the spritesheet", &_parent->_appConfig->_hoverTimer);

		AddResetButton("fcountreset", _animFCount, anim.FrameCount(), _parent->_appConfig);
		ImGui::InputInt("Frame Count", &_animFCount, 0, 0);
		ToolTip("The number of frames in the animation", &_parent->_appConfig->_hoverTimer);

		AddResetButton("fpsreset", _animFPS, anim.FPS(), _parent->_appConfig, nullptr, !anim.IsSynced());
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

		AddResetButton("framereset", _animFrameSize, { -1, -1 }, _parent->_appConfig);
		ImGui::InputFloat2("Frame Size (auto = [-1,-1])", _animFrameSize.data());
		ToolTip("The size of each animation frame (set to -1,-1 to calculate automatically)", &_parent->_appConfig->_hoverTimer);

		bool sync = _animsSynced;
		if (ImGui::Checkbox("Sync Playback", &sync))
		{
			_animsSynced = sync;
		}
		ToolTip("Syncronises playback of all animated sprites in this layer", &_parent->_appConfig->_hoverTimer);

		bool loop = _animLoop;
		if (ImGui::Checkbox("Loop", &loop))
		{
			_animLoop = loop;
		}
		ToolTip("Whether to loop the animation continously", &_parent->_appConfig->_hoverTimer);

		ImGui::PushStyleColor(ImGuiCol_Button, { 0.1,0.5,0.1,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.2,0.8,0.2,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.4,0.8,0.4,1.0 });
		ImGui::PushStyleColor(ImGuiCol_Text, { 1,1,1,1 });
		if (ImGui::Button("Save"))
		{
			anim.SetAttributes(_animFCount, _animGrid[0], _animGrid[1], _animFPS, { _animFrameSize[0], _animFrameSize[1] });
			anim._loop = _animLoop;
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
	_idleSprite->ClearSync();
	if (sync)
	{
		_idleSprite->AddSync(_talkSprite.get());
		_idleSprite->AddSync(_blinkSprite.get());
		_idleSprite->AddSync(_talkBlinkSprite.get());
		_idleSprite->AddSync(_screamSprite.get());
		_idleSprite->Restart();
	}
}