
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2.h"
#include <sstream>

#include "defines.h"
#include "Shaders.h"

#include "EffectManager.h"

#include <iostream>

#include "misc/single_file/imgui_single_file.h"

#include "Gamepad.h"

#ifdef _WIN32
#include <windows.h>

// For UUID
#include <Rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

#include "websocket.h"

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

const std::vector<std::string> g_canTriggerNames
{
	"Always",
	"While Talking",
	"While Idle"
};

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	_lastTalkLevel = talkLevel;
	_lastTalkMax = talkMax;

	if (_loadingFinished == false)
	{
		return;
	}
	else if (_loadingThread != nullptr)
	{
		if (_loadingThread->joinable())
			_loadingThread->join();

		delete _loadingThread;
		_loadingThread = nullptr;
	}

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

	float talkFactor = 0;
	if (_lastTalkMax > 0)
	{
		talkFactor = _lastTalkLevel / _lastTalkMax;
		talkFactor = pow(talkFactor, 0.5);
	}

	// activate timed states
	for (size_t stateIdx = 0; stateIdx < _states.size(); stateIdx++)
	{
		auto& state = _states[stateIdx];
		if (!state._active && state._schedule && state._timer.getElapsedTime().asSeconds() > state._currentIntervalTime)
		{
			bool canTrigger = state._enabled;
			if (canTrigger && state._canTrigger != StatesInfo::CanTrigger::TRIGGER_ALWAYS)
			{
				if (state._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_TALKING)
					canTrigger &= talkFactor >= state._threshold;
				if (state._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_IDLE)
					canTrigger &= talkFactor < state._threshold;
			}

			if (canTrigger)
			{
				SaveDefaultStates();

				state._active = true;
				state._currentIntervalTime = state._intervalTime + GetRandom11() * state._intervalVariation;
				state._timer.restart();
				_statesOrder.push_back(&_states[stateIdx]);
			}
		}
	}

	// progressively display the states in order of activation
	auto statesOrderCopy = _statesOrder;
	int stateIdx = 0;
	for (auto state : statesOrderCopy)
	{
		if (state->_active &&																											//     Is active
			(state->_useTimeout || state->_schedule) &&															// AND On a schedule/using the timeout
			state->_timer.getElapsedTime().asSeconds() >= state->_timeout &&				// AND Has timed out
			// AND Not currently being held on
			(state->_activeType == state->Toggle || (state->_activeType == state->Held && state->_keyIsHeld == false)))
		{
			state->_active = false;
			RemoveStateFromOrder(state);
			state->_timer.restart();
		}

		if (state->_active)
		{
			_statesDirty = true;
			std::scoped_lock lockState(_stateLocks[stateIdx]);

			for (auto& st : state->_layerStates)
			{
				LayerInfo* layer = GetLayer(st.first);
				if (layer && st.second != StatesInfo::NoChange)
				{
					layer->_visible = st.second;
				}
			}
		}

		++stateIdx;
	}

	_effectMan->UpdateEffects(_layers);

	std::vector<LayerInfo*> calculateOrder;
	for (int l = _layers.size() - 1; l >= 0; l--)
		calculateOrder.push_back(&_layers[l]);

	std::sort(calculateOrder.begin(), calculateOrder.end(), [](const LayerInfo* lhs, const LayerInfo* rhs)
		{
			return (lhs->_lastCalculatedDepth < rhs->_lastCalculatedDepth);
		});

	for (auto layer : calculateOrder)
	{
		// Don't calculate if invisible
		bool calculate = layer->EvaluateLayerVisibility();

		// if invisible, re-enable calculation if any other layer needs it as a parent
		if (!calculate)
		{
			for (int l = _layers.size() - 1; l >= 0; l--)
			{
				LayerInfo& checkLayer = _layers[l];
				//check if any layer relies on it as a parent
				if (checkLayer._motionParent != layer->_id)
				{
					calculate = true;
					break;
				}
			}
		}

		if (calculate)
			layer->CalculateDraw(windowHeight, windowWidth, talkLevel, talkMax);
	}


	for (int l = _layers.size() - 1; l >= 0; l--)
	{
		LayerInfo& layer = _layers[l];

		bool visible = layer.EvaluateLayerVisibility();

		if (visible)
		{
			sf::RenderStates state = sf::RenderStates::Default;

			state.transform.translate(_globalPos);
			state.transform.translate(0.5 * target->getSize().x, 0.5 * target->getSize().y);
			state.transform.scale(_globalScale * _appConfig->mainWindowScaling);
			state.transform.rotate(_globalRot);
			state.transform.translate(-0.5 * target->getSize().x, -0.5 * target->getSize().y);

			auto usingBlendmode = layer._blendMode;

			bool blendModeNeedsPremult = 
				usingBlendmode == g_blendmodes["Multiply"]
				|| usingBlendmode == g_blendmodes["Normal"]
				|| usingBlendmode == g_blendmodes["Lighten"]
				|| usingBlendmode == g_blendmodes["Darken"]
				|| usingBlendmode == g_blendmodes["Clip to Backdrop"];


			LayerInfo* clipLayer = GetLayer(layer._clipID);

			bool sharpEdge = _appConfig->_sharpEdge && layer._scaleFiltering == 1;


			bool useBlendShader = false;

			//if (sharpEdge || blendModeNeedsPremult)
			{
				if (_blendingShaderLoaded == false)
				{
					_blendingShader.loadFromMemory(SFML_DefaultVert, SFML_PremultFrag);
					_blendingShaderLoaded = true;
				}

				if (blendModeNeedsPremult)
				{
					_blendingShader.setUniform("premult", true);
				}
				else
				{
					_blendingShader.setUniform("premult", false);
				}

				if (usingBlendmode == g_blendmodes["Darken"])
					_blendingShader.setUniform("invert", true);
				else
					_blendingShader.setUniform("invert", false);

				_blendingShader.setUniform("sharpEdge", sharpEdge);

				float alphaClip = layer._alphaClip;
				if (alphaClip == 0.0)
					alphaClip = _appConfig->_alphaClip;

				_blendingShader.setUniform("alphaClip", alphaClip );

				useBlendShader = true;
			}

			if (clipLayer == nullptr)
			{
				state.blendMode = usingBlendmode;

				if (useBlendShader)
					state.shader = _blendingShader.get();

				layer._idleSprite->Draw(target, state);
				layer._talkSprite->Draw(target, state);
				layer._blinkSprite->Draw(target, state);
				layer._talkBlinkSprite->Draw(target, state);
				layer._screamSprite->Draw(target, state);
			}
			else
			{
				ClipRenderTextures& clipRTs = _clipRenderTextures[layer._id];

				if (clipRTs._clipInitialized == false || target->getSize() != clipRTs._clipRT.getSize())
				{
					clipRTs._clipRT.create(target->getSize().x, target->getSize().y);
					clipRTs._clipInitialized = true;
				}
				clipRTs._clipRT.clear({ 0,0,0,0 });

				if (clipRTs._soloLayerInitialized == false || target->getSize() != clipRTs._soloLayerRT.getSize())
				{
					clipRTs._soloLayerRT.create(target->getSize().x, target->getSize().y);
					clipRTs._soloLayerInitialized = true;
				}
				clipRTs._soloLayerRT.clear({ 0,0,0,0 });


				sf::RenderStates clipState = sf::RenderStates::Default;

				// Draw clip layer onto empty canvas
				clipState.transform.translate(_globalPos);
				clipState.transform.translate(0.5 * target->getSize().x, 0.5 * target->getSize().y);
				clipState.transform.scale(_globalScale * _appConfig->mainWindowScaling);
				clipState.transform.rotate(_globalRot);
				clipState.transform.translate(-0.5 * target->getSize().x, -0.5 * target->getSize().y);

				if (useBlendShader)
				{
					float alphaClip = clipLayer->_alphaClip;
					if (alphaClip == 0.0)
						alphaClip = _appConfig->_alphaClip;
					_blendingShader.setUniform("sharpEdge", sharpEdge);
					_blendingShader.setUniform("alphaClip", alphaClip);
					clipState.shader = _blendingShader.get();
				}

				clipLayer->_idleSprite->Draw(&clipRTs._clipRT, clipState);
				clipLayer->_talkSprite->Draw(&clipRTs._clipRT, clipState);
				clipLayer->_blinkSprite->Draw(&clipRTs._clipRT, clipState);
				clipLayer->_talkBlinkSprite->Draw(&clipRTs._clipRT, clipState);
				clipLayer->_screamSprite->Draw(&clipRTs._clipRT, clipState);

				layer._clipRect.setSize(sf::Vector2f(target->getSize().x, target->getSize().y));
				layer._clipRect.setPosition({ 0,0 });

				// Do not premultiply alpha here
				state.blendMode.colorSrcFactor = sf::BlendMode::One;

				if (useBlendShader)
				{
					float alphaClip = layer._alphaClip;
					if (alphaClip == 0.0)
						alphaClip = _appConfig->_alphaClip;
					_blendingShader.setUniform("sharpEdge", sharpEdge);
					_blendingShader.setUniform("alphaClip", alphaClip);
					state.shader = _blendingShader.get();
				}

				// Draw layer to be clipped onto an empty canvas
				layer._idleSprite->Draw(&clipRTs._soloLayerRT, state);
				layer._talkSprite->Draw(&clipRTs._soloLayerRT, state);
				layer._blinkSprite->Draw(&clipRTs._soloLayerRT, state);
				layer._talkBlinkSprite->Draw(&clipRTs._soloLayerRT, state);
				layer._screamSprite->Draw(&clipRTs._soloLayerRT, state);

				clipRTs._soloLayerRT.display();
				layer._clipRect.setTexture(&clipRTs._soloLayerRT.getTexture(), true);

				sf::RenderStates clipState2 = sf::RenderStates::Default;
				// Draw the layer RT onto the clip RT using the fancy new blend mode
				clipState2.blendMode = sf::BlendMode(sf::BlendMode::One, sf::BlendMode::Zero, sf::BlendMode::Add,
					sf::BlendMode::One, sf::BlendMode::One, sf::BlendMode::Min);

				clipRTs._clipRT.draw(layer._clipRect, clipState2);
				clipRTs._clipRT.display();

				// Draw the clip rect onto the actual window
				layer._clipRect.setTexture(&clipRTs._clipRT.getTexture(), true);

				sf::RenderStates RTState = sf::RenderStates::Default;
				RTState.blendMode = usingBlendmode;

				if (useBlendShader)
				{
					RTState.shader = _blendingShader.get();
				}

				target->draw(layer._clipRect, RTState);
			}
		}
		else
		{
			layer._idleSprite->Tick();
			layer._talkSprite->Tick();
			layer._blinkSprite->Tick();
			layer._talkBlinkSprite->Tick();
			layer._screamSprite->Tick();
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

			if (layer._inFolder != "")
			{
				LayerInfo* folder = GetLayer(layer._inFolder);
				if (folder)
					visible &= folder->_visible;
			}

			if (visible && layer._isFolder == false)
			{
				// Draw sprite borders

				bool layerHovered = false;
				for (auto& hl : _hoveredLayers)
					if (hl == layer._id)
					{
						layerHovered = true;
						break;
					}

				const auto& theme = _uiConfig->_themes[_uiConfig->_theme];

				sf::Vector2f origin = layer._activeSprite->getOrigin();
				sf::Vector2f pos = layer._activeSprite->getPosition();
				sf::Vector2f scale = layer._activeSprite->getScale();
				sf::Vector2f size = layer._activeSprite->Size();
				float rot = layer._activeSprite->getRotation();

				sf::Vector2f boxSize = { size.x * scale.x, size.y * scale.y };
				sf::Vector2f boxOrigin = { origin.x * scale.x, origin.y * scale.y };

				sf::Color brightColor = toSFColor(theme.second + ImVec4(0.2, 0.2, 0.2, 0.2));

				auto box = sf::RectangleShape(boxSize);
				box.setOrigin(boxOrigin);
				box.setPosition(pos);
				box.setRotation(rot);
				box.setOutlineColor(toSFColor(theme.first + ImVec4(0.2, 0.2, 0.2, 0.2)));
				box.setOutlineThickness((1.0f / _globalScale.x) * _appConfig->mainWindowScaling);
				sf::Color fill = brightColor;
				if (layerHovered && _uiConfig->_hilightHovered)
					fill.a = 40;
				else
					fill.a = 0;
				box.setFillColor(fill);

				float circleRadius = 3 * _appConfig->mainWindowScaling;
				if (layerHovered)
					circleRadius = 6 * _appConfig->mainWindowScaling;
				auto circle = sf::CircleShape(circleRadius, 8);
				circle.setPosition(pos);
				circle.setOrigin({ circleRadius ,circleRadius });
				circle.setFillColor(brightColor);
				circle.setOutlineColor(toSFColor(theme.first));
				circle.setOutlineThickness(2 * _appConfig->mainWindowScaling);

				circle.setScale({ 1.0f / _globalScale.x, 1.0f / _globalScale.y });

				sf::RenderStates state = sf::RenderStates::Default;

				state.transform.translate(_globalPos);
				state.transform.translate(0.5 * target->getSize().x, 0.5 * target->getSize().y);
				state.transform.scale(_globalScale * _appConfig->mainWindowScaling);
				state.transform.rotate(_globalRot);
				state.transform.translate(-0.5 * target->getSize().x, -0.5 * target->getSize().y);

				target->draw(box, state);
				target->draw(circle, state);

				float targetSize = 10 * _appConfig->mainWindowScaling;
				float targetLineW = 2 * _appConfig->mainWindowScaling;

				// Draw Crosshair for mouse tracking
				if (layerHovered && layer._trackingType)
				{
					auto targetColor = toSFColor(theme.second * 1.1 + ImVec4(0.2, 0.2, 0.2, 0.2));
					auto circle2 = sf::CircleShape(targetSize, 16);
					auto targetPos = layer._mouseNeutralPos - (sf::Vector2f)_appConfig->_window.getPosition();
					circle2.setPosition(targetPos);
					circle2.setOrigin({ targetSize ,targetSize });
					circle2.setFillColor(sf::Color::Transparent);
					circle2.setOutlineColor(targetColor);
					circle2.setOutlineThickness(2 * _appConfig->mainWindowScaling);

					auto lineV = sf::RectangleShape({ targetSize * 2 ,targetLineW });
					lineV.setPosition(targetPos);
					lineV.setOrigin({ targetSize, targetLineW / 2 });
					lineV.setFillColor(targetColor);

					auto lineH = lineV;
					lineH.setSize({ targetLineW ,targetSize * 2 });
					lineH.setOrigin({ targetLineW / 2, targetSize });

					target->draw(lineH, sf::RenderStates::Default);
					target->draw(lineV, sf::RenderStates::Default);
					target->draw(circle2, sf::RenderStates::Default);
				}
			}
		}
	}

	_hoveredLayers.clear();
}

void LayerManager::DrawOldLayerSetUI()
{

	float uiScale = _appConfig->scalingFactor;

	if (ImGui::BeginTable("##layerInOut", 5, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Text");
		ImGui::TableSetupColumn("LayerSetName");
		ImGui::TableSetupColumn("Browse", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Save", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed, uiScale * 80);
		ImGui::TableNextColumn();

		ImGui::AlignTextToFramePadding();
		ImGui::Text("Layer Set:");

		ImGui::TableNextColumn();

		char inputStr[MAX_PATH] = " ";
		fs::path appFolder = fs::absolute(_appConfig->_appLocation);
		fs::path relativeDir = fs::proximate(_loadedXMLAbsDirectory, appFolder);
		fs::path relativePath = _layerSetName;
		if (relativeDir != ".")
			relativePath = relativeDir.append(_layerSetName);
		ANSIToUTF8(relativePath.string()).copy(inputStr, MAX_PATH);

		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputText("##layersXMLInput", inputStr, MAX_PATH, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_AutoSelectAll) || ImGui::IsItemDeactivatedAfterEdit())
		{
			fs::current_path(appFolder);

			fs::path xmlPath = fs::absolute(UTF8ToANSI(inputStr));
			if (xmlPath.extension().string() != ".xml")
				xmlPath.replace_extension(".xml");

			std::error_code ec;
			_loadedXMLExists = fs::exists(xmlPath, ec);

			_layerSetName = xmlPath.filename().replace_extension("").string();
			UpdateWindowTitle();

		}
		ToolTip("Enter the name of a layer set to load.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();

		_loadXMLOpen = ImGui::Button("...", { 30 * uiScale,ImGui::GetFrameHeight() });
		ToolTip("Browse for a layer set (.xml) file.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();

		_saveXMLOpen = ImGui::Button(_loadedXMLExists ? "Overwrite" : "Save", { uiScale * 80 , ImGui::GetFrameHeight() }) && !_loadedXMLRelPath.empty();
		if (_saveXMLOpen)
		{
			_fullLoadedXMLPath = fs::absolute(_loadedXMLAbsDirectory).append(_layerSetName).replace_extension(".xml").string();
			fs::path proximateXMLPath = fs::proximate(_fullLoadedXMLPath, appFolder);
			_loadedXMLRelDirectory = proximateXMLPath.parent_path().string();
			_loadedXMLRelPath = proximateXMLPath.replace_extension("").string();
		}
		ToolTip("Save the current layer set.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();

		if (_loadedXMLExists)
		{
			ImGui::PushID("loadXMLBtn"); {
				_reloadXMLOpen = ImGui::Button("Load", { uiScale * 80 , ImGui::GetFrameHeight() });
			}ImGui::PopID();
		}
		ToolTip("Load the specified layer set.", &_appConfig->_hoverTimer);
		ImGui::EndTable();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2,0 });
	if (ImGui::BeginTable("##layerControls", 4, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextColumn();

		_newLayerOpen = ImGui::Button("Add Layer", { -1, ImGui::GetFrameHeight() });
		ToolTip("Adds a new layer to the list.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		_newFolderOpen = ImGui::Button("Add Folder", { -1, ImGui::GetFrameHeight() });
		ToolTip("Adds a new layer to the list.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		_clearLayersOpen = ImGui::Button("Remove All", { -1, ImGui::GetFrameHeight() });
		ToolTip("Removes all layers from the list.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		_editStatesOpen = ImGui::Button("States", { -1, ImGui::GetFrameHeight() });
		ToolTip("Opens the States setup menu.", &_appConfig->_hoverTimer);

		ImGui::EndTable();
	}

	ImGui::PopStyleVar();
}

void LayerManager::UpdateWindowTitle()
{
	_appConfig->_lastLayerSet = _fullLoadedXMLPath;

	if (_appConfig->_nameWindowWithSet)
	{
		_appConfig->_nameLock.lock();
		{
			_appConfig->windowName = "RahiTuber - " + _layerSetName;
			_appConfig->_pendingNameChange = true;
			_appConfig->_pendingSpoutNameChange = true;

		}
		_appConfig->_nameLock.unlock();
	}
}

void LayerManager::CopyFileAndUpdatePath(std::string& filePath, std::filesystem::path targetFolder, std::filesystem::copy_options copyOpts)
{
	std::error_code ec;
	fs::path fsFilePath(filePath);
	if (filePath != "")
	{
		fs::copy(fsFilePath, targetFolder, copyOpts, ec);
		if (!ec)
		{
			filePath = targetFolder.append(fsFilePath.filename().string()).string();
		}
	}
}

void LayerManager::DoMenuBarLogic()
{

	if (!_loadingFinished)
		return;

	//////////////////////////////// NEW ///////////////////////////////////

	fs::path appFolder = fs::absolute(_appConfig->_appLocation);

	ImGui::PushID("newXMLBtn"); {
		static imgui_ext::file_browser_modal newFolderSelect("New Layer Set");
		newFolderSelect._acceptedExt = { ".xml" };
		//_newXMLOpen = ImGui::Button("New", { -1, ImGui::GetFrameHeight() });

		bool showOpenWindow = _newXMLOpen;

		switch (ConfirmModal("Start new Layer Set", &_newXMLOpen, _newXMLOpen,
			"This will start a new layer set, discarding any unsaved changes.\nContinue?"))
		{
		case -1:
		case 0:
			showOpenWindow = false;
			break;
		case 1:
			showOpenWindow = true;
			break;
		}


		if (showOpenWindow)
			newFolderSelect.SetStartingDir(appFolder);
		if (newFolderSelect.render(showOpenWindow, _savingXMLPath, true))
		{
			ResetStates();
			_statesOrder.clear();
			_layers.clear();
			_states.clear();

			fs::path xmlPath = fs::absolute(_savingXMLPath);
			if (xmlPath.extension().string() != ".xml")
				xmlPath.replace_extension(".xml");

			SaveLayers(xmlPath.string());

			std::error_code ec;
			_loadedXMLExists = fs::exists(xmlPath, ec);
			_fullLoadedXMLPath = xmlPath.string();
			_loadedXMLAbsDirectory = xmlPath.parent_path().string();
			_layerSetName = xmlPath.filename().replace_extension("").string();
			fs::path proximateXMLPath = fs::proximate(xmlPath, appFolder);
			_loadedXMLRelPath = proximateXMLPath.replace_extension("").string();
			_loadedXMLRelDirectory = fs::proximate(_loadedXMLAbsDirectory, _appConfig->_appLocation).string();

			UpdateWindowTitle();
		}
	}ImGui::PopID();

	//////////////////////////////// OPEN ///////////////////////////////////

	ImGui::PushID("openXMLBtn"); {
		static imgui_ext::file_browser_modal fileBrowserXML("Load Layer Set");
		fileBrowserXML._acceptedExt = { ".xml" };
		//_loadXMLOpen = ImGui::Button("Open", { -1, ImGui::GetFrameHeight() });
		if (_loadXMLOpen)
			fileBrowserXML.SetStartingDir(_lastSavedLocation);
		if (fileBrowserXML.render(_loadXMLOpen, _fullLoadedXMLPath))
		{
			fs::path xmlPath = fs::absolute(_fullLoadedXMLPath);
			std::error_code ec;
			_loadedXMLExists = fs::exists(xmlPath, ec);
			_fullLoadedXMLPath = xmlPath.string();
			_loadedXMLAbsDirectory = xmlPath.parent_path().string();
			fs::path proximateXMLPath = fs::proximate(xmlPath, appFolder);
			_loadedXMLRelDirectory = proximateXMLPath.parent_path().string();
			_loadedXMLRelPath = proximateXMLPath.replace_extension("").string();
			LoadLayers(_fullLoadedXMLPath);

			UpdateWindowTitle();
		}
	}ImGui::PopID();

	//////////////////////////////// SAVE ///////////////////////////////////

	static imgui_ext::file_browser_modal folderSelect("Save Layer Set");
	folderSelect._acceptedExt = { ".xml" };

	if (_makePortableOpen)
	{
		ImGui::OpenPopup("Make Portable");
		_makePortableOpen = false;
	}

	if (ImGui::BeginPopupModal("Make Portable"))
	{
		_saveLayersPortable = false;
		_copyImagesPortable = false;

		ImGui::SetWindowSize({ 400 * _appConfig->scalingFactor, -1.f });
		ImGui::TextWrapped("This function creates a version of your layer set where all file paths are relative to the location of RahiTuber.\n\n\
This works best when all your sprite images are located in a subfolder of RahiTuber's directory.\n\nCopy them there now?");

		if (ImGui::Button("Copy files and create portable XML", { -1, ImGui::GetFrameHeight() }))
		{
			_saveAsXMLOpen = true;
			_saveLayersPortable = true;
			_copyImagesPortable = true;
			ImGui::CloseCurrentPopup();
		}
		ToolTip("You will be prompted to save a new XML\nfile in RahiTuber's directory.\nThen a folder will be created with the\nsame name, and your sprites copied there.", &_appConfig->_hoverTimer);

		if (ImGui::Button("Create portable XML only", { -1, ImGui::GetFrameHeight() }))
		{
			_saveAsXMLOpen = true;
			_saveLayersPortable = true;
			ImGui::CloseCurrentPopup();
		}
		ToolTip("You will be prompted to save a new XML\nfile in RahiTuber's directory. \nThe original sprite image locations will still be used,\nso this may yield awkward results if they are not already in a \nsubfolder of RahiTuber's directory.", &_appConfig->_hoverTimer);


		if (LesserButton("Cancel", { -1, ImGui::GetFrameHeight() }))
		{
			_saveLayersPortable = false;
			ImGui::CloseCurrentPopup();
		}
		ToolTip("Cancels the Make Portable process and leaves your files unchanged.", &_appConfig->_hoverTimer);


		ImGui::EndPopup();
	}

	if (_saveXMLOpen)
	{
		if (SaveLayers(_savingXMLPath = _fullLoadedXMLPath))
			_loadedXMLExists = true;

		_saveXMLOpen = false;
	}

	if (_saveAsXMLOpen)
		folderSelect.SetStartingDir(_savingXMLPath = _fullLoadedXMLPath);
	if (_saveLayersPortable)
		folderSelect.SetStartingDir(fs::path(_appConfig->_appLocation).append(_layerSetName + ".xml").string());
	if (folderSelect.render(_saveAsXMLOpen, _savingXMLPath, true))
	{
		fs::path xmlPath = fs::absolute(_savingXMLPath);
		if (xmlPath.extension().string() != ".xml")
			xmlPath.replace_extension(".xml");

		auto oldLSName = _layerSetName;
		_layerSetName = xmlPath.filename().replace_extension("").string();

		if (SaveLayers(xmlPath.string(), _saveLayersPortable, _copyImagesPortable))
		{
			std::error_code ec;
			_loadedXMLExists = fs::exists(xmlPath, ec);
			_fullLoadedXMLPath = xmlPath.string();
			_loadedXMLAbsDirectory = xmlPath.parent_path().string();
			fs::path proximateXMLPath = fs::proximate(xmlPath, appFolder);
			_loadedXMLRelDirectory = proximateXMLPath.parent_path().string();
			_loadedXMLRelPath = proximateXMLPath.replace_extension("").string();

			UpdateWindowTitle();
		}
		else
		{
			_layerSetName = oldLSName;
		}

		_saveLayersPortable = false;
		_copyImagesPortable = false;
	}

	//////////////////////////////// RELOAD ///////////////////////////////////

	if (ConfirmModal("Reload Layer Set", &_reloadXMLOpen, _reloadXMLOpen,
		"This will reload the current layer set, discarding any unsaved changes. Continue?") == 1)
	{
		_reloadXMLOpen = false;
		if (_loadedXMLExists)
		{
			LoadLayers(_fullLoadedXMLPath);
		}
	}

	/////////////////////////////// LAYERS /////////////////////////////////////
	if (_newLayerOpen)
	{
		AddLayer();
		_newLayerOpen = false;
	}

	if (_newFolderOpen)
	{
		AddLayer(nullptr, true);
		_newFolderOpen = false;
	}

	if(ConfirmModal("Remove All Layers", &_clearLayersOpen, _clearLayersOpen) == 1)
	{
		_layers.clear();
		_clearLayersOpen = false;
	}

	if (_editStatesOpen)
	{
		_statesMenuOpen = true;
		_editStatesOpen = false;
	}

	if (ConfirmModal("Remove All States", &_clearStatesOpen, _clearStatesOpen) == 1)
	{
		ResetStates();
		_states.clear();
	}
}

void LayerManager::DrawButtonsLayerSetUI()
{
	TextCentered(ANSIToUTF8(_layerSetName).c_str());
	float uiScale = _appConfig->scalingFactor;
	float btnSize = uiScale * 38;
	float separatorY = btnSize / 2 - uiScale * 5;
	sf::Color btnColor = toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
	if (ImGui::BeginTable("##toolButtons", 11, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableNextColumn();
		_newXMLOpen = ImGui::ImageButton("##newFile", *_newFileIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("New", "Start a new layer set.\n(discards unsaved changes!)", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_loadXMLOpen = ImGui::ImageButton("##Open", *_openFileIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Open", "Browse for a layer set (.xml) file.", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_saveXMLOpen = ImGui::ImageButton("##save", *_saveIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Save", "Save the current layer set.", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_saveAsXMLOpen = ImGui::ImageButton("##saveAs", *_saveAsIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Save As", "Save the current layer set with a new name.", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_makePortableOpen = ImGui::ImageButton("##makePortable", *_makePortableIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Make Portable", "Save a version of this layer set with\nfile paths relative to RahiTuber", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_reloadXMLOpen = ImGui::ImageButton("##reload", *_reloadIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Reload", "Reload the specified layer set.\n(discards unsaved changes!)", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + separatorY);
		ImGui::Text("|");

		ImGui::TableNextColumn();
		_newLayerOpen = ImGui::ImageButton("##newLayer", *_newLayerIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Add Layer", "Add a new Layer to the top of the list.", &_appConfig->_hoverTimer);
		ImGui::TableNextColumn();
		_newFolderOpen = ImGui::ImageButton("##newFolder", *_newFolderIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("Add Folder", "Add a new Folder to the top of the list.", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + separatorY);
		ImGui::Text("|");

		ImGui::TableNextColumn();
		_editStatesOpen = ImGui::ImageButton("##states", *_statesIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor);
		ToolTip("States", "Configure the States for this Layer Set.", &_appConfig->_hoverTimer);

		ImGui::EndTable();
	}
}

void LayerManager::DrawGUI(ImGuiStyle& style, float maxHeight)
{
	float topBarBegin = ImGui::GetCursorPosY();

	ImGui::PushID("layermanager"); {

		float frameW = ImGui::GetWindowWidth();

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2,0 });
		if (_uiConfig->_layersUIType == _uiConfig->LayersUI_Old)
			DrawOldLayerSetUI();
		else if (_uiConfig->_layersUIType == _uiConfig->LayersUI_Menus)
			DrawMenusLayerSetUI();
		else
			DrawButtonsLayerSetUI();

		DoMenuBarLogic();

		ImGui::PopStyleVar();

		if (_errorMessage.empty() == false)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.0, 0.0, 1.0));
			ImGui::TextWrapped(ANSIToUTF8(_errorMessage).c_str());
			ImGui::PopStyleColor();
		}

		ImGui::PushID("statesPopup"); {
			DrawStatesGUI();
		}ImGui::PopID();

		ImGui::Separator();


		if (ImGui::CollapsingHeader("Canvas Settings"))
		{
			ToolTip("Global position/scale/rotation settings\n(to change the size/position of the whole scene).", &_appConfig->_hoverTimer);

			DrawCanvasPresetGUI();

			ImGui::PushStyleColor(ImGuiCol_Separator, style.Colors[ImGuiCol_TextDisabled]);
			ImGui::Separator();
			ImGui::PopStyleColor();

			AddResetButton("pos", _globalPos, sf::Vector2f(0.0, 0.0), _appConfig, &style);
			float pos[2] = { _globalPos.x, _globalPos.y };
			if (Float2SliderDrag("Position", pos, -1000.0, 1000.f, "%.0f", 0, _uiConfig->_numberEditType))
			{
				_globalPos.x = pos[0];
				_globalPos.y = pos[1];
			}
			ToolTip("Change the position for all layers", &_appConfig->_hoverTimer, true);

			AddResetButton("rot", _globalRot, 0.f, _appConfig, &style);
			FloatSliderDrag("Rotation", &_globalRot, -180.f, 180.f, "%.1f deg", 0, _uiConfig->_numberEditType);
			ToolTip("Change the rotation for all layers", &_appConfig->_hoverTimer, true);

			AddResetButton("scale", _globalScale, sf::Vector2f(1.0, 1.0), _appConfig, &style);
			float scale[2] = { _globalScale.x, _globalScale.y };
			if (Float2SliderDrag("Scale", scale, 0.0, 5.f, "%.2f", 0, _uiConfig->_numberEditType))
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
			ToolTip("Change the scale for all layers", &_appConfig->_hoverTimer, true);

			ImGui::Checkbox("Constrain", &_globalKeepAspect);
			ToolTip("Keeps the Scale x/y values the same.", &_appConfig->_hoverTimer);
		}
		else
			ToolTip("Global position/scale/rotation settings\n(to change the size/position of the whole scene).", &_appConfig->_hoverTimer);

		ImGui::Separator();

		if (_loadingFinished == false)
		{
			DrawLoadingMessage();
		}
		else
		{
			float topBarHeight = ImGui::GetCursorPosY() - topBarBegin;

			ImGui::BeginChild(ImGuiID(10001), ImVec2(-1, (maxHeight - topBarHeight)), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			bool outOfFocus = !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow | ImGuiFocusedFlags_NoPopupHierarchy);

			ImVec2 resetPos = ImGui::GetCursorPos();

			int hoveredLayer = GetLayerUnderCursor(_layerDragPos.x, _layerDragPos.y);
			bool skipFolder = false;
			if (hoveredLayer == -2)
			{
				hoveredLayer = 0;
				skipFolder = true;
			}
			if (hoveredLayer == -3)
			{
				hoveredLayer = _layers.size() - 1;
				skipFolder = true;
			}

			if (_dragActive)
			{
				ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));

				if (hoveredLayer != _draggedLayer && hoveredLayer != -1 && _draggedLayer >= 0)
				{
					LayerInfo& layerBelowInsert = _layers[hoveredLayer];

					bool folderIntoFolder = _layers[_draggedLayer]._isFolder && (layerBelowInsert._isFolder || layerBelowInsert._inFolder != "");

					if ((layerBelowInsert._isFolder == false || skipFolder) && !folderIntoFolder)
					{
						sf::Vector2f linePos = layerBelowInsert._lastHeaderPos;

						if (hoveredLayer > _draggedLayer)
							linePos.y += layerBelowInsert._lastHeaderSize.y;
						else
							linePos.y -= 1;

						sf::Vector2f linePos2 = linePos;
						linePos2.x += layerBelowInsert._lastHeaderSize.x;

						ImVec4 lineCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
						ImGui::DrawLine(linePos, linePos2, toSFColor(lineCol), 2);
					}
				}

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
			}

			for (int l = 0; l < _layers.size(); l++)
			{
				auto& layer = _layers[l];
				layer._parent = this;

				if (layer._inFolder == "")
				{
					layer._lastHeaderPos = { -1,-1 };
					layer._lastHeaderScreenPos = { -1,-1 };
					layer._lastHeaderSize = { 0,0 };

					if (!layer.DrawGUI(style, l))
						break;
				}

			}

			if (_dragActive)
			{
				ImGui::PopStyleColor(3);

				if (hoveredLayer != _draggedLayer && hoveredLayer != -1 && _draggedLayer >= 0)
				{
					LayerInfo& layerBelowInsert = _layers[hoveredLayer];

					if (!skipFolder && layerBelowInsert._isFolder == true && !_layers[_draggedLayer]._isFolder)
					{
						ImGui::SetCursorPos(resetPos);
						sf::Vector2f linePos = layerBelowInsert._lastHeaderPos;
						linePos.y -= 1;

						ImVec4 lineCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
						lineCol.w = 0.5;
						sf::FloatRect hilightRect(linePos, layerBelowInsert._lastHeaderSize);
						ImGui::DrawRectFilled(hilightRect, toSFColor(lineCol), 2);
					}
				}
			}

			ImGui::EndChild();
		}

	}ImGui::PopID();

	_maxCursorDragY = ImGui::GetCursorScreenPos().y;

	if (_dragActive && _draggedLayer != -1 && _layers.size() > _draggedLayer && ImGui::BeginTooltip())
	{
		ImGui::Text(_layers[_draggedLayer]._name.c_str());
		ImGui::EndTooltip();
	}

}

void LayerManager::DrawLoadingMessage()
{
	if (_loadingPath != "")
	{
		ImGui::AlignTextToFramePadding();
		std::string txt = "Loading " + _loadingProgress + "...";
		TextCentered(ANSIToUTF8(txt).c_str());
	}
}

void LayerManager::DrawCanvasPresetGUI()
{
	ImGuiID presetPopupID = ImGui::GetID("Edit Canvas Preset");

	if (ImGui::BeginTable("PresetsTable", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX))
	{
		sf::Color btnColor = toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
		float btnSize = ImGui::GetFrameHeight() - 2;
		ImGui::TableNextColumn();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1, 1 });

		if (ImGui::ImageButton("##NewPreset", *_plusIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor))
		{
			_globalPresets.push_back(GlobalPreset());
			_currentGlobalPreset = _globalPresets.size() - 1;
			if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
			{
				_canvasPresetMenuOpen = true;
				ImGui::OpenPopup(presetPopupID);
			}
		}
		ToolTip("New", "Make a new preset", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		//ImGui::SameLine(btnSize*0.6);

		if (ImGui::ImageButton("##RenamePreset", *_editIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor))
		{
			if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
			{
				_canvasPresetMenuOpen = true;
				ImGui::OpenPopup(presetPopupID);
			}
		}
		ToolTip("Rename", "Rename the current preset", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		//ImGui::SameLine(btnSize * 0.6);

		if (ImGui::ImageButton("##SavePreset", *_saveIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor))
		{
			if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
			{
				GlobalPreset& thisPreset = _globalPresets[_currentGlobalPreset];
				thisPreset._scale = _globalScale;
				thisPreset._pos = _globalPos;
				thisPreset._rot = _globalRot;
			}
		}
		ToolTip("Save", "Save the current preset", &_appConfig->_hoverTimer);

		ImGui::TableNextColumn();
		//ImGui::SameLine(btnSize * 0.6);

		ImVec4 delCol = PushDeleteStyle();
		if (ImGui::ImageButton("##DeletePreset", *_delIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, toSFColor(delCol)))
		{
			if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
				_globalPresets.erase(_globalPresets.begin() + _currentGlobalPreset);
		}
		PopDeleteStyle();
		ToolTip("Delete", "Delete the current preset", &_appConfig->_hoverTimer);

		ImGui::PopStyleVar();

		ImGui::TableNextColumn();
		//ImGui::SameLine(btnSize * 0.6);

		std::string presetName = "None";
		if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
		{
			presetName = _globalPresets[_currentGlobalPreset]._name;
		}
		if (ImGui::BeginCombo("Preset", presetName.c_str()))
		{
			for (int gp = 0; gp < _globalPresets.size(); gp++)
			{
				GlobalPreset& thisPreset = _globalPresets[gp];
				if (ImGui::Selectable(thisPreset._name.c_str(), gp == _currentGlobalPreset))
				{
					_currentGlobalPreset = gp;
					_globalScale = thisPreset._scale;
					_globalPos = thisPreset._pos;
					_globalRot = thisPreset._rot;
				}
			}

			ImGui::EndCombo();
		}

		ImGui::EndTable();
	}

	ImGui::SetNextWindowSize({ ImGui::GetFrameHeight() * 10, ImGui::GetFrameHeight() * 4 }, ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("Edit Canvas Preset", &_canvasPresetMenuOpen))
	{
		if (_currentGlobalPreset != -1 && _currentGlobalPreset < _globalPresets.size())
		{
			GlobalPreset& thisPreset = _globalPresets[_currentGlobalPreset];

			char inputStr[32] = " ";
			ANSIToUTF8(thisPreset._name).copy(inputStr, 32);

			if (_canvasPresetMenuFirstOpen)
				ImGui::SetKeyboardFocusHere();

			_canvasPresetMenuFirstOpen = false;

			bool edited = false;
			if (ImGui::InputText("##renamebox", inputStr, 32, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				edited = true;
			}
			thisPreset._name = UTF8ToANSI(inputStr);
			ImGui::SameLine();

			bool saved = ImGui::Button("Save");
			if (saved || edited)
			{
				_canvasPresetMenuOpen = false;
				ImGui::CloseCurrentPopup();
				_canvasPresetMenuFirstOpen = true;
			}
		}

		ImGui::EndPopup();
	}
}

void LayerManager::DrawMenusLayerSetUI()
{
	TextCentered(ANSIToUTF8(_layerSetName).c_str());
	if (ImGui::BeginTable("##layerInOutNew", 3, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame))
	{
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::BeginCombo("##FileMenu", "File", ImGuiComboFlags_NoArrowButton))
		{
			_newXMLOpen = ImGui::Selectable("New");
			ToolTip("Start a new layer set.\n(discards unsaved changes!)", &_appConfig->_hoverTimer);

			_loadXMLOpen = ImGui::Selectable("Open");
			ToolTip("Browse for a layer set (.xml) file.", &_appConfig->_hoverTimer);

			_saveXMLOpen = ImGui::Selectable("Save");
			ToolTip("Save the current layer set.", &_appConfig->_hoverTimer);

			_saveAsXMLOpen = ImGui::Selectable("Save As");
			ToolTip("Save the current layer set with a new name.", &_appConfig->_hoverTimer);

			_makePortableOpen = ImGui::Selectable("Make Portable");
			ToolTip("Save a version of this layer set with\nfile paths relative to RahiTuber", &_appConfig->_hoverTimer);

			_reloadXMLOpen = ImGui::Selectable("Reload");
			ToolTip("Reload the specified layer set.\n(discards unsaved changes!)", &_appConfig->_hoverTimer);

			ImGui::EndCombo();
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::BeginCombo("##LayersMenu", "Layers", ImGuiComboFlags_NoArrowButton))
		{
			_newLayerOpen = ImGui::Selectable("New Layer");
			ToolTip("Add a new Layer to the top of the list.", &_appConfig->_hoverTimer);

			_newFolderOpen = ImGui::Selectable("New Folder");
			ToolTip("Add a new Folder to the top of the list.", &_appConfig->_hoverTimer);

			_clearLayersOpen = ImGui::Selectable("Remove All Layers");
			ToolTip("Removes all layers and folders from the list.", &_appConfig->_hoverTimer);

			ImGui::EndCombo();
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::BeginCombo("##StatesMenu", "States", ImGuiComboFlags_NoArrowButton))
		{
			_editStatesOpen = ImGui::Selectable("Edit States");
			ToolTip("Configure the States for this Layer Set.", &_appConfig->_hoverTimer);

			_clearStatesOpen = ImGui::Selectable("Remove All States");
			ToolTip("Removes all States.", &_appConfig->_hoverTimer);

			ImGui::EndCombo();
		}

		ImGui::EndTable();
	}
}

LayerManager::LayerInfo* LayerManager::AddLayer(const LayerInfo* toCopy, bool isFolder, int insertPosition)
{
	_errorMessage = "";

	LayerInfo newLayer = LayerInfo();

	LayerInfo* layer = nullptr;

	int layerPosition = 0;

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
		if (insertPosition == -1)
		{
			for (auto lit = _layers.begin(); lit < _layers.end(); lit++, idx++)
			{
				if (&(*lit) == toCopy)
				{
					_layers.insert(lit, std::move(newLayer));
					layer = &_layers[idx];
					layerPosition = idx;
					break;
				}
			}
		}
		else if (insertPosition >= 0)
		{
			if (insertPosition < _layers.size())
			{
				_layers.insert(_layers.begin() + insertPosition, std::move(newLayer));
				layer = &_layers[insertPosition];
				layerPosition = insertPosition;
			}
			else
			{
				_layers.insert(_layers.end(), std::move(newLayer));
				layer = &_layers.back();
				layerPosition = _layers.size() - 1;
			}
		}
	}
	else
	{
		_layers.insert(_layers.begin(), std::move(newLayer));
		layer = &_layers[0];

		layerPosition = 0;

		layer->_isFolder = isFolder;
		if (layer->_isFolder)
			layer->_name = "New Folder";
	}

#ifdef _WIN32
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
#else
	// Generate a UUID
	std::string guid = "";
	guid.resize(60, ' ');

	sprintf(guid.data(), "%x%x-%x-%x-%x-%x%x%x",
		rand(), rand(),                 // Generates a 64-bit Hex number
		rand(),                         // Generates a 32-bit Hex number
		((rand() & 0x0fff) | 0x4000),   // Generates a 32-bit Hex number of the form 4xxx (4 indicates the UUID version)
		rand() % 0x3fff + 0x8000,       // Generates a 32-bit Hex number in the range [0x8000, 0xbfff]
		rand(), rand(), rand());

#endif

	layer->_blinkTimer.restart();
	layer->_isBlinking = false;
	layer->_blinkVarDelay = GetRandom11() * layer->_blinkVariation;
	layer->_parent = this;
	layer->_id = guid;

	if(_appConfig->_unloadTimeoutEnabled)
		layer->SetUnloadingTimer(_appConfig->_unloadTimeout);
	else
		layer->SetUnloadingTimer(0);

	int childPosition = layerPosition + 1;
	for (auto& id : _layers[layerPosition]._folderContents)
	{
		LayerInfo* origChild = GetLayer(id);
		if (origChild != nullptr)
		{
			// remove the original child from its folder temporarily, so this duplicate doesn't get added to the original
			std::string origInFolder = origChild->_inFolder;
			origChild->_inFolder = "";

			LayerInfo* child = AddLayer(origChild, false, childPosition);

			// put the original back in its folder
			origChild = GetLayer(id);
			origChild->_inFolder = origInFolder;

			// add the new copy to this new folder
			child->_inFolder = guid;
			id = child->_id;
			childPosition++;
		}
	}

	//reassign since things probably got moved
	layer = &_layers[layerPosition];

	if (layer->_inFolder != "")
	{
		int folderIdx = 0;
		LayerInfo* folder = GetLayer(layer->_inFolder, &folderIdx);
		if (folder != nullptr)
		{
			folder->_folderContents.push_back(layer->_id);
			//MoveLayerTo(layerPosition, folderIdx + 1);

			std::sort(folder->_folderContents.begin(), folder->_folderContents.end(), [&](const std::string& a, const std::string& b) {
				for (LayerInfo& l : _layers)
				{
					if (l._id == a)
						return true;
					if (l._id == b)
						return false;
				}
				return false;
				});
		}
	}

	if (_appConfig->_createMinimalLayers == false && toCopy == nullptr)
	{
		layer->_useBlinkFrame = true;
		layer->_idleMotionEnabled = true;
		layer->_swapWhenTalking = true;
		layer->_bounceType = LayerInfo::BounceLoudness;
	}

	return layer;
}

void LayerManager::RemoveLayer(int toRemove)
{
	if (toRemove < 0 || toRemove >= _layers.size())
		return;

	if (_layers[toRemove]._isFolder)
	{
		for (std::string& id : _layers[toRemove]._folderContents)
		{
			LayerInfo* lyr = GetLayer(id);
			if (lyr)
				lyr->_inFolder = "";
		}
	}

	if (_clipRenderTextures.count(_layers[toRemove]._id))
		_clipRenderTextures.erase(_layers[toRemove]._id);

	_layers.erase(_layers.begin() + toRemove);
}

void LayerManager::MoveLayerTo(int toMove, int position, bool skipFolders)
{
	if (toMove < 0)
		return;

	if (toMove >= _layers.size())
		return;

	if (position < 0)
		position = 0;

	if (position > _layers.size() - 1)
		position = _layers.size() - 1;

	if (toMove == position && skipFolders == false)
		return;

	LayerInfo& origLayer = _layers[toMove];
	LayerInfo& targetLayer = _layers[position];

	std::string origID = origLayer._id;
	std::string targetID = targetLayer._id;

	if (origID == targetID)
	{
		if (skipFolders)
		{
			if (origLayer._inFolder != "")
			{
				LayerInfo* origFolder = GetLayer(origLayer._inFolder);
				for (int c = 0; c < origFolder->_folderContents.size(); c++)
				{
					if (origFolder->_folderContents[c] == origID)
					{
						origFolder->_folderContents.erase(origFolder->_folderContents.begin() + c);
						origLayer._inFolder = "";
						break;
					}
				}
			}

		}
		return;
	}


	int down = 0;
	if (position > toMove)
		down = 1;

	if (origLayer._inFolder != "")
	{
		LayerInfo* origFolder = GetLayer(origLayer._inFolder);
		if (origFolder)
		{
			// remove it from its original folder for now
			for (int c = 0; c < origFolder->_folderContents.size(); c++)
			{
				LayerInfo* child = GetLayer(origFolder->_folderContents[c]);
				if (child != nullptr && child->_id == origID)
				{
					origFolder->_folderContents.erase(origFolder->_folderContents.begin() + c);
					origLayer._inFolder = "";
					break;
				}
			}
		}
	}

	if (targetLayer._isFolder && skipFolders == false)
	{
		if (origLayer._isFolder)
			return;

		targetLayer._folderContents.push_back(origID);
		origLayer._inFolder = targetID;

		// invalidate the layer's header positions until it next gets drawn - to stop it being picked up by the drag/drop
		origLayer._lastHeaderPos = { -1,-1 };
		origLayer._lastHeaderScreenPos = { -1,-1 };
		origLayer._lastHeaderSize = { 0,0 };

		LayerInfo copy = _layers[toMove];
		RemoveLayer(toMove);

		// move the layer to be 1 below the folder layer in the main list
		int targetPosition = toMove;
		for (int l = 0; l < _layers.size(); l++)
		{
			LayerInfo* lyr = &_layers[l];
			if (lyr->_id == targetID)
			{
				targetPosition = l + 1;
				break;
			}
		}

		if (targetPosition < _layers.size())
			_layers.insert(_layers.begin() + targetPosition, copy);
		else
			_layers.push_back(copy);

		//re-find the target folder since it probably moved
		LayerInfo* targetLayer2 = GetLayer(targetID);

		std::sort(targetLayer2->_folderContents.begin(), targetLayer2->_folderContents.end(), [&](const std::string& a, const std::string& b) {
			for (LayerInfo& l : _layers)
			{
				if (l._id == a)
					return true;
				if (l._id == b)
					return false;
			}
			return false;
			});
	}
	else if (targetLayer._inFolder != "" && skipFolders == false)
	{
		if (origLayer._isFolder)
			return;

		LayerInfo* targetFolder = GetLayer(targetLayer._inFolder);

		if (origID == targetFolder->_id)
			return;

		//insert the id in the folder
		targetFolder->_folderContents.push_back(origLayer._id);
		std::string targFolderID = targetFolder->_id;
		origLayer._inFolder = targFolderID;

		LayerInfo copy = _layers[toMove];
		RemoveLayer(toMove);

		// move the layer to be 1 below the target layer in the main list
		int targetPosition = toMove;
		for (int c = 0; c < _layers.size(); c++)
		{
			LayerInfo* child = &_layers[c];
			if (child->_id == targetID)
			{
				targetPosition = c + down;
				break;
			}
		}

		_layers.insert(_layers.begin() + targetPosition, copy);

		//re-find the target folder since it probably moved
		LayerInfo* targetLayer2 = GetLayer(targFolderID);

		std::sort(targetLayer2->_folderContents.begin(), targetLayer2->_folderContents.end(), [&](const std::string& a, const std::string& b) {
			for (LayerInfo& l : _layers)
			{
				if (l._id == a)
					return true;
				if (l._id == b)
					return false;
			}
			return false;
			});
	}
	else
	{

		std::vector<std::pair<int, LayerInfo>> listToMove;
		listToMove.push_back({ toMove, _layers[toMove] });
		if (origLayer._isFolder)
		{
			for (std::string& child : origLayer._folderContents)
			{
				int childIdx = 0;
				LayerInfo* childLyr = GetLayer(child, &childIdx);
				if (childLyr != nullptr)
					listToMove.push_back({ childIdx , *childLyr });
			}
		}

		//remove in reverse to preserve order
		for (int r = listToMove.size() - 1; r > -1; r--)
		{
			RemoveLayer(listToMove[r].first);
		}

		//re-find the target folder since it probably moved
		int targetPosition = position;
		LayerInfo* targetLayer2 = GetLayer(targetID, &targetPosition);

		LayerInfo* movedPastFolder = nullptr;
		if (targetLayer2->_isFolder)
			movedPastFolder = targetLayer2;
		else if (targetLayer2->_inFolder != "")
			movedPastFolder = GetLayer(targetLayer2->_inFolder);

		if (movedPastFolder != nullptr && down != 0)
		{
			for (std::string& id : movedPastFolder->_folderContents)
			{
				int layerIdx = 0;
				GetLayer(id, &layerIdx);
				targetPosition = Max(targetPosition, layerIdx);
			}
		}

		for (int r = 0; r < listToMove.size(); r++)
		{
			_layers.insert(_layers.begin() + targetPosition + r + down, listToMove[r].second);
		}


	}
}

bool LayerManager::HandleLayerDrag(float mouseX, float mouseY, bool mousePressed)
{
	bool anyLayerPopupOpen = false;
	for (auto& l : _layers)
		if (l.AnyPopupOpen())
			anyLayerPopupOpen = true;

	if (_statesMenuOpen || _loadXMLOpen || _outOfFocus || anyLayerPopupOpen || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
	{
		_draggedLayer = -1;
		_dragActive = false;
		return false;
	}

	//for (LayerInfo& l : _layers)
	//{
	//	if (l.AnyPopupOpen())
	//		return false;
	//}

	int hoveredLayer = GetLayerUnderCursor(mouseX, mouseY);

	bool useInput = false;

	if (mousePressed == false)
	{
		_layerDragTimer.restart();

		// Drop the layer if active
		if (_dragActive == true)
		{
			if (hoveredLayer != -1)
			{
				if (hoveredLayer == -2)
					// above all, put at the absolute top, skipping folders
					MoveLayerTo(_draggedLayer, 0, true);
				else if (hoveredLayer == -3)
					// below all, put at the absolute bottom, skipping folders
					MoveLayerTo(_draggedLayer, _layers.size(), true);
				else
					MoveLayerTo(_draggedLayer, hoveredLayer);
			}
			useInput = true;
		}
		_draggedLayer = -1;
		_dragActive = false;
	}

	// test if mouse is within the layers window
	if (mouseY > _maxCursorDragY)
		return false;

	// If a mouse is newly down on a layer, save which layer and start the clock
	if (mousePressed == true && hoveredLayer >= 0)
	{
		if (_lastDragMouseDown == false)
		{
			_layerDragTimer.restart();
			_draggedLayer = hoveredLayer;
		}
	}

	// if mouse is pressed and held for long enough, drag
	if (mousePressed == true && _draggedLayer != -1 && _layerDragTimer.getElapsedTime().asSeconds() > 0.2)
	{
		_dragActive = true;
		useInput = true;

		_layerDragPos = { mouseX, mouseY };
	}

	_lastDragMouseDown = mousePressed;

	return useInput;
}

int LayerManager::GetLayerUnderCursor(float mouseX, float mouseY)
{
	bool aboveAll = true;
	bool belowAll = true;
	for (int l = 0; l < _layers.size(); l++)
	{
		auto& layer = _layers[l];
		float minX = layer._lastHeaderScreenPos.x;
		float minY = layer._lastHeaderScreenPos.y;
		float maxX = minX + layer._lastHeaderSize.x;
		float maxY = minY + layer._lastHeaderSize.y;

		if (mouseY > minY && minY != -1)
			aboveAll = false;

		if (mouseY < maxY)
			belowAll = false;

		if (mouseX >= minX && mouseX <= maxX && mouseY >= minY && mouseY <= maxY)
		{
			return l;
		}
	}

	if (aboveAll)
		return -2;

	if (belowAll)
		return -3;

	return -1;
}

void LayerManager::MoveLayerUp(int moveUp)
{
	int position = moveUp - 1;

	if (position < 0)
		return;

	LayerInfo& origLayer = _layers[moveUp];
	LayerInfo* targetLayer = &_layers[position];

	while (targetLayer->_inFolder != origLayer._inFolder)
	{
		position--;
		if (position < 0)
			return;

		targetLayer = &_layers[position];
	}

	MoveLayerTo(moveUp, position, origLayer._inFolder == "");
}

void LayerManager::MoveLayerDown(int moveDown)
{
	int position = moveDown + 1;

	if (position >= _layers.size())
		return;

	LayerInfo& origLayer = _layers[moveDown];
	LayerInfo* targetLayer = &_layers[position];

	while (targetLayer->_inFolder != origLayer._inFolder)
	{
		position++;
		if (position >= _layers.size())
			return;

		targetLayer = &_layers[position];
	}

	MoveLayerTo(moveDown, position, origLayer._inFolder == "");
}

void LayerManager::RemoveLayer(LayerInfo* toRemove)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == toRemove)
		{
			RemoveLayer(l);
			break;
		}
	}
}

void LayerManager::MoveLayerUp(LayerInfo* moveUp)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == moveUp)
		{
			MoveLayerUp(l);
			break;
		}
	}
}

void LayerManager::MoveLayerDown(LayerInfo* moveDown)
{
	for (int l = 0; l < _layers.size(); l++)
	{
		if (&_layers[l] == moveDown)
		{
			MoveLayerDown(l);
			break;
		}
	}
}

void LayerManager::MakePortablePath(std::string& path)
{
	path = fs::proximate(path, _appConfig->_appLocation).string();
	std::replace(path.begin(), path.end(), '\\', '/');
}

bool LayerManager::SaveLayers(const std::string& settingsFileName, bool makePortable, bool copyImages)
{
	if (_loadingFinished == false)
	{
		return false;
	}
	else if (_loadingThread != nullptr)
	{
		if (_loadingThread->joinable())
			_loadingThread->join();

		delete _loadingThread;
		_loadingThread = nullptr;
	}

	_errorMessage = "";

	tinyxml2::XMLDocument doc;

	doc.LoadFile(settingsFileName.c_str());

	logToFile(_appConfig, "Saving " + settingsFileName);

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
		logToFile(_appConfig, "Save Failed: " + _errorMessage);
		return false;
	}

	auto layers = root->FirstChildElement("layers");
	if (!layers)
		layers = root->InsertFirstChild(doc.NewElement("layers"))->ToElement();

	if (!layers)
	{
		_errorMessage = "Could not save layers element: " + settingsFileName;
		logToFile(_appConfig, "Save Failed: " + _errorMessage);
		return false;
	}

	if (makePortable && copyImages)
	{
		fs::path targetFolder(_appConfig->_appLocation);
		targetFolder.append(_layerSetName);
		std::error_code cec;
		fs::create_directory(targetFolder, cec);

		fs::copy_options copyOpts = fs::copy_options::update_existing;

		for (auto& layer : _layers)
		{
			CopyFileAndUpdatePath(layer._idleImagePath, targetFolder, copyOpts);
			CopyFileAndUpdatePath(layer._talkSpritePath, targetFolder, copyOpts);
			CopyFileAndUpdatePath(layer._blinkSpritePath, targetFolder, copyOpts);
			CopyFileAndUpdatePath(layer._talkBlinkSpritePath, targetFolder, copyOpts);
			CopyFileAndUpdatePath(layer._screamSpritePath, targetFolder, copyOpts);
		}
	}

	layers->DeleteChildren();

	ResetStates();

	for (int l = 0; l < _layers.size(); l++)
	{
		auto thisLayer = layers->InsertEndChild(doc.NewElement("layer"))->ToElement();

		auto& layer = _layers[l];

		thisLayer->SetAttribute("id", layer._id.c_str());

		thisLayer->SetAttribute("name", layer._name.c_str());
		thisLayer->SetAttribute("visible", layer._visible);

		SaveColor(thisLayer, &doc, "layerColor", layer._layerColor);

		if (layer._isFolder == false)
		{
			thisLayer->SetAttribute("talking", layer._swapWhenTalking);
			thisLayer->SetAttribute("talkThreshold", layer._talkThreshold);
			thisLayer->SetAttribute("smoothInput", layer._smoothTalkFactor);
			thisLayer->SetAttribute("smoothAmount", layer._smoothTalkFactorSize);
			thisLayer->SetAttribute("restartOnSwap", layer._restartTalkAnim);

			thisLayer->SetAttribute("useBlink", layer._useBlinkFrame);
			thisLayer->SetAttribute("talkBlink", layer._blinkWhileTalking);
			thisLayer->SetAttribute("blinkTime", layer._blinkDelay);
			thisLayer->SetAttribute("blinkDur", layer._blinkDuration);
			thisLayer->SetAttribute("blinkVar", layer._blinkVariation);

			thisLayer->SetAttribute("bounceType", layer._bounceType);
			thisLayer->SetAttribute("bounceHeight", layer._bounceMove.y);
			thisLayer->SetAttribute("bounceMoveX", layer._bounceMove.x);
			thisLayer->SetAttribute("bounceRotation", layer._bounceRotation);
			thisLayer->SetAttribute("bounceScaleX", layer._bounceScale.x);
			thisLayer->SetAttribute("bounceScaleY", layer._bounceScale.y);

			thisLayer->SetAttribute("bounceTime", layer._bounceFrequency);

			thisLayer->SetAttribute("breathing", layer._idleMotionEnabled);
			thisLayer->SetAttribute("breathHeight", layer._breathMove.y);
			thisLayer->SetAttribute("breathTime", layer._breathFrequency);
			thisLayer->SetAttribute("breathMoveX", layer._breathMove.x);
			thisLayer->SetAttribute("breathScaleX", layer._breathScale.x);
			thisLayer->SetAttribute("breathScaleY", layer._breathScale.y);
			thisLayer->SetAttribute("breathCircular", layer._breathCircular);
			thisLayer->SetAttribute("breatheWhileTalking", layer._breatheWhileTalking);

			thisLayer->SetAttribute("breathRotation", layer._breathRotation);
			thisLayer->SetAttribute("doBreathTint", layer._doBreathTint);
			SaveColor(thisLayer, &doc, "breathTint", layer._breathTint);

			thisLayer->SetAttribute("screaming", layer._scream);
			thisLayer->SetAttribute("screamThreshold", layer._screamThreshold);
			thisLayer->SetAttribute("screamVibrate", layer._screamVibrate);
			thisLayer->SetAttribute("screamVibrateAmount", layer._screamVibrateAmount);
			thisLayer->SetAttribute("screamVibrateSpeed", layer._screamVibrateSpeed);
			thisLayer->SetAttribute("minScreamTime", layer._minScreamTime);

			thisLayer->SetAttribute("constantHeight", layer._constantPos.y);
			thisLayer->SetAttribute("constantMoveX", layer._constantPos.x);
			thisLayer->SetAttribute("constantRotation", layer._constantRot);
			thisLayer->SetAttribute("constantScaleX", layer._constantScale.x);
			thisLayer->SetAttribute("constantScaleY", layer._constantScale.y);

			thisLayer->SetAttribute("restartAnimsOnVisible", layer._restartAnimsOnVisible);

			if (makePortable)
			{
				MakePortablePath(layer._idleImagePath);
				MakePortablePath(layer._talkSpritePath);
				MakePortablePath(layer._blinkSpritePath);
				MakePortablePath(layer._talkBlinkSpritePath);
				MakePortablePath(layer._screamSpritePath);
			}

			thisLayer->SetAttribute("idlePath", layer._idleImagePath.c_str());
			thisLayer->SetAttribute("talkPath", layer._talkSpritePath.c_str());
			thisLayer->SetAttribute("blinkPath", layer._blinkSpritePath.c_str());
			thisLayer->SetAttribute("talkBlinkPath", layer._talkBlinkSpritePath.c_str());
			thisLayer->SetAttribute("screamPath", layer._screamSpritePath.c_str());

			if (layer._idleSprite->FrameCount() > 1 || layer._idleSprite->GridSize() != sf::Vector2i(1, 1) || layer._animsSynced == true)
				SaveAnimInfo(thisLayer, &doc, "idleAnim", *layer._idleSprite, layer._animsSynced);

			if (layer._talkSprite->FrameCount() > 1 || layer._talkSprite->GridSize() != sf::Vector2i(1, 1) || layer._animsSynced == true)
				SaveAnimInfo(thisLayer, &doc, "talkAnim", *layer._talkSprite, layer._animsSynced);

			if (layer._blinkSprite->FrameCount() > 1 || layer._blinkSprite->GridSize() != sf::Vector2i(1, 1) || layer._animsSynced == true)
				SaveAnimInfo(thisLayer, &doc, "blinkAnim", *layer._blinkSprite, layer._animsSynced);

			if (layer._talkBlinkSprite->FrameCount() > 1 || layer._talkBlinkSprite->GridSize() != sf::Vector2i(1, 1) || layer._animsSynced == true)
				SaveAnimInfo(thisLayer, &doc, "talkBlinkAnim", *layer._talkBlinkSprite, layer._animsSynced);

			if (layer._screamSprite->FrameCount() > 1 || layer._screamSprite->GridSize() != sf::Vector2i(1, 1) || layer._animsSynced == true)
				SaveAnimInfo(thisLayer, &doc, "screamAnim", *layer._screamSprite, layer._animsSynced);

			thisLayer->SetAttribute("syncAnims", layer._animsSynced);

			SaveColor(thisLayer, &doc, "idleTint", layer._idleTint);
			SaveColor(thisLayer, &doc, "talkTint", layer._talkTint);
			SaveColor(thisLayer, &doc, "blinkTint", layer._blinkTint);
			SaveColor(thisLayer, &doc, "talkBlinkTint", layer._talkBlinkTint);
			SaveColor(thisLayer, &doc, "screamTint", layer._screamTint);

			thisLayer->SetAttribute("smoothTalkTint", layer._smoothTalkTint);

			thisLayer->SetAttribute("scaleX", layer._scale.x);
			thisLayer->SetAttribute("scaleY", layer._scale.y);
			thisLayer->SetAttribute("posX", layer._pos.x);
			thisLayer->SetAttribute("posY", layer._pos.y);
			thisLayer->SetAttribute("rot", layer._rot);

			thisLayer->SetAttribute("pivotX", layer._pivot.x);
			thisLayer->SetAttribute("pivotY", layer._pivot.y);

			thisLayer->SetAttribute("rotateChildren", layer._passRotationToChildLayers);

			thisLayer->SetAttribute("motionParent", layer._motionParent.c_str());
			thisLayer->SetAttribute("motionDelayTime", layer._motionDelay);
			thisLayer->SetAttribute("hideWithParent", layer._hideWithParent);
			thisLayer->SetAttribute("motionDrag", layer._motionDrag);
			thisLayer->SetAttribute("motionSpring", layer._motionSpring);
			thisLayer->SetAttribute("distanceLimit", layer._distanceLimit);
			thisLayer->SetAttribute("rotationEffect", layer._rotationEffect);
			thisLayer->SetAttribute("inheritTint", layer._inheritTint);

			thisLayer->SetAttribute("allowIndividualMotion", layer._allowIndividualMotion);
			thisLayer->SetAttribute("rotationIgnorePivots", layer._physicsIgnorePivots);

			thisLayer->SetAttribute("motionStretch", (int)layer._motionStretch);
			thisLayer->SetAttribute("stretchStrengthX", layer._motionStretchStrength.x);
			thisLayer->SetAttribute("stretchStrengthY", layer._motionStretchStrength.y);
			thisLayer->SetAttribute("stretchMinX", layer._stretchScaleMin.x);
			thisLayer->SetAttribute("stretchMinY", layer._stretchScaleMin.y);
			thisLayer->SetAttribute("stretchMaxX", layer._stretchScaleMax.x);
			thisLayer->SetAttribute("stretchMaxY", layer._stretchScaleMax.y);

			thisLayer->SetAttribute("weightPosX", layer._weightDirection.x);
			thisLayer->SetAttribute("weightPosY", layer._weightDirection.y);

			thisLayer->SetAttribute("trackingEnabled", layer._trackingEnabled);
			thisLayer->SetAttribute("trackingType", layer._trackingType);
			thisLayer->SetAttribute("followElliptical", layer._followElliptical);
			thisLayer->SetAttribute("mouseNeutralX", layer._mouseNeutralPos.x);
			thisLayer->SetAttribute("mouseNeutralY", layer._mouseNeutralPos.y);
			thisLayer->SetAttribute("mouseAreaX", layer._mouseAreaSize.x);
			thisLayer->SetAttribute("mouseAreaY", layer._mouseAreaSize.y);
			thisLayer->SetAttribute("trackingAxis", layer._trackingAxis);
			thisLayer->SetAttribute("trackingDeadzone", layer._axisDeadzone);
			thisLayer->SetAttribute("trackingSmooth", layer._trackingSmooth);
			thisLayer->SetAttribute("trackingLimitX", layer._trackingMoveLimits.x);
			thisLayer->SetAttribute("trackingLimitY", layer._trackingMoveLimits.y);
			thisLayer->SetAttribute("untrackedWhenHidden", layer._trackingOffWhenHidden);
			thisLayer->SetAttribute("mouseEffect", layer._mouseEffect);
			thisLayer->SetAttribute("joypadEffect", layer._joypadEffect);
			thisLayer->SetAttribute("trackingRotLimitX", layer._trackingRotation.x);
			thisLayer->SetAttribute("trackingRotLimitY", layer._trackingRotation.y);
			thisLayer->SetAttribute("trackingSelect", layer._trackingSelect);
			if(layer._trackingSelect == LayerInfo::TRACKINGSELECT_SPECIFIC)
				thisLayer->SetAttribute("trackingControllerID", layer._trackingJoystick);
			thisLayer->SetAttribute("trackingScaleMode", layer._trackingScaleMode);
			thisLayer->SetAttribute("trackingScaleHorizontalX", layer._trackingScaleHorizontal.x);
			thisLayer->SetAttribute("trackingScaleHorizontalY", layer._trackingScaleHorizontal.y);
			thisLayer->SetAttribute("trackingScaleVerticalX", layer._trackingScaleVertical.x);
			thisLayer->SetAttribute("trackingScaleVerticalY", layer._trackingScaleVertical.y);
			thisLayer->SetAttribute("clampTrackingScale", layer._clampTrackingScale);
			thisLayer->SetAttribute("trackingScaleClampX", layer._trackingScaleClamp.x);
			thisLayer->SetAttribute("trackingScaleClampY", layer._trackingScaleClamp.y);
			thisLayer->SetAttribute("trackingScaleAbsolute", layer._trackingScaleAbsolute);

			thisLayer->SetAttribute("pinLoaded", layer._pinLoaded);

			std::string bmName = "Normal";
			for (auto& bm : g_blendmodes)
				if (bm.second == layer._blendMode)
					bmName = bm.first;

			thisLayer->SetAttribute("blendMode", bmName.c_str());

			thisLayer->SetAttribute("scaleFilter", (bool)layer._scaleFiltering);
			thisLayer->SetAttribute("alphaCutoff", layer._alphaClip);

			thisLayer->SetAttribute("clipID", layer._clipID.c_str());
		}

		thisLayer->SetAttribute("isFolder", layer._isFolder);

		if (layer._isFolder == false)
		{
			thisLayer->SetAttribute("inFolder", layer._inFolder.c_str());
		}

		auto folderContent = thisLayer->FirstChildElement("folderContent");
		if (!folderContent)
			folderContent = thisLayer->InsertFirstChild(doc.NewElement("folderContent"))->ToElement();

		for (int h = 0; h < layer._folderContents.size(); h++)
		{
			auto thisID = folderContent->InsertEndChild(doc.NewElement("id"))->ToElement();
			thisID->SetText(layer._folderContents[h].c_str());
		}
	}

	root->SetAttribute("globalScaleX", _globalScale.x);
	root->SetAttribute("globalScaleY", _globalScale.y);
	root->SetAttribute("globalPosX", _globalPos.x);
	root->SetAttribute("globalPosY", _globalPos.y);
	root->SetAttribute("globalRot", _globalRot);

	root->SetAttribute("statesPassThrough", _statesPassThrough);
	root->SetAttribute("statesHideUnaffected", _statesHideUnaffected);
	root->SetAttribute("statesIgnoreAxis", _statesIgnoreStick);

	root->SetAttribute("DisableRotationEffectFix", _appConfig->_undoRotationEffectFix);

	auto canvasPresets = root->FirstChildElement("CanvasPresets");
	if (!canvasPresets)
		canvasPresets = root->InsertFirstChild(doc.NewElement("CanvasPresets"))->ToElement();

	if (!canvasPresets)
	{
		_errorMessage = "Could not save CanvasPresets element: " + settingsFileName;
		logToFile(_appConfig, "Save Failed: " + _errorMessage);
		return false;
	}

	canvasPresets->DeleteChildren();

	canvasPresets->SetAttribute("currentPreset", _currentGlobalPreset);

	for (int gp = 0; gp < _globalPresets.size(); gp++)
	{
		auto thisPresetElmt = canvasPresets->InsertEndChild(doc.NewElement("Preset"))->ToElement();
		const auto& thisPreset = _globalPresets[gp];

		thisPresetElmt->SetAttribute("name", thisPreset._name.c_str());
		thisPresetElmt->SetAttribute("scaleX", thisPreset._scale.x);
		thisPresetElmt->SetAttribute("scaleY", thisPreset._scale.y);
		thisPresetElmt->SetAttribute("posX", thisPreset._pos.x);
		thisPresetElmt->SetAttribute("posY", thisPreset._pos.y);
		thisPresetElmt->SetAttribute("rot", thisPreset._rot);
	}

	auto hotkeys = root->FirstChildElement("hotkeys");
	if (!hotkeys)
		hotkeys = root->InsertFirstChild(doc.NewElement("hotkeys"))->ToElement();

	if (!hotkeys)
	{
		_errorMessage = "Could not save hotkeys element: " + settingsFileName;
		logToFile(_appConfig, "Save Failed: " + _errorMessage);
		return false;
	}

	hotkeys->DeleteChildren();

	for (int h = 0; h < _states.size(); h++)
	{
		auto thisHotkey = hotkeys->InsertEndChild(doc.NewElement("hotkey"))->ToElement();
		const auto& stateInfo = _states[h];

		thisHotkey->SetAttribute("enabled", stateInfo._enabled);

		thisHotkey->SetAttribute("name", stateInfo._name.c_str());

		if (stateInfo._key != sf::Keyboard::Unknown)
			thisHotkey->SetAttribute("key", (int)stateInfo._key);
		if (stateInfo._scancode != sf::Keyboard::Scan::Unknown)
			thisHotkey->SetAttribute("scancode", (int)stateInfo._scancode);

		thisHotkey->SetAttribute("ctrl", stateInfo._ctrl);
		thisHotkey->SetAttribute("shift", stateInfo._shift);
		thisHotkey->SetAttribute("alt", stateInfo._alt);

		thisHotkey->SetAttribute("axis", (int)stateInfo._jAxis);
		thisHotkey->SetAttribute("btn", stateInfo._jButton);
		thisHotkey->SetAttribute("jpadID", stateInfo._jPadID);
		thisHotkey->SetAttribute("axisDir", stateInfo._jDir);

		thisHotkey->SetAttribute("mouseButton", stateInfo._mouseButton);

		thisHotkey->SetAttribute("timeout", stateInfo._timeout);
		thisHotkey->SetAttribute("useTimeout", stateInfo._useTimeout);
		thisHotkey->SetAttribute("activeType", stateInfo._activeType);
		thisHotkey->SetAttribute("canTrigger", stateInfo._canTrigger);
		thisHotkey->SetAttribute("threshold", stateInfo._threshold);

		thisHotkey->DeleteAttribute("toggle");

		thisHotkey->SetAttribute("schedule", stateInfo._schedule);
		thisHotkey->SetAttribute("interval", stateInfo._intervalTime);
		thisHotkey->SetAttribute("variation", stateInfo._intervalVariation);

		{
			std::scoped_lock lockState(_stateLocks[h]);
			for (auto& state : stateInfo._layerStates)
			{
				if (state.second != StatesInfo::State::NoChange)
				{
					auto thisState = thisHotkey->InsertEndChild(doc.NewElement("state"))->ToElement();
					thisState->SetAttribute("id", state.first.c_str());
					thisState->SetAttribute("state", state.second);

				}
			}
		}
	}

	std::string outFile = settingsFileName;
	if (outFile.find("/") == std::string::npos && outFile.find("\\") == std::string::npos)
	{
		outFile = fs::path(_appConfig->_appLocation).append(outFile).string();
	}

	doc.SaveFile(outFile.c_str());

	if (doc.Error())
	{
		_errorMessage = "Failed to save document: " + outFile;
		logToFile(_appConfig, "Save Failed: " + _errorMessage);
		return false;
	}

	logToFile(_appConfig, "Saved Layer Set " + outFile);

	_lastSavedLocation = outFile;

	return true;
}

bool LayerManager::LoadLayers(const std::string& settingsFileName)
{
	if (!_loadingFinished)
		return false;

	_loadingFinished = false;

	_errorMessage = "";
	_loadingProgress = "";

	_loadingPath = fs::path(settingsFileName).replace_extension(".xml").string();

	if (_loadingThread != nullptr)
	{
		if (_loadingThread->joinable())
			_loadingThread->join();

		delete _loadingThread;
		_loadingThread = nullptr;
	}

	logToFile(_appConfig, "Loading " + settingsFileName);

	_loadingThread = new std::thread([&]
		{
			tinyxml2::XMLDocument doc;
			doc.LoadFile(_loadingPath.c_str());

			if (doc.Error())
			{
				_loadingPath = _appConfig->_appLocation + _loadingPath;
				doc.LoadFile((_loadingPath).c_str());

				if (doc.Error())
				{
					if (fs::path(_loadingPath).filename() != "lastLayers.xml")
						_errorMessage = "Could not read document: " + _loadingPath;

					_loadingPath = "";
					_loadingFinished = true;
					_uiConfig->_menuShowing = true;
					logToFile(_appConfig, "Load Failed: " + _errorMessage);
					return;
				}
			}

			auto root = doc.FirstChildElement("Config");
			if (!root)
			{
				_errorMessage = "Invalid config element in " + _loadingPath;
				_loadingPath = "";
				_loadingFinished = true;
				_uiConfig->_menuShowing = true;
				logToFile(_appConfig, "Load Failed: " + _errorMessage);
				return;
			}

			auto layers = root->FirstChildElement("layers");
			if (!layers)
			{
				_errorMessage = "Invalid layers element in " + _loadingPath;
				_loadingPath = "";
				_loadingFinished = true;
				_uiConfig->_menuShowing = true;
				logToFile(_appConfig, "Load Failed: " + _errorMessage);
				return;
			}

			_loadingFinished = false;

			_statesOrder.clear();
			_layers.clear();
			_textureMan->Reset();

			_lastSavedLocation = _loadingPath;

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
				_loadingProgress = name;

				thisLayer->QueryAttribute("talking", &layer._swapWhenTalking);
				thisLayer->QueryAttribute("talkThreshold", &layer._talkThreshold);
				thisLayer->QueryAttribute("smoothInput", &layer._smoothTalkFactor);
				thisLayer->QueryAttribute("smoothAmount", &layer._smoothTalkFactorSize);
				thisLayer->QueryAttribute("restartOnSwap", &layer._restartTalkAnim);

				thisLayer->QueryAttribute("useBlink", &layer._useBlinkFrame);
				thisLayer->QueryAttribute("talkBlink", &layer._blinkWhileTalking);
				thisLayer->QueryAttribute("blinkTime", &layer._blinkDelay);
				thisLayer->QueryAttribute("blinkDur", &layer._blinkDuration);
				thisLayer->QueryAttribute("blinkVar", &layer._blinkVariation);

				int bobtype = 0;
				thisLayer->QueryIntAttribute("bounceType", &bobtype);
				layer._bounceType = (LayerInfo::BounceType)bobtype;
				thisLayer->QueryAttribute("bounceHeight", &layer._bounceMove.y);
				thisLayer->QueryAttribute("bounceMoveX", &layer._bounceMove.x);
				thisLayer->QueryAttribute("bounceRotation", &layer._bounceRotation);
				thisLayer->QueryAttribute("bounceScaleX", &layer._bounceScale.x);
				thisLayer->QueryAttribute("bounceScaleY", &layer._bounceScale.y);
				if (layer._bounceScale.x != layer._bounceScale.y)
					layer._bounceScaleConstrain = false;

				thisLayer->QueryAttribute("bounceTime", &layer._bounceFrequency);

				thisLayer->QueryAttribute("breathing", &layer._idleMotionEnabled);
				thisLayer->QueryAttribute("breathHeight", &layer._breathMove.y);
				thisLayer->QueryAttribute("breathTime", &layer._breathFrequency);
				thisLayer->QueryAttribute("breathMoveX", &layer._breathMove.x);
				thisLayer->QueryAttribute("breathScaleX", &layer._breathScale.x);
				thisLayer->QueryAttribute("breathScaleY", &layer._breathScale.y);
				if (layer._breathScale.x != layer._breathScale.y)
					layer._breathScaleConstrain = false;

				thisLayer->QueryAttribute("breathCircular", &layer._breathCircular);
				thisLayer->QueryAttribute("breatheWhileTalking", &layer._breatheWhileTalking);

				thisLayer->QueryAttribute("breathRotation", &layer._breathRotation);
				thisLayer->QueryAttribute("doBreathTint", &layer._doBreathTint);
				LoadColor(thisLayer, &doc, "breathTint", layer._breathTint);

				thisLayer->QueryAttribute("screaming", &layer._scream);
				thisLayer->QueryAttribute("screamThreshold", &layer._screamThreshold);
				thisLayer->QueryAttribute("screamVibrate", &layer._screamVibrate);
				thisLayer->QueryAttribute("screamVibrateAmount", &layer._screamVibrateAmount);
				thisLayer->QueryAttribute("screamVibrateSpeed", &layer._screamVibrateSpeed);
				thisLayer->QueryAttribute("minScreamTime", &layer._minScreamTime);

				thisLayer->QueryAttribute("constantHeight", &layer._constantPos.y);
				thisLayer->QueryAttribute("constantMoveX", &layer._constantPos.x);
				thisLayer->QueryAttribute("constantRotation", &layer._constantRot);
				thisLayer->QueryAttribute("constantScaleX", &layer._constantScale.x);
				thisLayer->QueryAttribute("constantScaleY", &layer._constantScale.y);

				thisLayer->QueryAttribute("restartAnimsOnVisible", &layer._restartAnimsOnVisible);

				if (const char* idlePth = thisLayer->Attribute("idlePath"))
					layer._idleImagePath = idlePth;
				if (const char* talkPth = thisLayer->Attribute("talkPath"))
					layer._talkSpritePath = talkPth;
				if (const char* blkPth = thisLayer->Attribute("blinkPath"))
					layer._blinkSpritePath = blkPth;
				if (const char* talkBlkPth = thisLayer->Attribute("talkBlinkPath"))
					layer._talkBlinkSpritePath = talkBlkPth;
				if (const char* screamPth = thisLayer->Attribute("screamPath"))
					layer._screamSpritePath = screamPth;

				fs::current_path(_appConfig->_appLocation);

				if (layer._idleImagePath != "")
					layer._idleSprite->LoadFromTexture(_textureMan, TryAbsolutePath(layer._idleImagePath).string(), 1, 1, 1, 1, { -1, -1 }, &_errorMessage);
				if (layer._talkSpritePath != "")
					layer._talkSprite->LoadFromTexture(_textureMan, TryAbsolutePath(layer._talkSpritePath).string(), 1, 1, 1, 1, { -1, -1 }, &_errorMessage);
				if (layer._blinkSpritePath != "")
					layer._blinkSprite->LoadFromTexture(_textureMan, TryAbsolutePath(layer._blinkSpritePath).string(), 1, 1, 1, 1, { -1, -1 }, &_errorMessage);
				if (layer._talkBlinkSpritePath != "")
					layer._talkBlinkSprite->LoadFromTexture(_textureMan, TryAbsolutePath(layer._talkBlinkSpritePath).string(), 1, 1, 1, 1, { -1, -1 }, &_errorMessage);
				if (layer._screamSpritePath != "")
					layer._screamSprite->LoadFromTexture(_textureMan, TryAbsolutePath(layer._screamSpritePath).string(), 1, 1, 1, 1, { -1, -1 }, &_errorMessage);

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

				LoadColor(thisLayer, &doc, "layerColor", layer._layerColor);

				thisLayer->QueryAttribute("smoothTalkTint", &layer._smoothTalkTint);

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

				bool oldMouseTrack = false;
				thisLayer->QueryBoolAttribute("followMouse", &oldMouseTrack);
				if (oldMouseTrack)
				{
					layer._trackingType = LayerInfo::TRACKING_MOUSE;
					layer._trackingEnabled = true;
				}

				thisLayer->QueryBoolAttribute("trackingEnabled", &layer._trackingEnabled);

				thisLayer->QueryIntAttribute("trackingType", (int*)&layer._trackingType);

				thisLayer->QueryBoolAttribute("followElliptical", &layer._followElliptical);
				thisLayer->QueryAttribute("mouseNeutralX", &layer._mouseNeutralPos.x);
				thisLayer->QueryAttribute("mouseNeutralY", &layer._mouseNeutralPos.y);
				thisLayer->QueryAttribute("mouseAreaX", &layer._mouseAreaSize.x);
				thisLayer->QueryAttribute("mouseAreaY", &layer._mouseAreaSize.y);

				//kept for compatibility
				thisLayer->QueryAttribute("mouseLimitX", &layer._trackingMoveLimits.x);
				thisLayer->QueryAttribute("mouseLimitY", &layer._trackingMoveLimits.y);

				thisLayer->QueryAttribute("trackingAxis", (int*) & layer._trackingAxis);
				thisLayer->QueryAttribute("trackingDeadzone", &layer._axisDeadzone);

				thisLayer->QueryAttribute("trackingSmooth", &layer._trackingSmooth);
				thisLayer->QueryAttribute("trackingLimitX", &layer._trackingMoveLimits.x);
				thisLayer->QueryAttribute("trackingLimitY", &layer._trackingMoveLimits.y);
				thisLayer->QueryBoolAttribute("untrackedWhenHidden", &layer._trackingOffWhenHidden);
				thisLayer->QueryAttribute("mouseEffect", &layer._mouseEffect);
				thisLayer->QueryAttribute("joypadEffect", &layer._joypadEffect);
				thisLayer->QueryAttribute("trackingRotLimitX", &layer._trackingRotation.x);
				thisLayer->QueryAttribute("trackingRotLimitY", &layer._trackingRotation.y);

				thisLayer->QueryAttribute("trackingSelect", (int*) & layer._trackingSelect);
				if (layer._trackingSelect == LayerInfo::TRACKINGSELECT_SPECIFIC)
					thisLayer->QueryAttribute("trackingControllerID", &layer._trackingJoystick);

				thisLayer->QueryAttribute("trackingScaleMode", (int*)&layer._trackingScaleMode);
				thisLayer->QueryAttribute("trackingScaleHorizontalX", &layer._trackingScaleHorizontal.x);
				thisLayer->QueryAttribute("trackingScaleHorizontalY", &layer._trackingScaleHorizontal.y);
				thisLayer->QueryAttribute("trackingScaleVerticalX", &layer._trackingScaleVertical.x);
				thisLayer->QueryAttribute("trackingScaleVerticalY", &layer._trackingScaleVertical.y);
				thisLayer->QueryBoolAttribute("clampTrackingScale", &layer._clampTrackingScale);
				thisLayer->QueryAttribute("trackingScaleClampX", &layer._trackingScaleClamp.x);
				thisLayer->QueryAttribute("trackingScaleClampY", &layer._trackingScaleClamp.y);
				thisLayer->QueryAttribute("trackingScaleAbsolute", &layer._trackingScaleAbsolute);
	

				const char* mpguid = thisLayer->Attribute("motionParent");
				if (mpguid)
					layer._motionParent = mpguid;

				float motionDelayFrames = 0;
				if (thisLayer->QueryAttribute("motionDelay", &motionDelayFrames) == tinyxml2::XML_SUCCESS)
				{
					layer._motionDelay = motionDelayFrames * (1.0 / 60.0);
				}

				thisLayer->QueryAttribute("motionDelayTime", &layer._motionDelay);
				thisLayer->QueryAttribute("hideWithParent", &layer._hideWithParent);
				thisLayer->QueryAttribute("motionDrag", &layer._motionDrag);
				thisLayer->QueryAttribute("motionSpring", &layer._motionSpring);
				thisLayer->QueryAttribute("distanceLimit", &layer._distanceLimit);
				thisLayer->QueryAttribute("rotationEffect", &layer._rotationEffect);
				thisLayer->QueryAttribute("inheritTint", &layer._inheritTint);
				thisLayer->QueryAttribute("allowIndividualMotion", &layer._allowIndividualMotion);
				thisLayer->QueryAttribute("rotationIgnorePivots", &layer._physicsIgnorePivots);

				thisLayer->QueryAttribute("motionStretch", (int*)&layer._motionStretch);
				thisLayer->QueryAttribute("stretchStrengthX", &layer._motionStretchStrength.x);
				thisLayer->QueryAttribute("stretchStrengthY", &layer._motionStretchStrength.y);
				thisLayer->QueryAttribute("stretchMinX", &layer._stretchScaleMin.x);
				thisLayer->QueryAttribute("stretchMinY", &layer._stretchScaleMin.y);
				thisLayer->QueryAttribute("stretchMaxX", &layer._stretchScaleMax.x);
				thisLayer->QueryAttribute("stretchMaxY", &layer._stretchScaleMax.y);

				thisLayer->QueryAttribute("weightPosX", &layer._weightDirection.x);
				thisLayer->QueryAttribute("weightPosY", &layer._weightDirection.y);

				// default to true if it has no parents (v12.0 compatibility)
				if (layer._motionParent == "")
					layer._passRotationToChildLayers = true;

				thisLayer->QueryAttribute("rotateChildren", &layer._passRotationToChildLayers);

				thisLayer->QueryBoolAttribute("pinLoaded", &layer._pinLoaded);

				layer._blendMode = g_blendmodes["Normal"];
				if (const char* blend = thisLayer->Attribute("blendMode"))
				{
					if (g_blendmodes.find(blend) != g_blendmodes.end())
						layer._blendMode = g_blendmodes[blend];
				}

				bool scalefilter = false;
				int scaleFilterInt = 0;
				if (thisLayer->QueryIntAttribute("scaleFilter", &scaleFilterInt) == tinyxml2::XML_SUCCESS)
					layer._scaleFiltering = scaleFilterInt;
				if (thisLayer->QueryBoolAttribute("scaleFilter", &scalefilter) == tinyxml2::XML_SUCCESS)
					layer._scaleFiltering = scalefilter;

				thisLayer->QueryAttribute("alphaCutoff", &layer._alphaClip);

				const char* clipGuid = thisLayer->Attribute("clipID");
				if (clipGuid)
					layer._clipID = clipGuid;


					layer._idleSprite->setSmooth(layer._scaleFiltering);
					layer._talkSprite->setSmooth(layer._scaleFiltering);
					layer._blinkSprite->setSmooth(layer._scaleFiltering);
					layer._talkBlinkSprite->setSmooth(layer._scaleFiltering);

				thisLayer->QueryAttribute("isFolder", &layer._isFolder);

				const char* inFolder = thisLayer->Attribute("inFolder");
				if (inFolder)
					layer._inFolder = inFolder;

				auto folderContent = thisLayer->FirstChildElement("folderContent");
				if (folderContent)
				{
					auto thisID = folderContent->FirstChildElement("id");

					while (thisID)
					{
						layer._folderContents.push_back(thisID->GetText());
						thisID = thisID->NextSiblingElement("id");
					}
				}

				thisLayer = thisLayer->NextSiblingElement("layer");
			}

			root->QueryAttribute("globalScaleX", &_globalScale.x);
			root->QueryAttribute("globalScaleY", &_globalScale.y);
			root->QueryAttribute("globalPosX", &_globalPos.x);
			root->QueryAttribute("globalPosY", &_globalPos.y);
			root->QueryAttribute("globalRot", &_globalRot);

			root->QueryAttribute("statesPassThrough", &_statesPassThrough);
			root->QueryAttribute("statesHideUnaffected", &_statesHideUnaffected);
			root->QueryAttribute("statesIgnoreAxis", &_statesIgnoreStick);

			root->QueryAttribute("DisableRotationEffectFix", &_appConfig->_undoRotationEffectFix);

			_globalPresets.clear();

			auto canvasPresets = root->FirstChildElement("CanvasPresets");
			if (canvasPresets)
			{
				canvasPresets->QueryAttribute("currentPreset", &_currentGlobalPreset);

				auto thisPresetElmt = canvasPresets->FirstChildElement("Preset");
				while (thisPresetElmt)
				{
					GlobalPreset thisPreset;
					if (const char* storedName = thisPresetElmt->Attribute("name"))
						thisPreset._name = storedName;

					thisPresetElmt->QueryAttribute("scaleX", &thisPreset._scale.x);
					thisPresetElmt->QueryAttribute("scaleY", &thisPreset._scale.y);
					thisPresetElmt->QueryAttribute("posX", &thisPreset._pos.x);
					thisPresetElmt->QueryAttribute("posY", &thisPreset._pos.y);
					thisPresetElmt->QueryAttribute("rot", &thisPreset._rot);

					_globalPresets.push_back(thisPreset);

					thisPresetElmt = thisPresetElmt->NextSiblingElement("Preset");
				}
			}

			auto hotkeys = root->FirstChildElement("hotkeys");
			if (!hotkeys)
			{
				_errorMessage = "Invalid hotkeys element in " + _loadingPath;
				logToFile(_appConfig, "Load Failed: " + _errorMessage);
				return;
			}

			_states.clear();

			auto thisHotkey = hotkeys->FirstChildElement("hotkey");
			while (thisHotkey)
			{
				_states.emplace_back(StatesInfo());
				StatesInfo& hkey = _states.back();

				thisHotkey->QueryAttribute("enabled", &hkey._enabled);

				if (const char* storedName = thisHotkey->Attribute("name"))
					hkey._name = storedName;

				int key = -1;
				thisHotkey->QueryAttribute("key", &key);
				hkey._key = (sf::Keyboard::Key)key;

				key = -1;
				thisHotkey->QueryAttribute("scancode", &key);
				hkey._scancode = (sf::Keyboard::Scan::Scancode)key;

				thisHotkey->QueryAttribute("ctrl", &hkey._ctrl);
				thisHotkey->QueryAttribute("shift", &hkey._shift);
				thisHotkey->QueryAttribute("alt", &hkey._alt);
				thisHotkey->QueryAttribute("timeout", &hkey._timeout);
				thisHotkey->QueryAttribute("useTimeout", &hkey._useTimeout);

				thisHotkey->QueryAttribute("axis", (int*)(&hkey._jAxis));
				thisHotkey->QueryAttribute("btn", &hkey._jButton);
				thisHotkey->QueryAttribute("jpadID", &hkey._jPadID);
				thisHotkey->QueryAttribute("axisDir", &hkey._jDir);

				thisHotkey->QueryAttribute("mouseButton", &hkey._mouseButton);

				if (hkey._scancode != -1 || hkey._key != -1 || (hkey._jPadID != -1 && (hkey._jButton != -1 || hkey._jAxis != -1)))
					hkey._mouseButton = -1;

				hkey._activeType = StatesInfo::ActiveType::Toggle;
				bool toggle = true;
				thisHotkey->QueryAttribute("toggle", &toggle);
				if (!toggle)
					hkey._activeType = StatesInfo::ActiveType::Held;

				thisHotkey->QueryIntAttribute("activeType", (int*)(&hkey._activeType));
				thisHotkey->QueryIntAttribute("canTrigger", (int*)(&hkey._canTrigger));
				thisHotkey->QueryAttribute("threshold", &hkey._threshold);

				thisHotkey->QueryAttribute("schedule", &hkey._schedule);
				thisHotkey->QueryAttribute("interval", &hkey._intervalTime);
				thisHotkey->QueryAttribute("variation", &hkey._intervalVariation);

				auto thisLayerState = thisHotkey->FirstChildElement("state");
				int stateIdx = 0;
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

					{
						std::scoped_lock lockState(_stateLocks[stateIdx]);
						hkey._layerStates[id] = (StatesInfo::State)state;
					}


					thisLayerState = thisLayerState->NextSiblingElement("state");
					++stateIdx;
				}

				thisHotkey = thisHotkey->NextSiblingElement("hotkey");
			}

			std::error_code ec = {};
			_fullLoadedXMLPath = fs::absolute(_loadingPath, ec).string();
			if (ec.value() != 0) logToFile(_appConfig, "LoadLayerSet: Failed to get absolute path for " + _loadingPath);
			_lastSavedLocation = _fullLoadedXMLPath;
			ec = {};
			_loadedXMLRelPath = fs::proximate(_fullLoadedXMLPath, _appConfig->_appLocation, ec).string();
			if (ec.value() != 0) logToFile(_appConfig, "LoadLayerSet: Failed to proximate path: " + _fullLoadedXMLPath);
			ec = {};
			_loadedXMLAbsDirectory = fs::absolute(_loadingPath).parent_path().string();
			if (ec.value() != 0) logToFile(_appConfig, "LoadLayerSet: Failed to get absolute path directory for  " + _loadingPath);

			ec = {};
			_loadedXMLRelDirectory = fs::proximate(_loadedXMLAbsDirectory, _appConfig->_appLocation).string();
			if (ec.value() != 0) logToFile(_appConfig, "LoadLayerSet: Failed to proximate path directory: " + _loadedXMLAbsDirectory);

			_layerSetName = fs::path(_loadingPath).filename().replace_extension("").string();

			UpdateWindowTitle();

			for (auto& layer : _layers)
			{
				layer.CalculateLayerDepth();
			}

			logToFile(_appConfig, "Loaded Layer Set " + _loadingPath);
			_loadingPath = "";

			_loadingFinished = true;

			if(_appConfig->_unloadTimeoutEnabled)
				SetUnloadingTimer(_appConfig->_unloadTimeout);
			else
				SetUnloadingTimer(0);

		});

	return true;
}

void LayerManager::SetUnloadingTimer(int timer)
{
	if (_loadingFinished)
	{
		for (auto& l : _layers)
		{
			l.SetUnloadingTimer(timer);
		}
	}
}

void LayerManager::LayerInfo::SetUnloadingTimer(int timer)
{
	int ltimer = _pinLoaded ? 0 : timer;

	_idleSprite->UnloadIfUnused(ltimer);
	_talkSprite->UnloadIfUnused(ltimer);
	_blinkSprite->UnloadIfUnused(ltimer);
	_talkBlinkSprite->UnloadIfUnused(ltimer);
	_screamSprite->UnloadIfUnused(ltimer);
}

void LayerManager::HandleHotkey(const sf::Event& evt, bool keyDown)
{
	for (auto& l : _layers)
		if (l._renamePopupOpen)
			return;

	sf::Keyboard::Key key = evt.key.code;
	sf::Keyboard::Scan::Scancode scancode = evt.key.scancode;
	bool ctrl = evt.key.control;
	bool shift = evt.key.shift;
	bool alt = evt.key.alt;
	int mButton = (int)evt.mouseButton.button;

	auto axis = evt.joystickMove.axis;
	float jDir = evt.joystickMove.position;
	int jButton = evt.joystickButton.button;
	int jPadID = -1;
	if (evt.type == evt.JoystickMoved)
		jPadID = evt.joystickMove.joystickId;
	if (evt.type == evt.JoystickButtonPressed || evt.type == evt.JoystickButtonReleased)
		jPadID = evt.joystickButton.joystickId;

	float talkFactor = 0;
	if (_lastTalkMax > 0)
	{
		talkFactor = _lastTalkLevel / _lastTalkMax;
		talkFactor = pow(talkFactor, 0.5);
	}

	for (int h = 0; h < _states.size(); h++)
	{
		auto& stateInfo = _states[h];

		bool canTrigger = stateInfo._enabled;
		if (canTrigger && stateInfo._canTrigger != StatesInfo::CanTrigger::TRIGGER_ALWAYS)
		{
			if (stateInfo._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_TALKING)
				canTrigger &= talkFactor >= stateInfo._threshold;
			if (stateInfo._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_IDLE)
				canTrigger &= talkFactor < stateInfo._threshold;
		}
		if (!canTrigger)
			continue;

		float timeout = 0.2;

		bool match = false;
		if ((evt.type == sf::Event::KeyPressed || evt.type == sf::Event::KeyReleased)
			&& (key != sf::Keyboard::Unknown && stateInfo._key == key) || (scancode != sf::Keyboard::Scan::Unknown && stateInfo._scancode == scancode)
			&& stateInfo._ctrl == ctrl && stateInfo._shift == shift && stateInfo._alt == alt)
			match = true;
		else if (evt.type == sf::Event::JoystickButtonPressed && stateInfo._jButton == jButton && stateInfo._jPadID == jPadID)
		{
			match = true;
			keyDown = true;
		}
		else if (evt.type == sf::Event::JoystickButtonReleased && stateInfo._jButton == jButton && stateInfo._jPadID == jPadID)
		{
			match = true;
			keyDown = false;
		}
		else if (ImGui::IsAnyItemHovered() == false && evt.type == sf::Event::MouseButtonPressed && stateInfo._mouseButton == mButton && stateInfo._mouseButton != -1)
		{
			match = true;
			keyDown = true;
		}
		else if (evt.type == sf::Event::MouseButtonReleased && stateInfo._mouseButton == mButton && stateInfo._mouseButton != -1)
		{
			match = true;
			keyDown = false;
		}
		else if (_statesIgnoreStick == false && evt.type == sf::Event::JoystickMoved && stateInfo._axisWasTriggered == false && stateInfo._jAxis == (int)axis && stateInfo._jPadID == jPadID)
		{
			if (keyDown && std::signbit(stateInfo._jDir) == std::signbit(jDir))
				match = true;
			else if (keyDown == false)
			{
				match = true;
				timeout = 0;
			}
		}

		if (stateInfo._activeType == StatesInfo::Held)
			timeout = 0;

		if (match && stateInfo._timer.getElapsedTime().asSeconds() > timeout)
		{
			if (evt.type == sf::Event::JoystickMoved)
				stateInfo._axisWasTriggered = true;

			if (stateInfo._active && ((stateInfo._activeType == StatesInfo::Toggle && keyDown) || (stateInfo._activeType == StatesInfo::Held && !keyDown)))
			{
				stateInfo._keyIsHeld = false;
				// deactivate
				stateInfo._active = false;
				RemoveStateFromOrder(&stateInfo);
				stateInfo._timer.restart();
			}
			else if (!stateInfo._active && keyDown)
			{
				if (stateInfo._activeType == StatesInfo::Permanent)
				{
					std::scoped_lock lockState(_stateLocks[h]);

					// activate immediately & alter the default states
					for (auto& state : stateInfo._layerStates)
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
					stateInfo._active = false;
				}
				else
				{
					if (stateInfo._activeType == StatesInfo::Held)
						stateInfo._keyIsHeld = true;

					// activate and add to stack
					SaveDefaultStates();

					{
						std::scoped_lock lockState(_stateLocks[h]);

						for (auto& state : stateInfo._layerStates)
						{
							LayerInfo* layer = GetLayer(state.first);
							if (layer && state.second != StatesInfo::NoChange)
							{
								layer->_visible = state.second;
							}
						}
					}

					AppendStateToOrder(&stateInfo);
					stateInfo._timer.restart();
					stateInfo._active = true;
					_statesTimer.restart();
				}
			}

			if (!_statesPassThrough && keyDown)
				break;
		}
		else
		{
			if (evt.type == sf::Event::JoystickMoved)
				stateInfo._axisWasTriggered = false;
		}
	}

	return;
}

void LayerManager::CheckHotkeys()
{
	if (_loadingFinished == false)
		return;

	for (auto& l : _layers)
		if (l._renamePopupOpen)
			return;

	float talkFactor = 0;
	if (_lastTalkMax > 0)
	{
		talkFactor = _lastTalkLevel / _lastTalkMax;
		talkFactor = pow(talkFactor, 0.5);
	}

	bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
	bool alt = sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt);
	bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);

	for (int h = 0; h < _states.size(); h++)
	{
		bool keyDown = false;
		float timeout = 0.2;
		bool changed = false;

		StatesInfo& stateInfo = _states[h];

		// Check websocket
		if (_appConfig->_listenHTTP && _appConfig->_webSocket != nullptr)
		{
			WebSocket::QueueItem qItem = _appConfig->_webSocket->QueueFront();

			bool match = qItem.stateId == std::to_string(h);
			match |= qItem.stateId == stateInfo._name;

			if (match)
			{
				if (qItem.activeState == 1)
				{
					keyDown = true;
					if (stateInfo._activeType == StatesInfo::Held && stateInfo._enabled)
					{
						stateInfo._alternateHeld = true;
					}
				}
				else
				{
					keyDown = false;
					if (stateInfo._activeType == StatesInfo::Held && stateInfo._enabled)
					{
						stateInfo._alternateHeld = false;
					}
				}

				if ((stateInfo._wasTriggered != keyDown) && stateInfo._enabled)
					changed = true;

				_appConfig->_webSocket->PopQueueFront();
			}
			else if (_appConfig->_webSocket->hasQueue())
			{
				bool anyMatched = false;
				// check if the id or name matches any state
				for (int checkState = 0; checkState < _states.size(); checkState++)
				{
					bool checkMatch = qItem.stateId == std::to_string(checkState);
					checkMatch |= qItem.stateId == _states[checkState]._name;

					if (checkMatch)
					{
						anyMatched = true;
						break;
					}
				}

				// pop this request if it matched no states
				if (!anyMatched)
					_appConfig->_webSocket->PopQueueFront();
			}

			if (stateInfo._activeType == StatesInfo::Held && stateInfo._alternateHeld)
			{
				keyDown = true;
			}
		}

		bool canTrigger = stateInfo._enabled;
		if (canTrigger && stateInfo._canTrigger != StatesInfo::CanTrigger::TRIGGER_ALWAYS)
		{
			if (stateInfo._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_TALKING)
				canTrigger &= talkFactor >= stateInfo._threshold;
			if (stateInfo._canTrigger == StatesInfo::CanTrigger::TRIGGER_WHILE_IDLE)
				canTrigger &= talkFactor < stateInfo._threshold;
		}
		if (!canTrigger)
			continue;

		bool codePressed = stateInfo._key != -1 && sf::Keyboard::isKeyPressed(stateInfo._key);
		bool scanPressed = stateInfo._scancode != -1 && sf::Keyboard::isKeyPressed(stateInfo._scancode);

		if ((codePressed || scanPressed)
			&& stateInfo._ctrl == ctrl && stateInfo._shift == shift && stateInfo._alt == alt)
		{
			if (stateInfo._wasTriggered == false)
				changed = true;
			keyDown = true;
		}
		else if (stateInfo._jPadID != -1 && stateInfo._jButton != -1 && GamePad::isButtonPressed(stateInfo._jPadID, stateInfo._jButton))
		{
			if (stateInfo._wasTriggered == false)
				changed = true;
			keyDown = true;
		}
		else if (stateInfo._mouseButton != -1 && sf::Mouse::isButtonPressed((sf::Mouse::Button)stateInfo._mouseButton))
		{
			if (ImGui::IsAnyItemHovered() == false)
			{
				if (stateInfo._wasTriggered == false)
					changed = true;
				keyDown = true;
			}
		}
		else if (stateInfo._jPadID != -1 && _statesIgnoreStick == false && stateInfo._jAxis != -1)
		{
			float jDir = GamePad::getAxisPosition(stateInfo._jPadID, (sf::Joystick::Axis)stateInfo._jAxis);
			if (Abs(jDir) > 30 && std::signbit(jDir) == std::signbit(stateInfo._jDir))
			{
				if (stateInfo._wasTriggered == false)
					changed = true;
				keyDown = true;
			}
		}

		if (stateInfo._wasTriggered == true && keyDown == false)
			changed = true;

		if (stateInfo._activeType == StatesInfo::Held)
			timeout = 0;

		stateInfo._wasTriggered = keyDown;

		if (changed && stateInfo._timer.getElapsedTime().asSeconds() > timeout)
		{
			if (stateInfo._active && ((stateInfo._activeType == StatesInfo::Toggle && keyDown) || (stateInfo._activeType == StatesInfo::Held && !keyDown)))
			{
				stateInfo._keyIsHeld = false;
				// deactivate
				stateInfo._active = false;
				RemoveStateFromOrder(&stateInfo);
				stateInfo._timer.restart();

				//BREAK here to force a new frame update before modifying any more states
				break;
			}
			else if (!stateInfo._active && keyDown)
			{
				if (stateInfo._activeType == StatesInfo::Permanent)
				{
					std::scoped_lock lockState(_stateLocks[h]);

					// activate immediately & alter the default states
					for (auto& state : stateInfo._layerStates)
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
					stateInfo._active = false;
				}
				else
				{
					if (stateInfo._activeType == StatesInfo::Held)
						stateInfo._keyIsHeld = true;

					// activate and add to stack
					SaveDefaultStates();

					{
						std::scoped_lock lockState(_stateLocks[h]);
						for (auto& state : stateInfo._layerStates)
						{
							LayerInfo* layer = GetLayer(state.first);
							if (layer && state.second != StatesInfo::NoChange)
							{
								layer->_visible = state.second;
							}
						}
					}

					AppendStateToOrder(&stateInfo);
					stateInfo._timer.restart();
					stateInfo._active = true;
					_statesTimer.restart();
				}
			}

			if (!_statesPassThrough && keyDown)
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
		if (_defaultLayerStates.count(l._id))
			l._visible = _defaultLayerStates[l._id];
	}
}

void LayerManager::Init(AppConfig* appConf, UIConfig* uiConf)
{
	_appConfig = appConf;
	_uiConfig = uiConf;
	if (_appConfig)
	{
		_textureMan = &_appConfig->_textureMan;
		_textureMan->LoadIcons(_appConfig->_appLocation);

		_resetIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_RESET);
		_emptyIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_EMPTY);
		_animIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_ANIM);
		_upIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_UP);
		_dnIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_DN);
		_editIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_EDIT);
		_delIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_DEL);
		_dupeIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_DUPE);
		_newFileIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_NEWFILE);
		_openFileIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_OPEN);
		_saveIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_SAVE);
		_saveAsIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_SAVEAS);
		_makePortableIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_MAKEPORTABLE);
		_reloadIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_RELOAD);
		_newLayerIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_NEWLAYER);
		_newFolderIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_NEWFOLDER);
		_statesIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_STATES);
		_plusIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_PLUS);

		_lockOpenIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_LOCK_OPEN);
		_lockClosedIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_LOCK_CLOSED);
		_eyeOpenIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_EYE_OPEN);
		_eyeClosedIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_EYE_CLOSED);

		_pinIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_PIN);
		_pinOffIcon = _appConfig->_textureMan.GetIcon(TextureManager::ICON_PIN_OFF);
	}
}

void LayerManager::DrawStatesGUI()
{
	float uiScale = _appConfig->scalingFactor;
	float windowSize = 400 * uiScale;

	if (_statesMenuOpen != _oldStatesMenuOpen)
	{
		_oldStatesMenuOpen = _statesMenuOpen;

		if (_statesMenuOpen)
		{
			if (_appConfig->_menuWindow.isOpen())
			{
				ImVec2 wSize = ImGui::GetWindowSize();

				ImGui::SetNextWindowPos({ wSize.x / 2 - windowSize / 2, wSize.y / 2 - windowSize / 2 });
			}
			else
			{
				ImGui::SetNextWindowPos({ _appConfig->_scrW / 2 - windowSize / 2, _appConfig->_scrH / 2 - windowSize / 2 });
			}
			ImGui::SetNextWindowSize({ windowSize, windowSize });
			ImGui::OpenPopup("States Setup");
		}
	}

	auto& style = ImGui::GetStyle();

	if (ImGui::BeginPopupModal("States Setup", &_statesMenuOpen))
	{

		ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		sf::Color btnColor = { sf::Uint8(255 * col.x), sf::Uint8(255 * col.y), sf::Uint8(255 * col.z) };

		ImGui::Columns(2, 0, false);
		ImGui::Checkbox("States pass through", &_statesPassThrough);
		ToolTip("When multiple states are defined with the same hotkey,\nonly the first will activate unless this is checked.", &_appConfig->_hoverTimer);
		ImGui::NextColumn();
		ImGui::Checkbox("Hide \"No Change\"", &_statesHideUnaffected);
		ToolTip("Hide all layers with \"No Change\" under\neach state effect", &_appConfig->_hoverTimer);
		ImGui::NextColumn();
		ImGui::Checkbox("Ignore joystick axis", &_statesIgnoreStick);
		ToolTip("Ignore events from joystick analog axis movement", &_appConfig->_hoverTimer);
		ImGui::Columns();

		if (ImGui::Button("Add"))
		{
			_states.push_back(StatesInfo());
			for (auto& l : _layers)
			{
				std::scoped_lock lockState(_stateLocks[_states.size()-1]);
				_states.back()._layerStates[l._id] = StatesInfo::NoChange;
			}
		}
		ToolTip("Add a new state", &_appConfig->_hoverTimer);

		int stateIdx = 0;
		while (stateIdx < _states.size())
		{
			auto& state = _states[stateIdx];

			std::string keyName = "";
			if (state._scancode != sf::Keyboard::Scan::Unknown)
			{
				if (g_scancode_names.count(state._scancode))
					keyName = g_scancode_names[state._scancode];
				else
					keyName = sf::Keyboard::getDescription(state._scancode);
			}
			else if (state._key != sf::Keyboard::Unknown)
				keyName = g_key_names[state._key];

			bool noKey = state._key == sf::Keyboard::Unknown && state._scancode == sf::Keyboard::Scan::Unknown;

			if (noKey)
			{
				if (state._awaitingHotkey)
					keyName = "---";
				else if (state._jButton != -1)
					keyName = "Joystick Btn " + std::to_string(state._jButton);
				else if (state._jDir != 0.f && state._jAxis != -1)
					keyName = "Joystick Axis " + g_axis_names[(sf::Joystick::Axis)state._jAxis] + ((state._jDir > 0) ? "+" : "-");
				else if (state._mouseButton != -1)
					keyName = "Mouse Btn " + std::to_string(state._mouseButton);
				else if (state._schedule)
					keyName = "";
				else
					keyName = "No hotkey";
			}
			if (state._alt)
				keyName = "Alt, " + keyName;
			if (state._shift)
				keyName = "Shift, " + keyName;
			if (state._ctrl)
				keyName = "Ctrl, " + keyName;

			std::string name;
			if (state._name == "")
			{
				// Generate a name from the actions
				name = keyName;

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
			}
			else
				name = state._name;

			float contentWidth = ImGui::GetWindowContentRegionMax().x;

			ImVec2 headerTxtPos = { ImGui::GetCursorPosX() + 6 * style.ItemSpacing.x, ImGui::GetCursorPosY() + 3 };
			ImVec2 delButtonPos = { ImGui::GetCursorPosX() + (contentWidth - 20 * style.ItemSpacing.x), ImGui::GetCursorPosY() };
			ImVec2 dupeButtonPos = { delButtonPos.x - 10 * style.ItemSpacing.x, delButtonPos.y };
			ImVec2 renameButtonPos = { dupeButtonPos.x - 10 * style.ItemSpacing.x, dupeButtonPos.y };
			ImVec2 enableButtonPos = { renameButtonPos.x - 10 * style.ItemSpacing.x, renameButtonPos.y };


			ImGui::PushID('id' + stateIdx); {

				auto disabledCol = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

				auto col = ImGui::GetStyleColorVec4(ImGuiCol_Header);
				if (state._active)
					col = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
				if (state._enabled == false)
					col = disabledCol;
				ImGui::PushStyleColor(ImGuiCol_Header, col);
				if (ImGui::CollapsingHeader("", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap))
				{

					ImGui::PopStyleColor();

					if (_appConfig->_listenHTTP)
					{
						DrawHTTPCopyHelpers(state, disabledCol, stateIdx);
					}


					ImGui::Columns(3, 0, false);
					ImGui::SetColumnWidth(0, style.ItemSpacing.x * 50);
					ImGui::SetColumnWidth(1, style.ItemSpacing.x * 33);
					ImGui::SetColumnWidth(2, style.ItemSpacing.x * 100);
					std::string btnName = keyName;
					if (noKey && state._jButton == -1 && state._jPadID == -1 && state._mouseButton == -1)
						btnName = " Click to\nrecord key";
					if (state._awaitingHotkey)
						btnName = "(press a key)";
					ImGui::PushID("recordKeyBtn"); {
						if (ImGui::Button(btnName.c_str(), { style.ItemSpacing.x * 43,style.ItemSpacing.x * 12 }) && !_waitingForHotkey)
						{
							_pendingKey = sf::Keyboard::Unknown;
							_pendingKeyScan = sf::Keyboard::Scan::Unknown;
							_pendingCtrl = false;
							_pendingShift = false;
							_pendingAlt = false;

							_pendingJAxis = sf::Joystick::Axis::X;
							_pendingJDir = 0.f;
							_pendingJButton = -1;
							_pendingJPadID = -1;

							_pendingMouseButton = -1;

							_waitingForHotkey = true;
							state._awaitingHotkey = true;
						}
						ToolTip("Set a hotkey", &_appConfig->_hoverTimer);

						if (ImGui::Button("Clear", { style.ItemSpacing.x * 43,style.ItemSpacing.x * 7 }))
						{
							state._jDir = 0.f;
							state._jButton = -1;
							state._key = sf::Keyboard::Unknown;
							state._jPadID = -1;

							state._ctrl = false;
							state._shift = false;
							state._alt = false;

							state._mouseButton = -1;

							_waitingForHotkey = false;
							state._awaitingHotkey = false;
						}
						ToolTip("Clear the hotkey", &_appConfig->_hoverTimer);

					}ImGui::PopID();

					if (state._awaitingHotkey && _waitingForHotkey)
					{
						state._ctrl = _pendingCtrl;
						state._shift = _pendingShift;
						state._alt = _pendingAlt;

						//reset them all first
						state._jDir = 0.f;
						state._jButton = -1;
						state._key = sf::Keyboard::Unknown;
						state._scancode = sf::Keyboard::Scan::Unknown;
						state._jAxis = -1;
						state._jPadID = -1;
						state._mouseButton = -1;

						bool set = false;
						if (_pendingMouseButton != -1)
						{
							state._mouseButton = _pendingMouseButton;
							set = true;
						}
						if (_pendingKeyScan != sf::Keyboard::Scan::Unknown)
						{
							state._scancode = _pendingKeyScan;
							set = true;
						}
						else if (_pendingKey != sf::Keyboard::Unknown)
						{
							state._key = _pendingKey;
							set = true;
						}
						else if (_pendingJDir != 0.f && _pendingJPadID != -1)
						{
							state._jDir = _pendingJDir;
							state._jAxis = _pendingJAxis;
							state._jPadID = _pendingJPadID;
							set = true;
						}
						else if (_pendingJButton != -1 && _pendingJPadID != -1)
						{
							state._jButton = _pendingJButton;
							state._jPadID = _pendingJPadID;
							set = true;
						}

						if (set)
						{
							_waitingForHotkey = false;
							state._awaitingHotkey = false;
						}
					}

					ImGui::NextColumn();

					ImGui::AlignTextToFramePadding();
					ImGui::Text("Active Type:");

					ImGui::AlignTextToFramePadding();
					ImGui::Text("Can Trigger:");

					if (state._canTrigger != StatesInfo::CanTrigger::TRIGGER_ALWAYS)
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Threshold:");
					}

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

					ImGui::PushItemWidth(100);
					if (ImGui::BeginCombo("##CanTrigger", g_canTriggerNames[state._canTrigger].c_str()))
					{
						for (int trigType = 0; trigType < g_canTriggerNames.size(); trigType++)
						{
							if (ImGui::Selectable(g_canTriggerNames[trigType].c_str(), state._canTrigger == trigType))
								state._canTrigger = (StatesInfo::CanTrigger)trigType;
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
					ToolTip("Change when the state can be triggered:\
\n- Always: Can always be triggered by the \n    schedule or the hotkey\
\n- While Talking: Can only be triggered above \n    the given voice threshold\
\n- While Idle: Can only be triggered below \n    the given voice threshold", &_appConfig->_hoverTimer);

					if (state._canTrigger != StatesInfo::CanTrigger::TRIGGER_ALWAYS)
					{
						FloatSliderDrag("##Threshold", &state._threshold, 0.0, 1.0, "%.3f", 0, _uiConfig->_numberEditType);
					}

					if (state._activeType != StatesInfo::Permanent)
					{
						ImGui::SetCursorPosY(timeoutpos);
						if (timeoutActive)
						{
							FloatSliderDrag("##timeoutSlider", &state._timeout, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic, _uiConfig->_numberEditType);
							ToolTip("How long the state stays active for", &_appConfig->_hoverTimer, true);
						}
					}

					ImGui::Columns();

					ImGui::Columns(2, 0, false);
					ImGui::SetColumnWidth(0, style.ItemSpacing.x * 50);
					ImGui::SetColumnWidth(1, windowSize);
					ImGui::NextColumn();
					if (state._schedule && state._activeType != StatesInfo::Permanent)
					{
						FloatSliderDrag("Interval", &state._intervalTime, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic, _uiConfig->_numberEditType);
						ToolTip("Sets how long the state will be inactive before reactivating", &_appConfig->_hoverTimer, true);
						FloatSliderDrag("Variation", &state._intervalVariation, 0.0, 30.0, "%.1f s", ImGuiSliderFlags_Logarithmic, _uiConfig->_numberEditType);
						ToolTip("Adds a random variation to the Interval.\nThis sets the maximum variation.", &_appConfig->_hoverTimer, true);
					}

					ImGui::Columns();

					ImGui::Separator();

					if (ImGui::BeginTable("hotkeystates", 4, ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, { 0.0f, 200.f * uiScale }))
					{
						ImGui::TableSetupColumn("Layer Name", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableSetupColumn("Show", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupColumn("No Change", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupScrollFreeze(0, 1);
						ImGui::TableHeadersRow();

						int layerIdx = 0;
						for (auto& l : _layers)
						{
							{
								std::scoped_lock lockState(_stateLocks[stateIdx]);
								if (state._layerStates.count(l._id) == 0)
									state._layerStates[l._id] = StatesInfo::NoChange;
							}

							if (_statesHideUnaffected && state._layerStates[l._id] == StatesInfo::NoChange)
								continue;

							bool customColor = l._layerColor != ImVec4(0, 0, 0, 0);
							if (customColor)
							{
								ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, l._layerColor);
								ImGui::PushStyleColor(ImGuiCol_TableRowBg, l._layerColor * 0.8);
							}

							ImGui::PushID(l._id.c_str()); {

								ImGui::TableNextColumn();

								ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_BorderShadow);

								//if (++layerIdx % 2)
								//	ImGui::DrawRectFilled(sf::FloatRect(0, 0, 400, 20), toSFColor(col));

								ImGui::AlignTextToFramePadding();
								ImGui::Text(ANSIToUTF8(l._name).c_str());

								{
									std::scoped_lock lockState(_stateLocks[stateIdx]);

									ImGui::TableNextColumn();
									ImGui::RadioButton("##Show", (int*)&state._layerStates[l._id], (int)StatesInfo::Show);
									ToolTip("Show this layer when the state is activated", &_appConfig->_hoverTimer);

									ImGui::TableNextColumn();
									ImGui::RadioButton("##Hide", (int*)&state._layerStates[l._id], (int)StatesInfo::Hide);
									ToolTip("Hide this layer when the state is activated", &_appConfig->_hoverTimer);

									ImGui::TableNextColumn();
									ImGui::RadioButton("##NoChange", (int*)&state._layerStates[l._id], (int)StatesInfo::NoChange);
									ToolTip("Do not affect this layer when the state is activated", &_appConfig->_hoverTimer);
								}

							}ImGui::PopID();

							ImGui::TableNextRow();

							if (customColor)
								ImGui::PopStyleColor(2);
						}

						ImGui::EndTable();
					}

				}
				else
					ImGui::PopStyleColor();

				ImVec2 endHeaderPos = ImGui::GetCursorPos();

				if (state._renaming)
				{
					headerTxtPos.y -= 3;
					ImGui::SetCursorPos(headerTxtPos);
					char inputStr[MAX_PATH] = {};
					ANSIToUTF8(state._name).copy(inputStr, MAX_PATH);
					ImGui::SetKeyboardFocusHere();
					if (ImGui::InputText("##rename", inputStr, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll))
					{
						state._name = UTF8ToANSI(inputStr);
					}
					if (ImGui::IsItemDeactivatedAfterEdit()
						|| (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered() == false))
					{
						state._renaming = false;
					}
				}
				else
				{
					if (_appConfig->_listenHTTP)
					{
						name = "[" + std::to_string(stateIdx) + "] " + name;
					}

					ImGui::SetCursorPos(headerTxtPos);
					ImGui::Text(ANSIToUTF8(name).c_str());
				}

				ImVec2 btnSize = { 17 * uiScale, 17 * uiScale };

				ImGui::SetCursorPos(delButtonPos);
				ImGuiStyle& style = ImGui::GetStyle();
				PushDeleteStyle();
				if (ImGui::Button("Delete"))
				{
					RemoveStateFromOrder(&state);
					_states.erase(_states.begin() + stateIdx);
				}
				PopDeleteStyle();
				ToolTip("Delete this state", &_appConfig->_hoverTimer);


				ImGui::SetCursorPos(enableButtonPos);
				ImGui::Checkbox("##enableState", &state._enabled);
				ToolTip("Enable this state to be triggered", &_appConfig->_hoverTimer);

				ImGui::SetCursorPos(dupeButtonPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });
				if (ImGui::ImageButton("dupeBtn", *_dupeIcon, toSFVector(btnSize), sf::Color::Transparent, btnColor))
				{
					_states.push_back(state);
					if(_states.back()._name != "")
						_states.back()._name += " copy";
				}
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(renameButtonPos);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });
				if (ImGui::ImageButton("renameBtn", *_editIcon, toSFVector(btnSize), sf::Color::Transparent, btnColor))
				{
					state._renaming = !state._renaming;
				}
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(endHeaderPos);

			}ImGui::PopID();

			if (stateIdx >= _states.size())
				break;

			stateIdx++;
		}

		ImGui::EndPopup();
	}
}

void LayerManager::PopDeleteStyle()
{
	ImGui::PopStyleColor(4);
}

ImVec4 LayerManager::PushDeleteStyle()
{
	ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
	ImGui::PushStyleColor(ImGuiCol_Text, { 255.f / 255,200.f / 255,170.f / 255, 1.f });

	return ImVec4(255.f / 255, 200.f / 255, 170.f / 255, 1.f);
}

void LayerManager::DrawHTTPCopyHelpers(LayerManager::StatesInfo& state, ImVec4& disabledCol, int stateIdx)
{
	ImGui::TextColored(disabledCol, state._webRequest.c_str());

	bool optionChanged = false;
	ImGui::PushID(&state); {
		if (ImGui::BeginTable("##urlCopy", 3, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("");
			ImGui::TableSetupColumn("");
			ImGui::TableSetupColumn("copybtn", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Copy request").x + ImGui::GetStyle().ItemSpacing.x * 2);

			ImGui::TableNextColumn();
			optionChanged = SwapButtons("", {
							{
								"Trigger",
								"Trigger the state when this request is called",
								1
							},
							{
								"Stop",
								"Stop the state when this request is called\n(Necessary only for 'While Held')",
								0
							},
				}, state._webRequestActive, &_appConfig->_hoverTimer, false);

			ImGui::TableNextColumn();
			optionChanged |= SwapButtons("", {
							{
								"Name",
								"Use the state's name in the request",
								1
							},
							{
								"ID",
								"Use the state's ID in the request",
								0
							},
				}, _copyStateNames, &_appConfig->_hoverTimer, false);

			ImGui::TableNextColumn();
			if (ImGui::Button("Copy request"))
			{
				ImGui::SetClipboardText(state._webRequest.c_str());
			}
			ImGui::EndTable();
		}
	}ImGui::PopID();

	if (state._webRequest == "" || optionChanged)
	{
		std::stringstream ss;
		if (_copyStateNames && state._name != "")
			ss << "http://127.0.0.1:" << _appConfig->_httpPort << "/state?[\"" << state._name << "\"," << state._webRequestActive << "]";
		else
			ss << "http://127.0.0.1:" << _appConfig->_httpPort << "/state?[" << stateIdx << "," << state._webRequestActive << "]";
		state._webRequest = ss.str();
		//ImGui::SetClipboardText(state._webRequest.c_str());
	}

	ImGui::PushStyleColor(ImGuiCol_Separator, disabledCol);
	ImGui::Separator();
	ImGui::PopStyleColor();
}

void LayerManager::LayerInfo::CalculateLayerDepth()
{
	if (_parent == nullptr)
		return;

	_lastCalculatedParents.clear();

	_lastCalculatedParents.insert(_lastCalculatedParents.begin(), this);

	int depth = 0;
	std::string& searchMotionParent = _motionParent;
	LayerInfo* mp = _parent->GetLayer(searchMotionParent);
	while (mp != nullptr)
	{
		_lastCalculatedParents.insert(_lastCalculatedParents.begin(), mp);

		depth++;
		mp = _parent->GetLayer(mp->_motionParent);
	}

	_lastCalculatedDepth = depth;
}

bool LayerManager::LayerInfo::EvaluateLayerVisibility()
{
	if (!_visible)
		return false;

	bool visible = _visible;
	

	if (_hideWithParent)
	{
		CalculateLayerDepth();

		for (int mpIdx = _lastCalculatedParents.size()-1; mpIdx > -1; mpIdx--)
		{
			auto& mp = _lastCalculatedParents[mpIdx];
			visible &= mp->_visible;

			if (!visible)
				return false;

			if (!mp->_hideWithParent)
				break;

		}
	}

	if (_inFolder != "")
	{
		LayerInfo* folder = _parent->GetLayer(_inFolder);
		if (folder)
			visible &= folder->_visible;
	}
	return visible;
}

void LayerManager::LayerInfo::DoIndividualMotion(bool talking, bool screaming, float talkAmount, double& rot, sf::Vector2<double>& motionScale, ImVec4& activeSpriteCol, sf::Vector2<double>& motionPos)
{
	float newMotionY = 0;
	float newMotionX = 0;

	int maxBounces = INT_MAX;

	switch (_bounceType)
	{
	case LayerManager::LayerInfo::BounceNone:
		break;
	case LayerManager::LayerInfo::BounceLoudness:
		_isBouncing = false;
		if (talking || screaming)
		{
			_isBouncing = true;

			newMotionX += _bounceMove.x * talkAmount;
			newMotionY += _bounceMove.y * talkAmount;
			rot += _bounceRotation * talkAmount;
			motionScale += sf::Vector2<double>(_bounceScale * talkAmount);
		}
		break;
	case LayerManager::LayerInfo::BounceOnce:
		maxBounces = 1;
	case LayerManager::LayerInfo::BounceRegular:
	{
		bool canStopBouncing = true;
		if ((talking || screaming) && _bounceFrequency > 0)
		{
			canStopBouncing = false;

			if (!_isBouncing)
			{
				_isBouncing = true;
				_bounceTimer.restart();
			}
		}

		if (_isBouncing)
		{
			float motionTime = _bounceTimer.getElapsedTime().asSeconds();
			int bounces = floor(motionTime / _bounceFrequency);

			// if can stop bouncing but we're still finishing a bounce, keep going
			if (bounces == _prevNumBounces && canStopBouncing)
				canStopBouncing = false;

			_prevNumBounces = bounces;

			if (bounces < maxBounces)
			{
				motionTime -= bounces * _bounceFrequency;
				float phase = (motionTime / _bounceFrequency) * 2.0 * PI;

				float bounceAmount = (-0.5 * cos(phase) + 0.5);
				newMotionX += _bounceMove.x * bounceAmount;
				newMotionY += _bounceMove.y * bounceAmount;
				rot += _bounceRotation * bounceAmount;
				motionScale += sf::Vector2<double>(_bounceScale * bounceAmount);
			}
		}

		if (canStopBouncing)
		{
			_prevNumBounces = 0;
			_isBouncing = false;
		}

		break;
	}
	default:
		break;
	}

	if (_idleMotionEnabled)
	{
		bool talkActive = (talking && _swapWhenTalking || _isBouncing) && !_breatheWhileTalking;

		if (!talkActive && _breathFrequency > 0)
		{
			if (!_isBreathing)
			{
				_motionTimer.restart();
				_isBreathing = true;
			}
			float motionTime = _motionTimer.getElapsedTime().asSeconds();

			float coolDownTime = _breathFrequency / 5;

			bool coolingDown = motionTime < coolDownTime && !_breatheWhileTalking;
			float coolDownFactor = Clamp((coolDownTime - motionTime) / coolDownTime, 0.0, 1.0);

			motionTime = fmod(motionTime, _breathFrequency);
			float phase = (motionTime / _breathFrequency) * 2.0 * PI;
			if (coolingDown)
			{
				_breathAmount.x = std::max(_breathAmount.x * coolDownFactor, (-0.5f * (float)cos(phase) + 0.5f) * 1.0f - coolDownFactor);
				if (_breathCircular)
					_breathAmount.y = std::max(_breathAmount.y * coolDownFactor, (-0.5f * (float)sin(phase) + 0.5f) * 1.0f - coolDownFactor);
				else
					_breathAmount.y = std::max(_breathAmount.y * coolDownFactor, (-0.5f * (float)cos(phase) + 0.5f) * 1.0f - coolDownFactor);
			}
			else
			{
				_breathAmount.x = (-0.5f * cos(phase) + 0.5f);
				if (_breathCircular)
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
		rot += _breathAmount.y * _breathRotation;

		if (_doBreathTint)
		{
			ImVec4 idleColAmount = activeSpriteCol * (1.0 - _breathAmount.y);
			ImVec4 breathColAmount = _breathTint * _breathAmount.y;
			activeSpriteCol = idleColAmount + breathColAmount;
		}

		motionScale = motionScale * sf::Vector2<double>(1.0, 1.0) + (double)_breathAmount.y * sf::Vector2<double>(_breathScale);
	}

	_motionY += (newMotionY - _motionY) * 0.3f;
	_motionX += (newMotionX - _motionX) * 0.3f;

	motionPos.x += _motionX;
	motionPos.y -= _motionY;
}

void LayerManager::LayerInfo::CalculateInheritedMotion(sf::Vector2<double>& motionScale, sf::Vector2<double>& motionPos, double& motionRot, double& motionParentRot, ImVec4& motionTint, sf::Vector2<double>& physicsPos, bool becameVisible, SpriteSheet* lastActiveSprite, float timeMult)
{
	LayerInfo* mp = _parent->GetLayer(_motionParent);
	if (mp)
	{
		float motionDelayNow = _motionDelay;
		if (motionDelayNow < 0)
			motionDelayNow = 0;

		double directParentRot = 0;
		sf::Vector2<double> directParentScale = {1,1};



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
				double fraction = framePosition / frameDuration;
				motionScale = mp->_motionLinkData[prev]._scale + fraction * (mp->_motionLinkData[next]._scale - mp->_motionLinkData[prev]._scale);
				motionPos = mp->_motionLinkData[prev]._pos + fraction * (mp->_motionLinkData[next]._pos - mp->_motionLinkData[prev]._pos);
				motionRot += mp->_motionLinkData[prev]._rot + fraction * (mp->_motionLinkData[next]._rot - mp->_motionLinkData[prev]._rot);
				motionTint = mp->_motionLinkData[prev]._tint + (mp->_motionLinkData[next]._tint - mp->_motionLinkData[prev]._tint) * fraction;

				motionParentRot += mp->_motionLinkData[prev]._parentRot + fraction * (mp->_motionLinkData[next]._parentRot - mp->_motionLinkData[prev]._parentRot);
				//motionParentPos += mp->_motionLinkData[prev]._parentPos + fraction * (mp->_motionLinkData[next]._parentPos - mp->_motionLinkData[prev]._parentPos);

			}
		}
		else if (mp->_motionLinkData.size() > 0)
		{
			directParentRot = mp->_motionLinkData[0]._rot;

			motionScale = mp->_motionLinkData[0]._scale;
			motionPos = mp->_motionLinkData[0]._pos;
			motionRot += directParentRot;
			motionTint = mp->_motionLinkData[0]._tint;

			motionParentRot += mp->_motionLinkData[0]._parentRot;
			//motionParentPos += mp->_motionLinkData[0]._parentPos;
		}

		
		directParentScale = motionScale;
		directParentScale -= {1, 1};
		directParentScale = Rotate(directParentScale, Deg2Rad(-mp->_rot));
		directParentScale += {1, 1};

		// transform this layer's position by the parent position
		sf::Vector2<double> originalOffset = sf::Vector2<double>(_pos) - sf::Vector2<double>(mp->_pos);
		sf::Vector2<double> originalOffsetRotated = Rotate(originalOffset, Deg2Rad(motionRot + motionParentRot));
		sf::Vector2<double> offsetScaled = { originalOffsetRotated.x * directParentScale.x, originalOffsetRotated.y * directParentScale.y };
		sf::Vector2<double> originMove = offsetScaled - originalOffset;

		motionPos += originMove;

		bool physics = (_motionDrag > 0 || _motionSpring > 0);
		physicsPos = motionPos;

		if (physics && becameVisible)
		{
			_lastAccel = { 0.f, 0.f };
			_physicsTimer.restart();
		}
		else if (physics && lastActiveSprite != nullptr && _motionLinkData.size() > 0)
		{
			double motionDrag = _motionDrag;
			double motionSpring = _motionSpring;
			double fadeIn = _physicsTimer.getElapsedTime().asSeconds() / 0.5;
			if (fadeIn < 1.0)
			{
				motionDrag = _motionDrag * fadeIn;
				motionSpring = _motionSpring * fadeIn;
			}

			const MotionLinkData& lastFrame = _motionLinkData.front();
			sf::Vector2<double> oldScale(lastFrame._scale.x, lastFrame._scale.y);
			sf::Vector2<double> oldPos(lastFrame._physicsPos.x, lastFrame._physicsPos.y);

			sf::Vector2<double> idealAccel = (sf::Vector2<double>(motionPos) - oldPos);
			sf::Vector2<double> accel = sf::Vector2<double>(_lastAccel) + (idealAccel - sf::Vector2<double>(_lastAccel)) * (1.0f - motionSpring);
			sf::Vector2<double> newPhysicsPos = oldPos + (1.0 - motionDrag) * accel * timeMult;
			_lastAccel = accel;

			sf::Vector2<double> offset = sf::Vector2<double>(motionPos) - newPhysicsPos;
			double movementDist = Length(offset);

			sf::Vector2<double> rotOffset = offset;
			if (!_parent->_appConfig->_undoRotationEffectFix)
				rotOffset.x *= -1;

			float totalRot = _rot + motionRot + motionParentRot;

			sf::Vector2<double> pivotDiff = sf::Vector2<double>(_pivot) - sf::Vector2<double>(.5f, .5f);
			if (_physicsIgnorePivots)
			{
				pivotDiff = sf::Vector2<double>(-_weightDirection);
			}
			pivotDiff = Rotate(pivotDiff, Deg2Rad(totalRot));

			float lenPivot = Length(pivotDiff);

			if (lenPivot > 0 && movementDist > 0 && _rotationEffect != 0)
			{
				float rotMult = Dot(rotOffset, pivotDiff);// / (movementDist * lenPivot);
				motionRot += _rotationEffect * rotMult;// *(movementDist * lenPivot);
			}

			if (_idleSprite && movementDist > 0)
			{
				totalRot = _rot + motionRot + motionParentRot;

				//use pivot direction only as a means of deciding the squash direction, not for weighting/strength
				sf::Vector2<double>pivotDirLocal = (sf::Vector2<double>(.5f, .5f) - sf::Vector2<double>(_pivot));
				if (_physicsIgnorePivots)
					pivotDirLocal = sf::Vector2<double>(_weightDirection);

				pivotDirLocal.x = pivotDirLocal.x == 0.0 ? 0.0 : 1.0 - 2.0 * (int)(pivotDirLocal.x < 0);
				pivotDirLocal.y = pivotDirLocal.y == 0.0 ? 0.0 : 1.0 - 2.0 * (int)(pivotDirLocal.y < 0);

				sf::Vector2<double> pivotStrength = sf::Vector2<double>(_motionStretchStrength.x * (pivotDirLocal.x == 0 ? 1 : pivotDirLocal.x), _motionStretchStrength.y * (pivotDirLocal.y == 0 ? 1 : pivotDirLocal.y));
				double pivotLen = Length(pivotDirLocal);
				sf::Vector2<double> pivotDir = pivotDirLocal / (pivotLen == 0 ? 1 : pivotLen);
				float angle = pivotLen == 0 ? 0 : atan2(pivotDir.y, pivotDir.x);

				// make the offset symmetrical if the corresponding pivot axis is 0
				if (pivotDirLocal.x == 0)
					offset.x = -Abs(offset.x);

				if (pivotDirLocal.y == 0)
					offset.y = -Abs(offset.y);

				//rotate all to match the sprite
				sf::Vector2<double> rotatedOffset = Rotate(offset, Deg2Rad(-totalRot));
				sf::Vector2<double> rotatedOffsetDir = rotatedOffset / movementDist;

				sf::Vector2<double> stretchFactor = pivotStrength * (movementDist / EllipseRadius(angle, _idleSprite->Size()));

				float xStretch = -rotatedOffsetDir.x * stretchFactor.x;
				float yStretch = -rotatedOffsetDir.y * stretchFactor.y;
				switch (_motionStretch)
				{
				case MS_PreserveVolume:
				{
					motionScale = motionScale * Clamp(sf::Vector2<double>(1.0 + xStretch - yStretch, 1.0 + yStretch - xStretch), sf::Vector2<double>(_stretchScaleMin), sf::Vector2<double>(_stretchScaleMax));
					break;
				}
				case MS_Linear:
					motionScale = motionScale * Clamp(sf::Vector2<double>(1.0 + xStretch, 1.0 + yStretch), sf::Vector2<double>(_stretchScaleMin), sf::Vector2<double>(_stretchScaleMax));
					break;
					//case MS_Circular:
					//	break;
				case MS_None:
				default:
					break;
				}
			}

			physicsPos = newPhysicsPos;

			if (_distanceLimit == 0.f)
			{
				newPhysicsPos = motionPos;
			}
			else if (_distanceLimit > 0.f && movementDist >= _distanceLimit)
			{
				sf::Vector2<double> offsetDir = offset / movementDist;
				newPhysicsPos = motionPos - offsetDir * _distanceLimit;
			}

			motionPos = newPhysicsPos;
		}
	}
}

void LayerManager::LayerInfo::DoConstantMotion(sf::Time& frameTime, sf::Vector2<double>& mpScale, sf::Vector2<double>& mpPos, double& mpRot)
{
	_storedConstantRot += _constantRot * frameTime.asSeconds();

	mpRot += _storedConstantRot;
}

void LayerManager::LayerInfo::CalculateDraw(float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	sf::Time frameTime = _frameTimer.restart();
	float fps = 1.0 / frameTime.asSeconds();

	float timeMult = 1.0;
	if (fps > 60)
	{
		timeMult = 60.0 / fps;
	}

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

	ImVec4 activeSpriteCol = _idleTint;

	float talkFactor = 0;
	if (talkMax > 0)
	{
		talkFactor = talkLevel / talkMax;
		talkFactor = pow(talkFactor, 0.5);

		if (_smoothTalkFactor && _smoothTalkFactorSize > 0)
		{
			_talkRunningAverage -= _talkRunningAverage / _smoothTalkFactorSize;
			_talkRunningAverage += talkFactor / _smoothTalkFactorSize;

			talkFactor = _talkRunningAverage;
		}

		_lastTalkFactor = talkFactor;
	}

	float talkAmount = Clamp(std::fmax(0.f, (talkFactor - _talkThreshold) / (1.0f - _talkThreshold)), 0.0, 1.0);

	bool reallyVisible = _visible;
	if (_hideWithParent)
	{
		LayerInfo* mp = _parent->GetLayer(_motionParent);
		if (mp)
			reallyVisible &= mp->_visible;
	}

	bool becameVisible = (reallyVisible == true) && (_oldVisible == false);

	if (becameVisible && _restartAnimsOnVisible)
	{
		if (_talkBlinkSprite)
			_talkBlinkSprite->Restart();
		if (_blinkSprite)
			_blinkSprite->Restart();
		if (_talkSprite)
			_talkSprite->Restart();
		if (_idleSprite)
			_idleSprite->Restart();
		if (_screamSprite)
			_screamSprite->Restart();

		_motionTimer.restart();
	}
	_oldVisible = reallyVisible;

	bool screaming = _scream && talkFactor > _screamThreshold;

	if (_screamTimer.getElapsedTime().asSeconds() < _minScreamTime)
		screaming = true;

	bool talking = !screaming && talkFactor > _talkThreshold;
	DetermineVisibleSprites(talking, screaming, activeSpriteCol, talkAmount);

	_wasTalking = talking;

	sf::Vector2<double>  motionScale = { 1.0,1.0 };
	sf::Vector2<double>  motionPos = { 0, 0 };
	sf::Vector2<double>  physicsPos = motionPos;
	double motionRot = 0;
	double motionParentRot = 0;
	ImVec4 mpTint;

	sf::Vector2<double> centerToPivot = _pivot - sf::Vector2<double>(0.5, 0.5);

	sf::Vector2<double> pivot = { 0.5*_activeSprite->Size().x + centerToPivot.x * _idleSprite->Size().x, 0.5 * _activeSprite->Size().y + centerToPivot.y * _idleSprite->Size().y };

	bool hasParent = !(_motionParent == "" || _motionParent == "-1");
	if (hasParent)
		CalculateInheritedMotion(motionScale, motionPos, motionRot, motionParentRot, mpTint, physicsPos, becameVisible, lastActiveSprite, timeMult);

	if (_inheritTint)
		activeSpriteCol = mpTint;

	if (!hasParent || _allowIndividualMotion)
		DoIndividualMotion(talking, screaming, talkAmount, motionRot, motionScale, activeSpriteCol, motionPos);

	DoConstantMotion(frameTime, motionScale, motionPos, motionRot);

	AddTrackingMovement(motionPos, motionRot, motionScale);

	if (screaming)
	{
		if (!_isScreaming)
		{
			_screamTimer.restart();
			_isScreaming = true;
		}

		if (_screamVibrate)
		{
			float motionTime = _screamTimer.getElapsedTime().asSeconds();
			motionPos.y += sin(motionTime / 0.02 * _screamVibrateSpeed) * _screamVibrateAmount;
			motionPos.x += sin(motionTime / 0.05 * _screamVibrateSpeed) * _screamVibrateAmount;
		}
	}
	else
	{
		_isScreaming = false;
	}

	MotionLinkData thisFrame;
	thisFrame._frameTime = frameTime;
	thisFrame._pos = motionPos;
	thisFrame._physicsPos = physicsPos;
	thisFrame._scale = motionScale;
	thisFrame._rot = motionRot;
	thisFrame._parentRot = motionParentRot + (_passRotationToChildLayers ? _rot : 0.0);
	thisFrame._tint = activeSpriteCol;

	_motionLinkData.push_front(thisFrame);

	sf::Time totalMotionStoredTime;
	for (auto& frame : _motionLinkData)
		totalMotionStoredTime += frame._frameTime;

	while (totalMotionStoredTime > sf::seconds(1.1) && _motionLinkData.size() > 0)
	{
		totalMotionStoredTime -= _motionLinkData.back()._frameTime;
		_motionLinkData.pop_back();
	}

	motionRot += _rot + motionParentRot;
	motionPos += sf::Vector2<double>(_pos);
	motionScale = sf::Vector2<double>(_scale) * motionScale;

	_activeSprite->setOrigin(sf::Vector2f(pivot));
	_activeSprite->setScale(sf::Vector2f(motionScale));
	_activeSprite->setPosition(sf::Vector2f( windowWidth / 2 + motionPos.x, windowHeight / 2 + motionPos.y ));
	_activeSprite->setRotation(motionRot);
	_activeSprite->SetColor(activeSpriteCol);

}

void LayerManager::LayerInfo::DetermineVisibleSprites(bool talking, bool screaming, ImVec4& activeSpriteCol, float& talkAmount)
{

	bool blinkAvailable = _blinkSprite->HasTexture() && !talking && !screaming;
	bool talkBlinkAvailable = _blinkWhileTalking && _talkBlinkSprite->HasTexture() && talking && !screaming;

	bool canStartBlinking = (talkBlinkAvailable || blinkAvailable) && !_isBlinking && _useBlinkFrame;

	if (canStartBlinking && _blinkTimer.getElapsedTime().asSeconds() > _blinkDelay + _blinkVarDelay)
	{
		_isBlinking = true;
		_blinkTimer.restart();
		if (!_blinkSprite->IsSynced())
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
			activeSpriteCol = _talkBlinkTint;
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
			activeSpriteCol = _blinkTint;
		}

		if (_blinkTimer.getElapsedTime().asSeconds() > _blinkDuration)
			_isBlinking = false;
	}

	if (_talkSprite->HasTexture() && !_isBlinking && _swapWhenTalking && talking)
	{
		_activeSprite = _talkSprite.get();
		_screamSprite->_visible = false;
		_idleSprite->_visible = false;
		_blinkSprite->_visible = false;
		_talkSprite->_visible = true;
		_talkSprite->SetColor(_talkTint);
		activeSpriteCol = _talkTint;

		if (_smoothTalkTint)
		{
			activeSpriteCol = _talkTint * talkAmount + _idleTint * (1.0 - talkAmount);
		}

		if (!_wasTalking && _restartTalkAnim)
		{
			_talkSprite->Restart();
		}
	}
	else if (_screamSprite->HasTexture() && screaming)
	{
		_activeSprite = _screamSprite.get();
		_screamSprite->_visible = true;
		_idleSprite->_visible = false;
		_blinkSprite->_visible = false;
		_talkSprite->_visible = false;
		_screamSprite->SetColor(_screamTint);
		activeSpriteCol = _screamTint;
	}
}

void LayerManager::LayerInfo::AddTrackingMovement(sf::Vector2<double>& mpPos, double& mpRot, sf::Vector2<double>& mpScale)
{
	bool doTracking = _trackingEnabled && (_trackingType != TRACKING_NONE);
	if (!doTracking)
		return;

	sf::Vector2f newTrackingAmount = {0, 0};

	float mouseEffect = 1.f;
	float axisEffect = 1.f;
	if (_trackingType == TRACKING_BOTH)
	{
		mouseEffect = _mouseEffect;
		axisEffect = _joypadEffect;
	}

	bool visible = EvaluateLayerVisibility();
	if (visible || !_trackingOffWhenHidden)
	{
		if ((_trackingType & TRACKING_MOUSE) && _parent->_appConfig->_mouseTrackingEnabled)
		{
			sf::Vector2f mousePos = (sf::Vector2f)sf::Mouse::getPosition();
			sf::Vector2f mouseMove = (mousePos - _mouseNeutralPos);

			const sf::Vector2f mouseMult = Clamp(mouseMove / _mouseAreaSize, -1.f, 1.f);

			newTrackingAmount += mouseMult*mouseEffect;
		}

		if ((_trackingType & TRACKING_CONTROLLER) && _parent->_appConfig->_controllerTrackingEnabled)
		{
			if (_trackingJoystick == -1)
			{
				if (_trackingSelect == TRACKINGSELECT_FIRST)
				{
					for (int jStick = 0; jStick < sf::Joystick::Count; jStick++)
					{
						if (sf::Joystick::isConnected(jStick))
						{
							_trackingJoystick = jStick;
							break;
						}
					}
				}			
			}

			if (_trackingJoystick != -1 && sf::Joystick::isConnected(_trackingJoystick))
			{
				sf::Vector2f axisPos;
				switch (_trackingAxis)
				{
				case AXIS_XY:
					axisPos.x = 0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::X);
					axisPos.y = 0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::Y);
					break;
				case AXIS_UV:
					axisPos.x = 0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::U);
					axisPos.y = 0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::V);
					break;
				case AXIS_POV:
					axisPos.x = 0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::PovX);
					axisPos.y = -0.01f * GamePad::getAxisPosition(_trackingJoystick, sf::Joystick::Axis::PovY);
					break;
				}

				const sf::Vector2f deadspot(_axisDeadzone, _axisDeadzone);
				const sf::Vector2f signAxis(axisPos.x >= 0 ? 1.f : -1.f, axisPos.y >= 0 ? 1.f : -1.f);
				float axisLength = Length(axisPos);
				const sf::Vector2f axisDir = axisLength > 0 ? axisPos / axisLength : sf::Vector2f(0.f, 0.f);
				sf::Vector2f axisAmount;
				axisAmount.x = (Clamp(Abs(axisPos.x) - _axisDeadzone * Abs(axisDir.x), 0.f, 1.f) / (1.f - _axisDeadzone * Abs(axisDir.x))) * signAxis.x;
				axisAmount.y = (Clamp(Abs(axisPos.y) - _axisDeadzone * Abs(axisDir.y), 0.f, 1.f) / (1.f - _axisDeadzone * Abs(axisDir.y))) * signAxis.y;

				newTrackingAmount = Clamp(newTrackingAmount + axisAmount * axisEffect, -1.f, 1.f);
			}
		}
		
	}

	float smooth = 1 + _trackingSmooth * 10;
	_trackingAmount -= _trackingAmount / smooth;
	_trackingAmount += newTrackingAmount / smooth;

	if (_followElliptical)
	{
		// limit the vector length to 1
		float moveLength = Length(_trackingAmount);
		if(moveLength > 1.0)
			_trackingAmount = _trackingAmount / moveLength;
	}

	sf::Vector2<double> newTrackingPos = sf::Vector2<double>(_trackingAmount) * sf::Vector2<double>(_trackingMoveLimits);
	sf::Vector2<double> newTrackingRot = sf::Vector2<double>(_trackingAmount) * sf::Vector2<double>(_trackingRotation);

	auto trackingAmountScale = _trackingAmount;

	if (_trackingScaleMode >= TRACKINGSCALE_SIMPLE_BOTTOMLEFT)
	{
		// remap track amount to range 0,1 from -1,1

		trackingAmountScale = (_trackingAmount + sf::Vector2f(1.f, 1.f)) * 0.5f;
		trackingAmountScale.y = 1.0 - trackingAmountScale.y;
	}
	else
	{
		trackingAmountScale.y *= -1;

		if (_trackingScaleAbsolute == 1)
			trackingAmountScale = Abs(trackingAmountScale);
	}

	sf::Vector2<double> newTrackingScaleY = sf::Vector2<double>(trackingAmountScale.y, trackingAmountScale.y) * sf::Vector2<double>(_trackingScaleVertical);
	sf::Vector2<double> newTrackingScaleX = sf::Vector2<double>(trackingAmountScale.x, trackingAmountScale.x) * sf::Vector2<double>(_trackingScaleHorizontal);

	sf::Vector2<double> newTrackingScale = newTrackingScaleX + newTrackingScaleY;
	if(_clampTrackingScale)
		newTrackingScale = Clamp(newTrackingScale, sf::Vector2<double>(_trackingScaleClamp.x, _trackingScaleClamp.x), sf::Vector2<double>(_trackingScaleClamp.y, _trackingScaleClamp.y));

	mpPos += newTrackingPos;
	mpRot = mpRot + newTrackingRot.x + newTrackingRot.y;
	mpScale = mpScale + newTrackingScale;
}

bool LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{
	float uiScale = _parent->_appConfig->scalingFactor;

	float UIUnit = ImGui::GetFrameHeight();

	ImVec4 col = style.Colors[ImGuiCol_Text];
	sf::Color btnColor = toSFColor(col);

	bool allowContinue = true;

	std::vector<int> toRemove;
	for (int l = 0; l < _folderContents.size(); l++)
	{
		auto* layer = _parent->GetLayer(_folderContents[l]);
		if (layer != nullptr)
		{
			layer->_lastHeaderPos = { -1,-1 };
			layer->_lastHeaderScreenPos = { -1,-1 };
			layer->_lastHeaderSize = { 0,0 };
		}
		else
			toRemove.insert(toRemove.begin(), l);
	}
	for (int rem : toRemove)
	{
		_folderContents.erase(_folderContents.begin() + rem);
	}


	ImGui::PushID((_id).c_str()); {

#ifdef DEBUG
		std::string name = "[" + std::to_string(layerID) + "] " + _name;
#else
		std::string name = _name;
#endif
		if (_isFolder)
			name = "[" + _name + "]";

		name += "##0";

		sf::Vector2f headerBtnSize(UIUnit - 2, UIUnit - 2);
		ImVec2 headerButtonsPos = { ImGui::GetWindowWidth() - UIUnit * 7, ImGui::GetCursorPosY() };

		float indentSize = 8 * uiScale;

		_lastHeaderScreenPos = toSFVector(ImGui::GetCursorScreenPos());
		_lastHeaderPos = toSFVector(ImGui::GetCursorPos());
		_lastHeaderSize = sf::Vector2f(ImGui::GetContentRegionAvail().x - 8 * uiScale, ImGui::GetFrameHeight());

		if (_isFolder)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
		}

		if (_scrollToHere)
		{
			ImGui::SetNextItemOpen(true);
		}

		if (_layerColor.w != 0)
			ImGui::PushStyleColor(ImGuiCol_Header, _layerColor);
		ImGui::SetNextItemStorageID(ImGui::GetID(_id.c_str()));
		if (ImGui::CollapsingHeader(ANSIToUTF8(name).c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap))
		{
			if (_scrollToHere)
			{
				_scrollToHere = false;
				if (!_isFolder)
					ImGui::SetScrollHereY();
			}

			if (_layerColor.w != 0)
				ImGui::PopStyleColor();

			ImGui::Indent(indentSize);

			if (_isFolder)
			{
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();

				for (int l = 0; l < _folderContents.size(); l++)
				{
					int layerIdx = 0;
					auto* layer = _parent->GetLayer(_folderContents[l], &layerIdx);
					if (layer != nullptr)
						layer->DrawGUI(style, layerIdx);
				}
			}
			else
			{
				_parent->_hoveredLayers.push_back(_id);

				float imgBtnWidth = UIUnit * 6 - 2;
				float smlImageBtnWidth = UIUnit * 3 - 2;
				float animBtnWidth = UIUnit;

				if (ImGui::BeginTable("imagebuttons", 3, ImGuiTableFlags_SizingFixedSame))
				{
					ImGui::TableNextColumn();

					ImGui::AlignTextToFramePadding();
					ImGui::Text("Idle");
					ImGui::PushID("idleimport"); {

						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

						ImageBrowsePreviewBtn(_importIdleOpen, "idleimgbtn", imgBtnWidth, _idleImagePath, _idleSprite.get());

						ImGui::SameLine();
						_spriteIdleOpen |= ImGui::ImageButton("idleanimbtn", *_animIcon, sf::Vector2f(animBtnWidth, animBtnWidth), sf::Color::Transparent, btnColor);
						ToolTip("Animation settings", &_parent->_appConfig->_hoverTimer);
						AnimPopup(*_idleSprite, _spriteIdleOpen, _oldSpriteIdleOpen);

						ImGui::PopStyleVar();//ImGuiStyleVar_FramePadding

						ImGui::PushID("idleimportfile"); {
							char idlebuf[MAX_PATH] = {};
							ANSIToUTF8(_idleImagePath).copy(idlebuf, MAX_PATH);
							ImGui::SetNextItemWidth(-1);
							if (ImGui::InputText("", idlebuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft))
							{
								_idleImagePath = UTF8ToANSI(idlebuf);
								if (_idleImagePath == "")
									_idleSprite->Clear();
								else
								{
									_idleSprite->LoadFromTexture(_parent->_textureMan, _idleImagePath, 1, 1, 1, 1, { -1,-1 }, &_parent->_errorMessage);
									_idleSprite->setSmooth(_scaleFiltering);
								}

							}
						}ImGui::PopID();
						ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

						ImGui::ColorEdit4("Tint", &_idleTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);

					}ImGui::PopID();

					ImGui::TableNextColumn();

					if (_swapWhenTalking)
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("Talk");
						ImGui::PushID("talkimport"); {
							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

							ImageBrowsePreviewBtn(_importTalkOpen, "talkimgbtn", imgBtnWidth, _talkSpritePath, _talkSprite.get());

							ImGui::SameLine();
							ImGui::PushID("talkanimbtn"); {
								_spriteTalkOpen |= ImGui::ImageButton("talkanimbtn", *_animIcon, sf::Vector2f(animBtnWidth, animBtnWidth), sf::Color::Transparent, btnColor);
								ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
								AnimPopup(*_talkSprite, _spriteTalkOpen, _oldSpriteTalkOpen);
							}ImGui::PopID();

							ImGui::PopStyleVar();//ImGuiStyleVar_FramePadding

							ImGui::PushID("talkimportfile"); {
								char talkbuf[MAX_PATH] = {};
								ANSIToUTF8(_talkSpritePath).copy(talkbuf, MAX_PATH);
								ImGui::SetNextItemWidth(-1);
								if (ImGui::InputText("", talkbuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft))
								{
									_talkSpritePath = UTF8ToANSI(talkbuf);
									if (_talkSpritePath == "")
										_talkSprite->Clear();
									else
									{
										_talkSprite->LoadFromTexture(_parent->_textureMan, _talkSpritePath, 1, 1, 1, 1, { -1,-1 }, &_parent->_errorMessage);
										_talkSprite->setSmooth(_scaleFiltering);
									}
								}
							}ImGui::PopID();
							ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

							ImGui::ColorEdit4("Tint", &_talkTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);

						}ImGui::PopID();
					}

					ImGui::TableNextColumn();

					if (_useBlinkFrame)
					{
						float blinkBtnSize = _blinkWhileTalking ? smlImageBtnWidth : imgBtnWidth;

						ImGui::AlignTextToFramePadding();
						ImGui::Text("Blink");
						ImGui::PushID("blinkimport"); {
							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

							ImageBrowsePreviewBtn(_importBlinkOpen, "blinkimgbtn", blinkBtnSize, _blinkSpritePath, _blinkSprite.get());

							ImGui::SameLine();
							auto tintPos = ImGui::GetCursorPos();
							ImGui::PushID("blinkanimbtn"); {
								_spriteBlinkOpen |= ImGui::ImageButton("blinkanimbtn", *_animIcon, sf::Vector2f(animBtnWidth, animBtnWidth), sf::Color::Transparent, btnColor);
								tintPos.y += ImGui::GetItemRectSize().y;
								ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
								AnimPopup(*_blinkSprite, _spriteBlinkOpen, _oldSpriteBlinkOpen);
							}ImGui::PopID();
							ImGui::PopStyleVar();//ImGuiStyleVar_FramePadding

							ImGui::PushID("blinkimportfile"); {
								char blinkbuf[MAX_PATH] = {};
								ANSIToUTF8(_blinkSpritePath).copy(blinkbuf, MAX_PATH);
								ImGui::SetNextItemWidth(-1);
								if (ImGui::InputText("", blinkbuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft))
								{
									_blinkSpritePath = UTF8ToANSI(blinkbuf);
									if (_blinkSpritePath == "")
										_blinkSprite->Clear();
									else
									{
										_blinkSprite->LoadFromTexture(_parent->_textureMan, _blinkSpritePath, 1, 1, 1, 1, { -1,-1 }, &_parent->_errorMessage);
										_blinkSprite->setSmooth(_scaleFiltering);
									}
								}
							}ImGui::PopID();
							ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

							auto saveCursor = ImGui::GetCursorPos();
							if (_blinkWhileTalking)
								ImGui::SetCursorPos(tintPos);

							ImGui::ColorEdit4("Tint", &_blinkTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
							ImGui::SetCursorPos(saveCursor);
						}ImGui::PopID();

						if (_blinkWhileTalking)
						{
							//ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk Blink");
							ImGui::PushID("talkblinkimport"); {
								ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

								ImageBrowsePreviewBtn(_importTalkBlinkOpen, "talkblinkimgbtn", blinkBtnSize, _talkBlinkSpritePath, _talkBlinkSprite.get());

								ImGui::SameLine();
								auto tintPos = ImGui::GetCursorPos();
								ImGui::PushID("talkblinkanimbtn"); {
									_spriteTalkBlinkOpen |= ImGui::ImageButton("talkblinkanimbtn", *_animIcon, sf::Vector2f(animBtnWidth, animBtnWidth), sf::Color::Transparent, btnColor);
									tintPos.y += ImGui::GetItemRectSize().y;
									ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
									AnimPopup(*_talkBlinkSprite, _spriteTalkBlinkOpen, _oldSpriteTalkBlinkOpen);
								}ImGui::PopID();//talkblinkanimbtn

								ImGui::PopStyleVar();//ImGuiStyleVar_FramePadding

								ImGui::PushID("talkblinkimportfile"); {
									char talkblinkbuf[MAX_PATH] = {};
									ANSIToUTF8(_talkBlinkSpritePath).copy(talkblinkbuf, MAX_PATH);
									ImGui::SetNextItemWidth(-1);
									if (ImGui::InputText("", talkblinkbuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_ElideLeft))
									{
										_talkBlinkSpritePath = UTF8ToANSI(talkblinkbuf);
										if (_talkBlinkSpritePath == "")
											_talkBlinkSprite->Clear();
										else
										{
											_talkBlinkSprite->LoadFromTexture(_parent->_textureMan, _talkBlinkSpritePath, 1, 1, 1, 1, { -1,-1 }, &_parent->_errorMessage);
											_talkBlinkSprite->setSmooth(_scaleFiltering);
										}
									}
								}ImGui::PopID();//talkblinkimportfile
								ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

								auto saveCursor = ImGui::GetCursorPos();
								if (_blinkWhileTalking)
									ImGui::SetCursorPos(tintPos);

								ImGui::ColorEdit4("Tint", &_talkBlinkTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
								ImGui::SetCursorPos(saveCursor);
							}ImGui::PopID();
						}
					}

					ImGui::EndTable();
				}

				ImGui::Checkbox("Restart anims on becoming visible", &_restartAnimsOnVisible);
				ToolTip("Restarts all this layer's sprite-sheets whenever\nthis layer is made visible", &_parent->_appConfig->_hoverTimer);

				if (LesserCollapsingHeader("Compositing..."))
				{
					BetterIndent(indentSize, _id + "comp");
					{
						bool scaleFilterChanged = SwapButtons("Scale Filter", {
							{
								"Linear",
								"Smooth (linear) interpolation when the image is not actual size",
								1
							},
							{
								"Nearest Pixel",
								"Nearest-neighbour interpolation, sharp pixels at any size",
								0
							},
							}, _scaleFiltering, &_parent->_appConfig->_hoverTimer);
						if (scaleFilterChanged)
						{
							if (_idleSprite) _idleSprite->setSmooth(_scaleFiltering);
							if (_talkSprite) _talkSprite->setSmooth(_scaleFiltering);
							if (_blinkSprite) _blinkSprite->setSmooth(_scaleFiltering);
							if (_talkBlinkSprite) _talkBlinkSprite->setSmooth(_scaleFiltering);
							if (_screamSprite) _screamSprite->setSmooth(_scaleFiltering);
						}

						AddResetButton("alphaCutoff", _alphaClip, 0.0f, _parent->_appConfig, &style);
						FloatSliderDrag("Alpha Cutoff", &_alphaClip, 0.0, 1.0, "%.3f", ImGuiSliderFlags_ClampOnInput, _parent->_uiConfig->_numberEditType);
						ToolTip("Define the minimum alpha (transparency) needed for visibility.\nValues below this will be fully transparent.\nUseful for removing unwanted soft edges from the Linear filter.\nSet to 0.0 to use the default from Advanced Settings.", &_parent->_appConfig->_hoverTimer);

						sf::BlendMode oldBlendMode = _blendMode;
						std::string bmName = "";
						for (auto& bm : g_blendmodes)
							if (bm.second == oldBlendMode)
								bmName = bm.first;
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
						ToolTip("Define how this layer's color should be\nblended onto the layers below it", &_parent->_appConfig->_hoverTimer);

						LayerInfo* oldClip = _parent->GetLayer(_clipID);
						std::string clipName = oldClip ? oldClip->_name : "Off";
						if (ImGui::BeginCombo("Clip Layer", ANSIToUTF8(clipName).c_str()))
						{
							float comboW = ImGui::GetContentRegionAvail().x;

							ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, uiScale });

							if (ImGui::Selectable("Off", (_clipID == "")))
								_clipID = "";
							for (auto& layer : _parent->GetLayers())
							{
								if (layer._id != _id && layer._clipID != _id && layer._isFolder == false)
								{
									bool customColor = layer._layerColor != ImVec4(0, 0, 0, 0);
									if (customColor)
										ImGui::PushStyleColor(ImGuiCol_Button, layer._layerColor);

									bool clicked = false;
									if (_clipID == layer._id)
										clicked = ImGui::Button(ANSIToUTF8(layer._name).c_str(), { comboW, UIUnit });
									else
										clicked = LesserButton(ANSIToUTF8(layer._name).c_str(), { comboW, UIUnit }, false);

									if (customColor)
										ImGui::PopStyleColor();

									if (clicked)
									{
										_clipID = layer._id;
										ImGui::CloseCurrentPopup();
									}
								}
							}
							ImGui::PopStyleVar(2);
							ImGui::EndCombo();
						}
						ToolTip("Clip this layer to the transparency\nof any other layer", &_parent->_appConfig->_hoverTimer);

					}
					BetterUnindent(_id + "comp");
				}


				ImGui::Separator();

				AddResetButton("talkThresh", _talkThreshold, 0.15f, _parent->_appConfig, &style, true);
				ImVec2 barPos = ImGui::GetCursorScreenPos();
				ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0, "%.3f");
				barPos.x += style.GrabMinSize * 0.5;
				barPos.y += ImGui::GetItemRectSize().y;
				float barWidth = ImGui::CalcItemWidth() - (style.GrabMinSize + UIUnit + style.ItemSpacing.x*2);
				ToolTip("The audio level needed to trigger the talking state", &_parent->_appConfig->_hoverTimer);
				ImGui::NewLine();

				DrawThresholdBar(_lastTalkFactor, _talkThreshold, barPos, uiScale, barWidth);

				if (ImGui::BeginTable("smoothinputtable", 2, ImGuiTableFlags_SizingFixedFit))
				{
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableNextColumn();
					ImGui::Checkbox("Smooth Input", &_smoothTalkFactor);
					ToolTip("Smooth the audio input coming into this layer", &_parent->_appConfig->_hoverTimer);

					ImGui::TableNextColumn();
					if (_smoothTalkFactor)
					{
						FloatSliderDrag("##Amount", &_smoothTalkFactorSize, 0.0, 50.0, "%.1f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("Set how much smoothing to apply.", &_parent->_appConfig->_hoverTimer, true);
					}
					ImGui::EndTable();
				}


				ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);
				ToolTip("Swap to the 'talk' sprite when Talk Threshold is reached", &_parent->_appConfig->_hoverTimer);

				ImGui::SameLine(0.0, 10.f);

				ImGui::Checkbox("Restart on Swap", &_restartTalkAnim);
				ToolTip("Restarts the 'talk' anim when swapping to it", &_parent->_appConfig->_hoverTimer);

				ImGui::SameLine(0.0, 10.f);

				ImGui::Checkbox("Smooth Tint", &_smoothTalkTint);
				ToolTip("Smoothly transition between the Idle and Talk tint colors", &_parent->_appConfig->_hoverTimer);

				ImVec2 subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
				if (ImGui::CollapsingHeader("Screaming", ImGuiTreeNodeFlags_AllowOverlap))
				{
					ToolTip("Swap to a different sprite when reaching a second audio threshold", &_parent->_appConfig->_hoverTimer);
					BetterIndent(indentSize, "screaming" + _id);

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

					ImGui::TextColored(style.Colors[ImGuiCol_Text], "Scream");
					ImGui::PushID("screamimport"); {

						ImageBrowsePreviewBtn(_importScreamOpen, "screamimgbtn", imgBtnWidth, _screamSpritePath, _screamSprite.get());

						ImGui::SameLine();
						ImGui::PushID("screamanimbtn"); {
							_spriteScreamOpen |= ImGui::ImageButton("screamanimbtn", *_animIcon, sf::Vector2f(animBtnWidth, animBtnWidth), sf::Color::Transparent, btnColor);
							ToolTip("Animation Settings", &_parent->_appConfig->_hoverTimer);
							AnimPopup(*_screamSprite, _spriteScreamOpen, _oldSpriteScreamOpen);
						}ImGui::PopID();//screamanimbtn

						ImGui::PopStyleVar();

						ImGui::PushID("screamimportfile"); {
							char screambuf[MAX_PATH] = {};
							ANSIToUTF8(_screamSpritePath).copy(screambuf, MAX_PATH);
							if (ImGui::InputText("", screambuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll))
							{
								_screamSpritePath = UTF8ToANSI(screambuf);
								_screamSprite->LoadFromTexture(_parent->_textureMan, _screamSpritePath, 1, 1, 1, 1, { -1,-1 }, & _parent->_errorMessage);
								_screamSprite->setSmooth(_scaleFiltering);
							}
						}ImGui::PopID();//screamimportfile
						ToolTip("Edit the current image path (This will reload the sprite texture!)", &_parent->_appConfig->_hoverTimer);

						ImGui::ColorEdit4("Tint", &_screamTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
						ToolTip("Tint the sprite a different color, or change its opacity (alpha value)", &_parent->_appConfig->_hoverTimer);
					}ImGui::PopID();

					AddResetButton("screamThresh", _screamThreshold, 0.15f, _parent->_appConfig, &style, true, &barPos);
					ImVec2 barPos = ImGui::GetCursorScreenPos();

					ImGui::SliderFloat("Scream Threshold", &_screamThreshold, 0.0, 1.0, "%.3f");
					barPos.x += style.GrabMinSize * 0.5;
					barPos.y += ImGui::GetItemRectSize().y;
					float barWidth = ImGui::CalcItemWidth() - (style.GrabMinSize + UIUnit + style.ItemSpacing.x * 2);
					ToolTip("The audio level needed to trigger the screaming state", &_parent->_appConfig->_hoverTimer, true);
					ImGui::NewLine();

					DrawThresholdBar(_lastTalkFactor, _screamThreshold, barPos, uiScale, barWidth);

					AddResetButton("minscream", _minScreamTime, 0.2f, _parent->_appConfig, &style);
					FloatSliderDrag("Min Time", &_minScreamTime, 0.0, 5.0, "%.1f s", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The minimum time the scream sprite should appear for\n(To eliminate flickering at the threshold)", &_parent->_appConfig->_hoverTimer, true);

					ImGui::Checkbox("Vibrate", &_screamVibrate);
					ToolTip("Randomly shake the sprite whilst screaming", &_parent->_appConfig->_hoverTimer);
					AddResetButton("vibamt", _screamVibrateAmount, 5.f, _parent->_appConfig, &style);
					FloatSliderDrag("Vibrate Amount", &_screamVibrateAmount, 0.0, 50.0, "%.1f px", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The distance of the vibration", &_parent->_appConfig->_hoverTimer, true);
					AddResetButton("vibspeed", _screamVibrateSpeed, 1.f, _parent->_appConfig, &style);
					FloatSliderDrag("Vibrate Speed", &_screamVibrateSpeed, 0.0, 5.0, "%.1f", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The speed of the vibration", &_parent->_appConfig->_hoverTimer, true);

					BetterUnindent("screaming" + _id);
				}
				else
					ToolTip("Swap to a different sprite when reaching a second audio threshold", &_parent->_appConfig->_hoverTimer);

				auto oldCursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(subHeaderBtnPos);
				ImGui::Checkbox("##Scream", &_scream);
				ImGui::SetCursorPos(oldCursorPos);

				subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
				if (ImGui::CollapsingHeader("Blinking", ImGuiTreeNodeFlags_AllowOverlap))
				{
					ToolTip("Show a blinking sprite at random intervals", &_parent->_appConfig->_hoverTimer);
					BetterIndent(indentSize, "blinking" + _id);

					ImGui::Checkbox("Blink While Talking", &_blinkWhileTalking);
					ToolTip("Show another blinking sprite whilst talking", &_parent->_appConfig->_hoverTimer);
					AddResetButton("blinkdur", _blinkDuration, 0.2f, _parent->_appConfig, &style);
					FloatSliderDrag("Blink Duration", &_blinkDuration, 0.0, 10.0, "%.2f s", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The amount of time to show the blinking sprite", &_parent->_appConfig->_hoverTimer, true);
					AddResetButton("blinkdelay", _blinkDelay, 6.f, _parent->_appConfig, &style);
					FloatSliderDrag("Blink Delay", &_blinkDelay, 0.0, 10.0, "%.2f s", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The amount of time between blinks", &_parent->_appConfig->_hoverTimer, true);
					AddResetButton("blinkvar", _blinkVariation, 4.f, _parent->_appConfig, &style);
					FloatSliderDrag("Variation", &_blinkVariation, 0.0, 5.0, "%.2f s", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("Adds a random variation to the Blink Delay.\nThis sets the maximum variation allowed.", &_parent->_appConfig->_hoverTimer, true);

					BetterUnindent("blinking" + _id);
				}
				else
					ToolTip("Show a blinking sprite at random intervals", &_parent->_appConfig->_hoverTimer);

				oldCursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(subHeaderBtnPos);
				ImGui::Checkbox("##Blink", &_useBlinkFrame);
				ImGui::SetCursorPos(oldCursorPos);

				bool hasParent = !(_motionParent == "" || _motionParent == "-1");

				if (!hasParent)
					ImGui::SetNextItemOpen(false);

				subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
				if (ImGui::CollapsingHeader("Motion Inherit", ImGuiTreeNodeFlags_AllowOverlap) && hasParent)
				{
					ToolTip("Copy the motion of another layer", &_parent->_appConfig->_hoverTimer);
					BetterIndent(indentSize, "motioninherit" + _id);

					if (hasParent)
					{
						if (LesserButton("View Inheritance Graph"))
						{
							//ImGui::OpenPopup("Inheritance Graph##motionInheritanceGraphPopup");
							_inheritanceGraphOpen = true;
							_inheritanceGraphWasOpen = false;
							_inheritanceGraphStartPos = ImGui::GetCursorScreenPos();
						}

						float md = _motionDelay;
						AddResetButton("motionDelay", _motionDelay, 0.f, _parent->_appConfig, &style);
						if (FloatSliderDrag("Delay", &md, 0.0, 1.0, "%.2f s", 0, _parent->_uiConfig->_numberEditType))
							_motionDelay = Clamp(md, 0.0, 1.0);

						ToolTip("The time before this layer follows the parent's motion", &_parent->_appConfig->_hoverTimer, true);

						AddResetButton("motionDrag", _motionDrag, 0.f, _parent->_appConfig, &style);
						if (FloatSliderDrag("Drag", &_motionDrag, 0.f, .999f, "%.2f", 0, _parent->_uiConfig->_numberEditType))
							_motionDrag = Clamp(_motionDrag, 0.f, .999f);
						ToolTip("Makes the layer slower to reach its target position", &_parent->_appConfig->_hoverTimer, true);

						AddResetButton("motionSpring", _motionSpring, 0.f, _parent->_appConfig, &style);
						if (FloatSliderDrag("Spring", &_motionSpring, 0.f, .999f, "%.2f", 0, _parent->_uiConfig->_numberEditType))
							_motionSpring = Clamp(_motionSpring, 0.f, .999f);
						ToolTip("Makes the layer slower to change direction", &_parent->_appConfig->_hoverTimer, true);

						AddResetButton("motionDistLimit", _distanceLimit, -1.f, _parent->_appConfig, &style);
						FloatSliderDrag("Distance limit", &_distanceLimit, 0.0, 100.f, "%.1f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("Limits how far this layer can stray from the parent's position\n(Set to -1 for no limit)", &_parent->_appConfig->_hoverTimer, true);

						AddResetButton("rotationEffect", _rotationEffect, 0.f, _parent->_appConfig, &style);
						FloatSliderDrag("Rotation effect", &_rotationEffect, -5.f, 5.f, "%.2f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("The amount of rotation to apply\n(based on the pivot point's distance from the layer's center)", &_parent->_appConfig->_hoverTimer, true);

						AddResetButton("stretchReset", _motionStretch, MS_None, _parent->_appConfig, &style);
						ImGui::Combo("Stretch", (int*)&_motionStretch, g_motionStretchNames, MotionStretch_End);
						ToolTip("Stretch the sprite along with the motion.\nUses the pivot point as the center of stretch.", &_parent->_appConfig->_hoverTimer);

						if (_motionStretch != MS_None)
						{
							AddResetButton("stretchStrengthReset", _motionStretchStrength, sf::Vector2f(1.0f, 1.0f), _parent->_appConfig, &style);
							Float2SliderDrag("Stretch strength", &_motionStretchStrength.x, -2.0f, 2.0f, "%.1f", 0, _parent->_uiConfig->_numberEditType);
							ToolTip("Set the strength of the stretch effect.", &_parent->_appConfig->_hoverTimer);

							AddResetButton("minStretchReset", _stretchScaleMin, sf::Vector2f(0.5f, 0.5f), _parent->_appConfig, &style);
							Float2SliderDrag("Min Scale", &_stretchScaleMin.x, -2.0f, 2.0f, "%.1f", 0, _parent->_uiConfig->_numberEditType);
							ToolTip("Set the minimum scale that stretch can apply.", &_parent->_appConfig->_hoverTimer);

							AddResetButton("maxStretchReset", _stretchScaleMax, sf::Vector2f(2.0f, 2.0f), _parent->_appConfig, &style);
							Float2SliderDrag("Max Scale", &_stretchScaleMax.x, -2.0f, 2.0f, "%.1f", 0, _parent->_uiConfig->_numberEditType);
							ToolTip("Set the maximum scale that stretch can apply.", &_parent->_appConfig->_hoverTimer);
						}

						ImGui::Checkbox("Ignore pivots", &_physicsIgnorePivots);
						ToolTip("Ignores the position of the layer's pivot point\nwhen calculating stretch and rotation.", &_parent->_appConfig->_hoverTimer);

						if (_physicsIgnorePivots)
						{
							float spacing = style.ItemSpacing.x;
							float spacingY = style.ItemSpacing.y;
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });

							if (ImGui::RadioButton("##topleft", _weightDirection == sf::Vector2f(-1.0, -1.0))) { _weightDirection = { -1.0, -1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##top", _weightDirection == sf::Vector2f(0.0, -1.0))) { _weightDirection = { 0.0, -1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##topRight", _weightDirection == sf::Vector2f(1.0, -1.0))) { _weightDirection = { 1.0, -1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);

							if (ImGui::RadioButton("##left", _weightDirection == sf::Vector2f(-1.0, 0.0))) { _weightDirection = { -1.0, 0.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##middle", _weightDirection == sf::Vector2f(0.0, 0.0))) { _weightDirection = { 0.0, 0.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##right", _weightDirection == sf::Vector2f(1.0, 0.0))) { _weightDirection = { 1.0, 0.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine(0, spacing * 2);
							ImGui::Text("Weight Direction");

							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,spacingY * 2 });

							if (ImGui::RadioButton("##bottomleft", _weightDirection == sf::Vector2f(-1.0, 1.0))) { _weightDirection = { -1.0, 1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##bottom", _weightDirection == sf::Vector2f(0.0, 1.0))) { _weightDirection = { 0.0, 1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);
							ImGui::SameLine();
							if (ImGui::RadioButton("##bottomright", _weightDirection == sf::Vector2f(1.0, 1.0))) { _weightDirection = { 1.0, 1.0 }; }
							ToolTip("Manually set the direction of the heaviest\npart of the sprite.\n(Normally calculated from the pivot location)", &_parent->_appConfig->_hoverTimer);

							ImGui::PopStyleVar(2);
						}

						ImGui::Checkbox("Hide with Parent", &_hideWithParent);
						ToolTip("Hide this layer when the parent is hidden.", &_parent->_appConfig->_hoverTimer);

						ImGui::Checkbox("Inherit Tint", &_inheritTint);
						ToolTip("Inherit the tint color of the parent layer.", &_parent->_appConfig->_hoverTimer);

						ImGui::Checkbox("Allow Individual Motion", &_allowIndividualMotion);
						ToolTip("Enable the original motion settings in addition to the parent.", &_parent->_appConfig->_hoverTimer);

					}

					BetterUnindent("motioninherit" + _id);
				}
				else
					ToolTip("Copy the motion of another layer", &_parent->_appConfig->_hoverTimer);


				oldCursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(subHeaderBtnPos);
				LayerInfo* oldMp = _parent->GetLayer(_motionParent);
				std::string mpName = oldMp ? oldMp->_name : "Off";
				ImGui::SetNextItemWidth(headerBtnSize.x * 7);

				if (ImGui::BeginCombo("##MotionInherit", ANSIToUTF8(mpName).c_str()))
				{
					float comboW = ImGui::GetContentRegionAvail().x;

					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, uiScale });

					if (ImGui::Selectable("Off", !hasParent))
						_motionParent = "-1";

					for (auto& layer : _parent->_layers)
					{
						if (layer._id != _id && layer._motionParent != _id && layer._isFolder == false)
						{
							if (layer._motionParent != "")
							{
								// If the selection layer has a parent, check the hierarchy to ensure it's not already beneath this one
								auto& parents = layer._lastCalculatedParents;
								if (std::find_if(parents.begin(), parents.end(), [&](const LayerInfo* lyr) {
										return lyr->_id == _id;
									}) != parents.end())
								{
									// Is a child, can't be a parent
									continue;
								}
							}

							bool customColor = layer._layerColor != ImVec4(0, 0, 0, 0);
							if (customColor)
								ImGui::PushStyleColor(ImGuiCol_Button, layer._layerColor);

							bool clicked = false;
							if (_motionParent == layer._id)
								clicked = ImGui::Button(ANSIToUTF8(layer._name).c_str(), { UIUnit * 8, UIUnit });
							else
								clicked = LesserButton(ANSIToUTF8(layer._name).c_str(), { UIUnit * 8, UIUnit }, false);

							if (customColor)
								ImGui::PopStyleColor();

							if (clicked)
							{
								_motionParent = layer._id;
								ImGui::CloseCurrentPopup();

								//recalculate the depths
								for (auto& layer : _parent->_layers)
								{
									layer.CalculateLayerDepth();
								}
							}
						}
					}
					ImGui::PopStyleVar(2);
					ImGui::EndCombo();
				}
				ImGui::SetCursorPos(oldCursorPos);

				bool indivEnabled = !hasParent || _allowIndividualMotion;

				ImVec2 headerSize = {};
				ImVec2 headerPosition = {};

				ImGui::BeginDisabled(!indivEnabled);
				{
					subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };

					if (!indivEnabled)
						ImGui::SetNextItemOpen(false);

					if (ImGui::CollapsingHeader("Individual Motion", ImGuiTreeNodeFlags_AllowOverlap))
					{
						BetterIndent(indentSize, "indivMotion" + _id);

						if (ImGui::BeginTabBar("##indivMotionTabs"))
						{
							if (ImGui::BeginTabItem("Talking"))
							{
								std::vector<const char*> bobOptions = { "None", "Loudness", "Regular", "Once" };
								if (ImGui::BeginCombo("Motion Type", bobOptions[_bounceType]))
								{
									if (ImGui::Selectable("None", _bounceType == BounceNone))
										_bounceType = BounceNone;
									ToolTip("No Talking Motion", &_parent->_appConfig->_hoverTimer);
									if (ImGui::Selectable("Loudness", _bounceType == BounceLoudness))
										_bounceType = BounceLoudness;
									ToolTip("Motion is determined by the audio level", &_parent->_appConfig->_hoverTimer);
									if (ImGui::Selectable("Regular", _bounceType == BounceRegular))
										_bounceType = BounceRegular;
									ToolTip("Fixed motion, on a regular time interval", &_parent->_appConfig->_hoverTimer);
									if (ImGui::Selectable("Once", _bounceType == BounceOnce))
										_bounceType = BounceOnce;
									ToolTip("One bounce, when you start talking", &_parent->_appConfig->_hoverTimer);
									ImGui::EndCombo();
								}
								ToolTip("Select the talking motion type", &_parent->_appConfig->_hoverTimer);

								if (_bounceType != BounceNone)
								{
									AddResetButton("bounceMoveReset", _bounceMove, { 0.0, 0.0 }, _parent->_appConfig, &style);
									float data[2] = { _bounceMove.x, _bounceMove.y };
									if (Float2SliderDrag("Move##talkingMove", data, -100, 100, "%.2f", 0, _parent->_uiConfig->_numberEditType))
										_bounceMove = { data[0], data[1] };
									ToolTip("The max distance the sprite will move", &_parent->_appConfig->_hoverTimer, true);

									AddResetButton("bouncerotatereset", _bounceRotation, 0.0f, _parent->_appConfig, &style);
									FloatSliderDrag("Rotation##talkingRot", &_bounceRotation, -180.f, 180.f, "%.1f deg", 0, _parent->_uiConfig->_numberEditType);
									ToolTip("The amount the sprite will rotate", &_parent->_appConfig->_hoverTimer, true);

									AddResetButton("breathscale", _bounceScale, { 0.0, 0.0 }, _parent->_appConfig, &style);
									float data2[2] = { _bounceScale.x, _bounceScale.y };
									if (Float2SliderDrag("Scale##talkingScale", data2, -1, 1, "%.2f", 0, _parent->_uiConfig->_numberEditType))
									{
										if (!_bounceScaleConstrain)
										{
											_bounceScale.x = data2[0];
											_bounceScale.y = data2[1];
										}
										else if (data2[0] != _bounceScale.x)
										{
											_bounceScale = { data2[0] , data2[0] };
										}
										else if (data2[1] != _bounceScale.y)
										{
											_bounceScale = { data2[1] , data2[1] };
										}
									}
									ToolTip("The amount added to the sprite's scale", &_parent->_appConfig->_hoverTimer, true);

									ImGui::PushID("BounceScaleConstrain"); {
										ImGui::Checkbox("Constrain", &_bounceScaleConstrain);
										ToolTip("Keep the X / Y scale the same", &_parent->_appConfig->_hoverTimer);
									}ImGui::PopID(); //bouncescaleconstrain


									if (_bounceType == BounceRegular || _bounceType == BounceOnce)
									{
										AddResetButton("bobtime", _bounceFrequency, 0.333f, _parent->_appConfig, &style);
										FloatSliderDrag("Cycle time##bounceCycleTime", &_bounceFrequency, 0.0, 2.0, "%.2f s", 0, _parent->_uiConfig->_numberEditType);
										ToolTip("The time taken to complete one full motion", &_parent->_appConfig->_hoverTimer, true);
									}
								}
								ImGui::EndTabItem();
							}
							ToolTip("Move the sprite whilst talking", &_parent->_appConfig->_hoverTimer);

							if (ImGui::BeginTabItem("Idle"))
							{
								ImGui::Checkbox("Do Idle Motion", &_idleMotionEnabled);
								if (_idleMotionEnabled)
								{
									AddResetButton("breathmove", _breathMove, { 0.0, 0.0 }, _parent->_appConfig, &style);
									float data[2] = { _breathMove.x, _breathMove.y };
									if (Float2SliderDrag("Move##idleMove", data, -100, 100, "%.2f", 0, _parent->_uiConfig->_numberEditType))
										_breathMove = { data[0], data[1] };
									ToolTip("The max distance the sprite will move", &_parent->_appConfig->_hoverTimer, true);

									AddResetButton("breathrotate", _breathRotation, 0.0f, _parent->_appConfig, &style);
									FloatSliderDrag("Rotation##idleRotate", &_breathRotation, -180.f, 180.f, "%.1f deg", 0, _parent->_uiConfig->_numberEditType);
									ToolTip("The amount the sprite will rotate", &_parent->_appConfig->_hoverTimer, true);

									AddResetButton("breathscale", _breathScale, { 0.0, 0.0 }, _parent->_appConfig, &style);
									float data2[2] = { _breathScale.x, _breathScale.y };
									if (Float2SliderDrag("Scale##idleScale", data2, -1, 1, "%.2f", 0, _parent->_uiConfig->_numberEditType))
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
									ToolTip("The amount added to the sprite's scale", &_parent->_appConfig->_hoverTimer, true);

									ImGui::PushID("BreathScaleConstrain"); {
										ImGui::Checkbox("Constrain", &_breathScaleConstrain);
										ToolTip("Keep the X / Y scale the same", &_parent->_appConfig->_hoverTimer);
									}ImGui::PopID(); //breathscaleconstrain

									ImGui::Checkbox("Circular Motion", &_breathCircular);
									ToolTip("Move the sprite in a circle instead of a line", &_parent->_appConfig->_hoverTimer);

									ImGui::Checkbox("Continue Whilst Talking", &_breatheWhileTalking);
									ToolTip("Idle animation continues whilst talking", &_parent->_appConfig->_hoverTimer);

									AddResetButton("breathfreq", _breathFrequency, 4.f, _parent->_appConfig, &style);
									FloatSliderDrag("Cycle Time##breathcycletime", &_breathFrequency, 0.0, 10.f, "%.2f s", 0, _parent->_uiConfig->_numberEditType);
									ToolTip("The time taken to complete one full motion", &_parent->_appConfig->_hoverTimer, true);

									ImGui::Checkbox("Change Color", &_doBreathTint);
									ToolTip("Interpolate the active sprite's Tint color with\nthis one along with the idle animation", &_parent->_appConfig->_hoverTimer);

									if (_doBreathTint)
									{
										AddResetButton("breathtintreset", _breathTint, _idleTint, _parent->_appConfig, &style);
										ImGui::ColorEdit4("Tint##BreathTint", &_breathTint.x, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs);
									}
								}
								ImGui::EndTabItem();
							}

							if (ImGui::BeginTabItem("Constant"))
							{
								if (ImGui::Button("Reset"))
								{
									_storedConstantScale = { 1.f, 1.f };
									_storedConstantPos = { 0,0 };
									_storedConstantRot = 0;
								}
								ToolTip("Reset the stored constant motion", &_parent->_appConfig->_hoverTimer);

								AddResetButton("rotspeedreset", _constantRot, 0.0, _parent->_appConfig, &style);
								float cRot = _constantRot;
								if (FloatSliderDrag("Rotation Speed", &cRot, -360, 360, "%.1f deg/s", 0, _parent->_uiConfig->_numberEditType))
									_constantRot = cRot;
								ToolTip("Continuously rotate the sprite with this speed \n(degrees per second)", &_parent->_appConfig->_hoverTimer, true);
								ImGui::EndTabItem();
							}

							ImGui::EndTabBar();
						}
						BetterUnindent("indivMotion" + _id);
					}

					headerSize = ImGui::GetItemRectSize();
					headerPosition = ImGui::GetItemRectMin();

					if (indivEnabled)
					{
						ToolTip("Set motion during talking, when idle,\nor to occur constantly.", &_parent->_appConfig->_hoverTimer);
					}

					auto oldCursorPos = ImGui::GetCursorPos();
					ImGui::SetCursorPos(subHeaderBtnPos);

					if (_bounceType != BounceNone)
					{
						if (GreaterButton("T##TalkEnableHeader", { UIUnit, UIUnit }, false))
						{
							_lastEnabledBounceType = _bounceType;
							_bounceType = BounceNone;
						}
					}
					else
					{
						if (LesserButton("T##TalkEnableHeader", { UIUnit, UIUnit }, false))
						{
							_bounceType = _lastEnabledBounceType;
						}
					}
					ToolTip("Quickly enable/disable Talking motion", &_parent->_appConfig->_hoverTimer);
					ImGui::SameLine();
					if (_idleMotionEnabled)
					{
						if (GreaterButton("I##IdleEnableHeader", { UIUnit, UIUnit }, false))
							_idleMotionEnabled = false;
					}
					else
					{
						if (LesserButton("I##IdleEnableHeader", { UIUnit, UIUnit }, false))
							_idleMotionEnabled = true;
					}
					ToolTip("Quickly enable/disable Idle motion", &_parent->_appConfig->_hoverTimer);


					ImGui::SetCursorPos(oldCursorPos);
				}
				ImGui::EndDisabled();
				if (hasParent)
				{
					auto curpos = ImGui::GetCursorPos();

					if (!indivEnabled)
					{
						headerSize.x -= UIUnit * 1.5;
						ImGui::SetCursorScreenPos(headerPosition);
						ImGui::InvisibleButton("indivMotionDisabledTooltip", headerSize);
						ToolTip("This is disabled by default while Motion Inherit is used.\nYou can re-enable it in the Motion Inherit settings.", &_parent->_appConfig->_hoverTimer);
					}

					subHeaderBtnPos.x = ImGui::GetContentRegionMax().x - UIUnit;
					ImGui::SetCursorPos(subHeaderBtnPos);
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });
					if (ImGui::ImageButton("allowIndivMotion", _allowIndividualMotion ? *_lockOpenIcon : *_lockClosedIcon, headerBtnSize, sf::Color::Transparent, btnColor))
					{
						_allowIndividualMotion = !_allowIndividualMotion;
					}
					ImGui::PopStyleVar(2);
					if (_allowIndividualMotion)
						ToolTip("Disable Individual Motion alongide Motion Inherit\n(See Motion Inherit settings)", &_parent->_appConfig->_hoverTimer);
					else
						ToolTip("Enable Individual Motion alongide Motion Inherit\n(See Motion Inherit settings)", &_parent->_appConfig->_hoverTimer);


					ImGui::SetCursorPos(curpos);
				}

				subHeaderBtnPos = { ImGui::GetWindowWidth() - headerBtnSize.x * 8, ImGui::GetCursorPosY() };
				if (ImGui::CollapsingHeader("Tracking", ImGuiTreeNodeFlags_AllowOverlap))
				{
					const sf::Vector2f halfFullscreen(_parent->_appConfig->_fullScrW / 2, _parent->_appConfig->_fullScrH / 2);
					if (_mouseNeutralPos == sf::Vector2f(-1.f, -1.f))
					{
						_mouseNeutralPos = halfFullscreen;
					}

					if (_mouseAreaSize == sf::Vector2f(-1.f, -1.f))
					{
						_mouseAreaSize = halfFullscreen;
					}

					BetterIndent(indentSize, "tracking" + _id);

					float widgetWidth = 0.7;

					if (ImGui::BeginCombo("Tracking Type", g_trackingNames[_trackingType]))
					{

						if (ImGui::Selectable("Mouse", _trackingType == TRACKING_MOUSE))
							_trackingType = TRACKING_MOUSE;
						ToolTip("Track the mouse movement", &_parent->_appConfig->_hoverTimer);
						if (ImGui::Selectable("Controller Axis", _trackingType == TRACKING_CONTROLLER))
							_trackingType = TRACKING_CONTROLLER;
						ToolTip("Track the movement of a controller analog stick", &_parent->_appConfig->_hoverTimer);
						if (ImGui::Selectable("Combined", _trackingType == TRACKING_BOTH))
							_trackingType = TRACKING_BOTH;
						ToolTip("Combine the first two options", &_parent->_appConfig->_hoverTimer);
						ImGui::EndCombo();
					}

					if (_trackingType & TRACKING_MOUSE)
					{
						ImGui::SeparatorText("Mouse");

						AddResetButton("neutralPos", _mouseNeutralPos, halfFullscreen, _parent->_appConfig, &style);
						ImGui::InputFloat2("Neutral position", &_mouseNeutralPos.x, "%.1f px", ImGuiInputTextFlags_CharsNoBlank);
						ToolTip("The 'starting point' - 0 movement when the mouse is here\nThese are screen co-ordinates relative to your main monitor.", &_parent->_appConfig->_hoverTimer);

						AddResetButton("distFactor", _mouseAreaSize, halfFullscreen, _parent->_appConfig, &style);
						ImGui::InputFloat2("Distance Factor", &_mouseAreaSize.x, "%.1f px", ImGuiInputTextFlags_CharsNoBlank);
						ToolTip("The maximum mouse distance from the Neutral position.\nThese are screen co-ordinates relative to your main monitor.", &_parent->_appConfig->_hoverTimer);
					
						if (_trackingType == TRACKING_BOTH)
						{
							AddResetButton("mouseEffect", _mouseEffect, 1.f, _parent->_appConfig, &style);
							FloatSliderDrag("Mouse Effect", &_mouseEffect, 0.f, 1.0f, "%.2f", 0, _parent->_uiConfig->_numberEditType);
							ToolTip("Choose how much the mouse movement affects the tracking.", &_parent->_appConfig->_hoverTimer);
						}

					}

					if (_trackingType & TRACKING_CONTROLLER)
					{
						ImGui::SeparatorText("Controller");

						if (ImGui::BeginCombo("Controller Select", g_trackingSelectNames[_trackingSelect]))
						{
							for (int csel = 0; csel < TRACKINGSELECT_END; csel++)
							{
								if (ImGui::Selectable(g_trackingSelectNames[csel]))
								{
									_trackingSelect = (TrackingControllerSelect)csel;
									if (_trackingSelect == TRACKINGSELECT_FIRST)
										_trackingJoystick = -1;
								}
							}
							ImGui::EndCombo();
						}
						ToolTip("Choose how RahiTuber selects a controller to use.", &_parent->_appConfig->_hoverTimer);

						if (_trackingSelect == TRACKINGSELECT_SPECIFIC)
						{
							if (ImGui::BeginCombo("Controller Index", std::to_string(_trackingJoystick).c_str()))
							{
								for (int ctrlr = 0; ctrlr < 8; ctrlr++)
								{
									if (sf::Joystick::isConnected(ctrlr))
									{
										std::stringstream name;
										name << "[" << ctrlr << "] " << (std::string)sf::Joystick::getIdentification(ctrlr).name;
										if (ImGui::Selectable(name.str().c_str()))
										{
											_trackingJoystick = (TrackingControllerSelect)ctrlr;
										}
									}
								}
								ImGui::EndCombo();
							}
							ToolTip("Choose which controller this layer tracks.\nThis is chosen by connection order (by design).\nThe name is just for your reference.", &_parent->_appConfig->_hoverTimer);
						}

						if (ImGui::BeginCombo("Controller Axis", g_trackingAxisNames[_trackingAxis]))
						{
							for (int ax = 0; ax < AXIS_END; ax++)
							{
								if (ImGui::Selectable(g_trackingAxisNames[ax]))
								{
									_trackingAxis = (TrackingAxis)ax;
								}
							}
							ImGui::EndCombo();
						}
						ToolTip("Choose which axis this layer will track.", &_parent->_appConfig->_hoverTimer);

						AddResetButton("axisDeadzone", _axisDeadzone, 0.f, _parent->_appConfig, &style);
						if (FloatSliderDrag("Axis Deadzone", &_axisDeadzone, 0.f, 0.99f, "%.2f", 0, _parent->_uiConfig->_numberEditType))
							Clamp(_axisDeadzone, 0.0f, 0.99f);
						ToolTip("The amount your joystick will have to move before doing anything", &_parent->_appConfig->_hoverTimer);

						if (_trackingType == TRACKING_BOTH)
						{
							AddResetButton("ctrlEffect", _joypadEffect, 1.f, _parent->_appConfig, &style);
							FloatSliderDrag("Controller Effect", &_joypadEffect, 0.f, 1.0f, "%.2f", 0, _parent->_uiConfig->_numberEditType);
							ToolTip("Choose how much the joystick movement affects the tracking.", &_parent->_appConfig->_hoverTimer);
						}
					}

					ImGui::SeparatorText("");

					AddResetButton("trackingSmooth", _trackingSmooth, 0.2f, _parent->_appConfig, &style);
					FloatSliderDrag("Smooth", &_trackingSmooth, 0.f, 1.f, "%.2f", ImGuiInputTextFlags_CharsNoBlank, _parent->_uiConfig->_numberEditType);
					ToolTip("How much to smooth the movement", &_parent->_appConfig->_hoverTimer);

					AddResetButton("moveLimits", _trackingMoveLimits, sf::Vector2f(50.f, 50.f), _parent->_appConfig, &style);
					Float2SliderDrag("Movement Limits", &_trackingMoveLimits.x, -halfFullscreen.x, halfFullscreen.x, "%.1f px", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The maximum offset applied to the layer position.", &_parent->_appConfig->_hoverTimer, true);

					AddResetButton("rotLimits", _trackingRotation, sf::Vector2f(0.f, 0.f), _parent->_appConfig, &style);
					Float2SliderDrag("Rotation Limits", &_trackingRotation.x, -180.f, 180.f, "%.1f deg", 0, _parent->_uiConfig->_numberEditType);
					ToolTip("The maximum rotation applied to the layer.\n(First box is from horizontal movement, 2nd box from vertical)", &_parent->_appConfig->_hoverTimer, true);

					ImGui::Separator();

					if (ImGui::BeginCombo("Scale type", g_trackingScaleNames[_trackingScaleMode]))
					{
						for (int sm = 0; sm < TRACKINGSCALE_END; sm++)
						{
							if (ImGui::Selectable(g_trackingScaleNames[sm]))
							{
								_trackingScaleMode = (TrackingScaleMode)sm;

								if ((_trackingScaleMode & TRACKINGSCALE_SPLIT) == 0)
								{
									_trackingScaleHorizontal = { _trackingScaleHorizontal.x, _trackingScaleHorizontal.x };
									_trackingScaleVertical = { _trackingScaleVertical.y, _trackingScaleVertical.y };
								}
							}
						}
						ImGui::EndCombo();
					}
					ToolTip("Choose the type of scaling", &_parent->_appConfig->_hoverTimer);

					if ((_trackingScaleMode & TRACKINGSCALE_SPLIT) == 0)
					{
						sf::Vector2f uniformScale = { _trackingScaleHorizontal.x, _trackingScaleVertical.x };
						sf::Vector2f reset = { 0.f,0.f };

						bool scalereset = AddResetButton("scaleLimits", uniformScale, reset, _parent->_appConfig, &style);
						if (Float2SliderDrag("Scale", &uniformScale.x, -2.f, 2.f, "%.3f", 0, _parent->_uiConfig->_numberEditType) || scalereset)
						{
							_trackingScaleHorizontal = { uniformScale.x, uniformScale.x };
							_trackingScaleVertical = { uniformScale.y, uniformScale.y };
						}
						ToolTip("The scale added to the layer.\n(First box is from horizontal movement, 2nd box from vertical)", &_parent->_appConfig->_hoverTimer, true);
					}
					else
					{
						AddResetButton("scaleLimitsHoriz", _trackingScaleHorizontal, sf::Vector2f(0.f, 0.f), _parent->_appConfig, &style);
						Float2SliderDrag("Horizontal Scale", &_trackingScaleHorizontal.x, -2.f, 2.f, "%.3f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("The scale added to the layer from\nhorizontal mouse/controller movement", &_parent->_appConfig->_hoverTimer, true);


						AddResetButton("scaleLimitsVert", _trackingScaleVertical, sf::Vector2f(0.f, 0.f), _parent->_appConfig, &style);
						Float2SliderDrag("Vertical Scale", &_trackingScaleVertical.x, -2.f, 2.f, "%.3f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("The scale added to the layer from\nvertical mouse/controller movement", &_parent->_appConfig->_hoverTimer, true);

					}

					std::vector<SwapButtonDef> magBtns = {
						{"Directional", "Scale is reversed when below/behind the Neutral Position", 0},
						{"Absolute", "Scale is based on the absolute distance from the Neutral Position", 1}
					};

					SwapButtons("Magnitude", magBtns, _trackingScaleAbsolute, &_parent->_appConfig->_hoverTimer, true);

					ImGui::Checkbox("Clamp Scale", &_clampTrackingScale);
					ToolTip("Clamp the tracking scale to within a certain range.", &_parent->_appConfig->_hoverTimer);

					if (_clampTrackingScale)
					{
						AddResetButton("scaleClamp", _trackingScaleClamp, sf::Vector2f(-1.f, 1.f), _parent->_appConfig, &style);
						Float2SliderDrag("Scale Clamp", &_trackingScaleClamp.x, -2.f, 2.f, "%.3f", 0, _parent->_uiConfig->_numberEditType);
						ToolTip("The minimum and maximum scale that can be added to the layer.\nSet min to 0.0 to avoid shrinking\nSet min to -1.0 to avoid flipping", &_parent->_appConfig->_hoverTimer, true);
					}

					ImGui::Separator();

					ImGui::Checkbox("Elliptical", &_followElliptical);
					ToolTip("Limits the movement to an ellipse based on the Movement Limits.", &_parent->_appConfig->_hoverTimer);

					ImGui::Checkbox("Only When Visible", &_trackingOffWhenHidden);
					ToolTip("Stops moving the layer with the mouse if it's invisible.", &_parent->_appConfig->_hoverTimer);
					

					BetterUnindent("tracking" + _id);
				}
				oldCursorPos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(subHeaderBtnPos);
				ImGui::Checkbox("##trackingEnabled", &_trackingEnabled);
				ToolTip("Move this layer with a mouse or controller.", &_parent->_appConfig->_hoverTimer);
				ImGui::SetCursorPos(oldCursorPos);


				if (ImGui::CollapsingHeader("Transforms", ImGuiTreeNodeFlags_AllowOverlap))
				{
					BetterIndent(indentSize, "transforms" + _id);

					AddResetButton("pos", _pos, sf::Vector2f(0.0, 0.0), _parent->_appConfig, &style);
					float pos[2] = { _pos.x, _pos.y };
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 30);
					if (Float2SliderDrag("Position", pos, -1000.0, 1000.f, "%.0f px", 0, _parent->_uiConfig->_numberEditType))
					{
						_pos.x = pos[0];
						_pos.y = pos[1];
					}

					ToolTip("The position of this layer.", &_parent->_appConfig->_hoverTimer, true);

					AddResetButton("rot", _rot, 0.0, _parent->_appConfig, &style);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 30);
					float rot = _rot;
					if (FloatSliderDrag("Rotation", &rot, -180.f, 180.f, "%.1f deg", 0, _parent->_uiConfig->_numberEditType))
						_rot = rot;
					ToolTip("The rotation of this layer.", &_parent->_appConfig->_hoverTimer, true);

					AddResetButton("scale", _scale, sf::Vector2f(1.0, 1.0), _parent->_appConfig, &style);
					float scale[2] = { _scale.x, _scale.y };
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 30);
					if (Float2SliderDrag("Scale", scale, 0.0, 5.f, "%.2f", 0, _parent->_uiConfig->_numberEditType))
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
					ToolTip("The scale of this layer.", &_parent->_appConfig->_hoverTimer, true);

					ImGui::Checkbox("Constrain", &_keepAspect);
					ToolTip("Keeps the X and Y scale values the same", &_parent->_appConfig->_hoverTimer);

					ImGui::Checkbox("Rotate Children", &_passRotationToChildLayers);
					ToolTip("Apply this rotation value to any layers using\nthis as a Motion Parent.\n\
(This is different from the rotation in Individual Motion.\nChildrens' position will also be rotated.)", &_parent->_appConfig->_hoverTimer);

					sf::Vector2f spriteSize = _idleSprite->Size();

					sf::Vector2<double> prevPivot = _pivot;
					AddResetButton("pivot", _pivot, sf::Vector2<double>(0.5, 0.5), _parent->_appConfig, &style);
					if (prevPivot != _pivot && _parent->_pivotPreservePosition)
						_pos += sf::Vector2f((_pivot - prevPivot) * sf::Vector2<double>(spriteSize) * sf::Vector2<double>(_scale));

					std::vector<float> pivot = { (float)_pivot.x * 100, (float)_pivot.y * 100 };
					std::string pivunit = "%";
					std::string pivfmt = "%.1f %%";
					float pivmax = 100.0;
					float pivmin = 0.0;
					if (_pivotPx)
					{
						pivunit = "px";
						pivfmt = "%.1f px";
						pivmax = Max(spriteSize.x, spriteSize.y);
						pivot = { (float)_pivot.x * spriteSize.x, (float)_pivot.y * spriteSize.y };
					}

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 45);
					if (Float2SliderDrag("Pivot Point", pivot.data(), pivmin, pivmax, pivfmt.c_str(), 0, _parent->_uiConfig->_numberEditType))
					{
						sf::Vector2<double> prevPivot = _pivot;

						if (!_pivotPx)
						{
							_pivot.x = pivot[0] / 100;
							_pivot.y = pivot[1] / 100;
						}
						else
						{
							_pivot.x = pivot[0] / spriteSize.x;
							_pivot.y = pivot[1] / spriteSize.y;
						}

						if (_parent->_pivotPreservePosition)
						{
							sf::Vector2<double> pivotDiff = _pivot - prevPivot;
							sf::Vector2<double> pivotRotatedDiff = Rotate(pivotDiff, Deg2Rad(_rot));
							_pos += sf::Vector2f(pivotRotatedDiff * sf::Vector2<double>(spriteSize) * sf::Vector2<double>(_scale));
						}

					}
					ToolTip("Sets the pivot point (range 0 - 1. 0 = top/left, 1 =  bottom/right)", &_parent->_appConfig->_hoverTimer, true);
					ImGui::SameLine();
					if (ImGui::Button(_pivotPx ? "px" : "%"))
					{
						_pivotPx = !_pivotPx;
					}
					ImGui::Checkbox("Preserve Position", &_parent->_pivotPreservePosition);
					ToolTip("Preserve the layer position while moving the pivot point.", &_parent->_appConfig->_hoverTimer);

					BetterUnindent("transforms" + _id);
				}
				else
				{
					ToolTip("Set the Position, Scale and Rotation of the layer\n(also Pivot Point and Mouse Tracking)", &_parent->_appConfig->_hoverTimer);
				}
				ImGui::Separator();

			}

			ImGui::Unindent(indentSize);

		}
		else
		{
			if (ImGui::IsItemHovered() && _isFolder == false)
				_parent->_hoveredLayers.push_back(_id);

			if (_layerColor.w != 0)
				ImGui::PopStyleColor();

			if (_isFolder)
			{
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();
			}
		}

		auto oldCursorPos = ImGui::GetCursorPos();
		

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

		if (_parent->_appConfig->_unloadTimeoutEnabled && !_isFolder)
		{
			auto pinPos = headerButtonsPos;
			pinPos.x -= UIUnit;
			ImGui::SetCursorPos(pinPos);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			if (ImGui::ImageButton("pinloaded", _pinLoaded ? *_pinIcon : *_pinOffIcon, headerBtnSize, sf::Color::Transparent, btnColor))
			{
				_pinLoaded = !_pinLoaded;

				SetUnloadingTimer(_parent->_appConfig->_unloadTimeout);
			}
			ImGui::PopStyleColor();
			ToolTip(_pinLoaded ? "Pinned" : "Unpinned", _pinLoaded ? "Click to unpin\n(allow unloading this layer's images)" : "Click to pin\n(keep this layer's images in memory)", & _parent->_appConfig->_hoverTimer);
		}
		
		ImGui::SetCursorPos(headerButtonsPos);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.5));
		if (ImGui::ImageButton("visible", _visible ? *_eyeOpenIcon : *_eyeClosedIcon, headerBtnSize, sf::Color::Transparent, btnColor))
		{
			_visible = !_visible;

			bool safe = true;
			// if any active state changes this layer's visibility, it's not safe to update the default
			int stateIdx = 0;
			for (auto& state : _parent->_states)
			{
				if (state._active == false)
					continue;

				{
					std::scoped_lock lockState(_parent->_stateLocks[stateIdx]);
					if (state._layerStates.count(_id) == 0u)
						continue;

					if (state._layerStates[_id] != StatesInfo::NoChange)
					{
						safe = false;
						break;
					}
				}
				++stateIdx;
			}
			if (safe)
				_parent->_defaultLayerStates[_id] = _visible;
		}
		ImGui::PopStyleColor();
		ToolTip("Show or hide the layer", &_parent->_appConfig->_hoverTimer);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.25));
		ImGui::SameLine();
		if (ImGui::ImageButton("upbtn", *_upIcon, headerBtnSize, sf::Color::Transparent, btnColor))
			_parent->MoveLayerUp(this);
		ToolTip("Move the layer up", &_parent->_appConfig->_hoverTimer);

		ImGui::SameLine();
		if (ImGui::ImageButton("dnbtn", *_dnIcon, headerBtnSize, sf::Color::Transparent, btnColor))
			_parent->MoveLayerDown(this);
		ToolTip("Move the layer down", &_parent->_appConfig->_hoverTimer);

		ImGui::SameLine();
		if (ImGui::ImageButton("renamebtn", *_editIcon, headerBtnSize, sf::Color::Transparent, btnColor))
		{
			_renamingString = _name;
			_renamePopupOpen = true;
			_renameFirstOpened = true;
			ImGui::SetNextWindowSize({ 200 * uiScale,80 * uiScale });
			ImGui::OpenPopup("Rename Layer");
		}
		ToolTip("Rename or Color the layer", &_parent->_appConfig->_hoverTimer);

		ImGui::SameLine();
		if (ImGui::ImageButton("dupebtn", *_dupeIcon, headerBtnSize, sf::Color::Transparent, btnColor))
		{
			allowContinue = false;
			_parent->AddLayer(this);
		}
		ToolTip("Duplicate the layer", &_parent->_appConfig->_hoverTimer);
		ImGui::PopStyleColor();

		if (ImGui::BeginPopupModal("Rename Layer", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::PopStyleVar(2);

			char inputStr[32] = " ";
			ANSIToUTF8(_renamingString).copy(inputStr, 32);
			if (_renameFirstOpened)
			{
				ImGui::SetKeyboardFocusHere();
				_renameFirstOpened = false;
			}

			bool edited = false;
			if (ImGui::InputText("##renamebox", inputStr, 32, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				edited = true;
			}
			_renamingString = UTF8ToANSI(inputStr);
			ImGui::SameLine();

			bool saved = ImGui::Button("Save");

			ImGui::Separator();

			if (ImGui::BeginTable("colorPickerTable", 8, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadInnerX))
			{
				float fp = ImGui::GetStyle().FramePadding.y;
				ImVec2 btnSize = { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() };
				sf::Vector2f imgBtnSize = { ImGui::GetFrameHeight() - fp * 2,ImGui::GetFrameHeight() - fp * 2 };
				auto textCol = toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));

				ImGui::TableNextColumn();

				ImVec2 colPos = ImGui::GetCursorPos();
				if (ImGui::ColorButton("delbtn", ImGui::GetStyleColorVec4(ImGuiCol_Header), ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0, 0, 0, 0 };

				ImGui::SetCursorPos(colPos + ImVec2(fp, fp));
				ImGui::Image(*_delIcon, imgBtnSize, textCol);


				ImGui::TableNextColumn();
				colPos = ImGui::GetCursorPos();
				if (ImGui::ColorEdit3("##customCol", &_layerColor.x, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs))
					_layerColor.w = 1.f;

				ImGui::SetCursorPos(colPos + ImVec2(fp, fp));
				ImGui::Image(*_editIcon, imgBtnSize, textCol);

				ImGui::TableNextColumn();
				if (ImGui::ColorButton("red", { 0.5, 0.0, 0.1, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.5, 0.0, 0.1, 1 };
				ImGui::TableNextColumn();
				if (ImGui::ColorButton("yellow", { 0.5, 0.35, 0.1, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.5, 0.35, 0.1, 1 };
				ImGui::TableNextColumn();
				if (ImGui::ColorButton("green", { 0.1, 0.4, 0.1, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.1, 0.4, 0.1, 1 };
				ImGui::TableNextColumn();
				if (ImGui::ColorButton("cyan", { 0.1, 0.4, 0.4, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.1, 0.4, 0.4, 1 };
				ImGui::TableNextColumn();
				if (ImGui::ColorButton("blue", { 0.1, 0.2, 0.5, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.1, 0.2, 0.5, 1 };
				ImGui::TableNextColumn();
				if (ImGui::ColorButton("magenta", { 0.5, 0.1, 0.4, 1 }, ImGuiColorEditFlags_NoTooltip, btnSize))
					_layerColor = { 0.5, 0.1, 0.4, 1 };


				ImGui::EndTable();
			}

			if (saved || edited)
			{
				_renamePopupOpen = false;
				_name = _renamingString;
				ImGui::CloseCurrentPopup();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 1,1 });

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, { 0.5,0.1,0.1,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.8,0.2,0.2,1.0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8,0.4,0.4,1.0 });
		ImGui::PushStyleColor(ImGuiCol_Text, { 255 / 255,200 / 255,170 / 255, 1 });
		ImGui::PushID("deleteBtn"); {
			if (ImGui::ImageButton("delbtn", *_delIcon, headerBtnSize, sf::Color::Transparent, sf::Color(255, 200, 170)))
			{
				allowContinue = false;
				_parent->RemoveLayer(this);
			}
			ToolTip("Delete the layer", &_parent->_appConfig->_hoverTimer);
		}ImGui::PopID();//deletebtn

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);

		ImGui::SetCursorPos(oldCursorPos);

		if (_inheritanceGraphOpen)
		{
			std::string graphName = "Inheritance Graph (" + _name + ")";
			if (!_inheritanceGraphWasOpen)
			{
				ImGui::SetNextWindowPos(_inheritanceGraphStartPos, 0, { 0,1 });
				ImGui::SetNextWindowFocus();
			}
			ImGui::SetNextWindowSizeConstraints({ ImGui::CalcTextSize(graphName.c_str()).x + ImGui::GetFrameHeight() * 2, 0 }, ImGui::GetWindowSize());

			if (ImGui::Begin(graphName.c_str(), 0, ImGuiWindowFlags_AlwaysAutoResize))
			{
				_inheritanceGraphWasOpen = true;

				float treeIndent = ImGui::GetFrameHeight() * 0.8;
				std::vector<LayerInfo*> layerParents = _lastCalculatedParents;
				for (int n = 0; n < layerParents.size(); n++)
				{
					ImGui::Indent(treeIndent);

					auto& graphLayer = *layerParents[n];
					bool vis = graphLayer.EvaluateLayerVisibility();
					ImVec4 col = graphLayer._id == _id ? style.Colors[ImGuiCol_ButtonActive] * ImVec4(2, 2, 2, 0.8) : (vis ? style.Colors[ImGuiCol_Text] : style.Colors[ImGuiCol_TextDisabled]);
					ImGui::PushStyleColor(ImGuiCol_Text, col);
					auto cursPos = ImGui::GetCursorScreenPos();
					if (ImGui::Selectable(graphLayer._name.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
					{
						graphLayer._scrollToHere = true;
						if (graphLayer._inFolder != "")
							_parent->GetLayer(graphLayer._inFolder)->_scrollToHere = true;
					}
					ImGui::PopStyleColor();
					ToolTip("Open and scroll to this layer", &_parent->_appConfig->_hoverTimer);

					if (n > 0)
					{
						auto* drawList = ImGui::GetWindowDrawList();
						cursPos.y += treeIndent / 2;
						cursPos.x -= treeIndent * 0.3f;
						ImVec2 linePos1 = { cursPos.x - treeIndent * 1.3f, cursPos.y };
						ImVec2 linePos2 = { linePos1.x, linePos1.y - treeIndent };
						ImVec2 points[3] = { linePos2, linePos1, cursPos };
						drawList->AddPolyline(points, 3, ImGui::ColorConvertFloat4ToU32(col), ImDrawFlags_None, 2.0);
					}
				}

				ImGui::Unindent(treeIndent * layerParents.size());

				if (ImGui::Button("Close"))
				{
					_inheritanceGraphOpen = false;
					_inheritanceGraphWasOpen = false;
				}
			}
			ImGui::End();
		}

	}ImGui::PopID();

	return allowContinue;
}

void LayerManager::LayerInfo::ImageBrowsePreviewBtn(bool& openFlag, const char* btnname, float imgBtnWidth, std::string& path, SpriteSheet* sprite)
{
	bool emptyTex = !sprite->HasTexture();
	sf::Color btnCol = emptyTex ? toSFColor(ImGui::GetStyleColorVec4(ImGuiCol_Text)) : sf::Color::White;
	sf::Texture* btnIcon = emptyTex ? _emptyIcon : sprite->getTexture();

	bool reloading = false;
	if (btnIcon == nullptr)
	{
		reloading = true;
		btnIcon = _reloadIcon;
	}

	static imgui_ext::file_browser_modal fileBrowserIdle("Import Sprite");
	if (fileBrowserIdle.GetLastChosenDir() == "")
		fileBrowserIdle.SetStartingDir(_parent->_loadedXMLAbsDirectory);
	else
		fileBrowserIdle.SetStartingDir(fileBrowserIdle.GetLastChosenDir());

	ImGui::BeginDisabled(reloading);
	openFlag = ImGui::ImageButton(btnname, *btnIcon, { imgBtnWidth,imgBtnWidth }, sf::Color::Transparent, btnCol);
	ToolTip("Browse for an image file", &_parent->_appConfig->_hoverTimer);
	if (openFlag && sprite->HasTexture())
		fileBrowserIdle.SetStartingDir(path);
	if (fileBrowserIdle.render(openFlag, path))
	{
		if (path == "")
			sprite->Clear();
		else
		{
			sprite->LoadFromTexture(_parent->_textureMan, path, 1, 1, 1, 1, { -1, -1 }, &_parent->_errorMessage);
			sprite->setSmooth(_scaleFiltering);
		}
	}
	ImGui::EndDisabled();
}

void LayerManager::LayerInfo::DrawThresholdBar(float thresholdLevel, float thresholdTrigger, ImVec2& barPos, float uiScale, float barWidth)
{
	sf::Color barHighlight(60, 140, 60, 255);
	sf::Color barBg(20, 60, 20, 255);
	if (thresholdLevel < 0.001 || thresholdLevel < thresholdTrigger)
	{
		barHighlight = sf::Color(140, 60, 60, 255);
		barBg = sf::Color(60, 20, 20, 255);
	}

	auto drawList = ImGui::GetWindowDrawList();

	sf::Vector2f topLeft = { barPos.x, barPos.y };
	//float barWidth = (ImGui::GetWindowWidth() - topLeft.x) - 148;
	float barHeight = 10 * uiScale;
	ImVec2 volumeBarTL = { topLeft.x, topLeft.y };
	ImVec2 volumeBarBR = { volumeBarTL.x + barWidth, volumeBarTL.y + barHeight };
	float activeBarWidth = barWidth * thresholdLevel;
	ImVec2 activeBarBarBR = { volumeBarTL.x + activeBarWidth, volumeBarTL.y + barHeight };
	drawList->AddRectFilled(volumeBarTL, volumeBarBR, toImColor(barBg), 3 * uiScale);
	drawList->AddRectFilled(volumeBarTL, activeBarBarBR, toImColor(barHighlight), 3 * uiScale);
	float rootThresh = thresholdTrigger;
	float thresholdPos = barWidth * rootThresh;
	ImVec2 thresholdBarTL = { topLeft.x + thresholdPos, topLeft.y - 3 * uiScale };
	ImVec2 thresholdBarBR = { thresholdBarTL.x + 2 * uiScale, thresholdBarTL.y + barHeight + 5 * uiScale };
	drawList->AddRectFilled(thresholdBarTL, thresholdBarBR, ImColor(200, 150, 80));
}

void LayerManager::LayerInfo::AnimPopup(SpriteSheet& anim, bool& open, bool& oldOpen)
{
	float uiScale = _parent->_appConfig->scalingFactor;

	if (open != oldOpen)
	{
		oldOpen = open;

		if (open)
		{
			ImGui::SetNextWindowPos({ _parent->_appConfig->_scrW / 2 - 200 * uiScale, _parent->_appConfig->_scrH / 2 - 120 * uiScale });
			ImGui::SetNextWindowSize({ 400 * uiScale, -1 });
			ImGui::OpenPopup("Sprite Sheet Setup");

			auto gridSize = anim.GridSize();
			_animGrid = { gridSize.x, gridSize.y };
			_animFCount = anim.FrameCount();
			_animFPS = anim.FPS();
			_animLoop = anim._loop;
			anim._frameSizeSetting = { anim.Size().x, anim.Size().y };
		}
		else
		{
			//closed
			SyncAnims(_animsSynced);
		}
	}

	if (ImGui::BeginPopupModal("Sprite Sheet Setup", &open))
	{
		ImGui::SetWindowSize({ 400 * uiScale, -1 });

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { uiScale * 3, uiScale * 3 });

		ImGui::Columns(2, 0, false);
		ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
		ImGui::TextWrapped("If you need help creating a sprite sheet, here's a free tool by Final Parsec:");
		ImGui::PopStyleColor();
		ImGui::NextColumn();
		if (ImGui::Button("Sprite Sheet\nTool (web link)"))
		{
			OsOpenInShell("https://www.finalparsec.com/tools/sprite_sheet_maker");
		}
		ImGui::Columns();

		ImGui::Separator();

		ImGui::PushItemWidth(120 * uiScale);

		AddResetButton("gridreset", _animGrid, { anim.GridSize().x, anim.GridSize().y }, _parent->_appConfig);
		if (ImGui::InputInt2("Sheet Columns/Rows", _animGrid.data()))
		{
			if (_animGrid[0] != anim.GridSize().x || _animGrid[1] != anim.GridSize().y)
			{
				_animFCount = _animGrid[0] * _animGrid[1];
				anim._frameSizeSetting = { -1,-1 };
			}
		}
		ToolTip("The number of rows & columns in the spritesheet", &_parent->_appConfig->_hoverTimer);

		AddResetButton("fcountreset", _animFCount, anim.FrameCount(), _parent->_appConfig);
		ImGui::InputInt("Frame Count", &_animFCount, 0, 0);
		ToolTip("The number of frames in the animation", &_parent->_appConfig->_hoverTimer);

		AddResetButton("fpsreset", _animFPS, anim.FPS(), _parent->_appConfig, nullptr, !anim.IsSynced());
		if (!anim.IsSynced())
			ImGui::InputFloat("FPS", &_animFPS, 1, 1, "%.1f");
		else
		{
			std::stringstream ss;
			ss << _animFPS;
			ImGui::PushStyleColor(ImGuiCol_Text, { 0.4,0.4,0.4,1 });
			ImGui::InputText("FPS", ss.str().data(), ss.str().length(), ImGuiInputTextFlags_ReadOnly);
			ImGui::PopStyleColor();
		}

		AddResetButton("framereset", anim._frameSizeSetting, { -1, -1 }, _parent->_appConfig);
		ImGui::InputFloat2("Frame Size (auto = [-1,-1])", &anim._frameSizeSetting.x);
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

		ImGui::Separator();

		if (LesserButton("Cancel"))
		{
			open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();

		if (ImGui::Button("Save"))
		{
			anim.SetAttributes(_animFCount, _animGrid[0], _animGrid[1], _animFPS, anim._frameSizeSetting );
			anim._loop = _animLoop;
			open = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

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
