
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2\tinyxml2.h"

#include "defines.h"

#include <windows.h>
//#include <shellapi.h>

void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	for (int l = _layers.size()-1; l >= 0; l--)
	{
		LayerInfo& layer = _layers[l];
		if(layer._visible)
			layer.Draw(target, windowHeight, windowWidth, talkLevel, talkMax);
	}
}

void LayerManager::DrawGUI(ImGuiStyle& style, float maxHeight)
{
	float topBarBegin = ImGui::GetCursorPosY();

	ImGui::PushID("layermanager");

	if (ImGui::Button("Add Layer"))
		AddLayer();

	ImGui::SameLine();
	if (ImGui::Button("Remove All"))
		_layers.clear();

	ImGui::SameLine();
	if (ImGui::Button("Save Layers") && !_lastSavedLocation.empty())
		SaveLayers(_lastSavedLocation);

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

	_layers.back()._blinkTimer.restart();
	_layers.back()._isBlinking = false;
	_layers.back()._blinkVarDelay = GetRandom11() * _layers.back()._blinkVariation;
	_layers.back()._parent = this;
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

	auto lastLayers = root->FirstChildElement("lastLayers");
	if (!lastLayers) 
		lastLayers = root->InsertFirstChild(doc.NewElement("lastLayers"))->ToElement();

	if (!lastLayers)
		return false;

	lastLayers->DeleteChildren();

	for (const auto& layer : _layers)
	{
		auto thisLayer = lastLayers->FirstChildElement("layer");
		if (!thisLayer)
			thisLayer = lastLayers->InsertFirstChild(doc.NewElement("layer"))->ToElement();

		thisLayer->SetAttribute("name", layer._name.c_str());
		thisLayer->SetAttribute("visible", layer._visible);

		thisLayer->SetAttribute("talking", layer._swapWhenTalking);
		thisLayer->SetAttribute("talkThreshold", layer._talkThreshold);

		thisLayer->SetAttribute("useBlink", layer._useBlinkFrame);
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

		if (layer._idleSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "idleAnim", layer._idleSprite);

		if (layer._talkSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "talkAnim", layer._talkSprite);

		if (layer._blinkSprite.FrameCount() > 1)
			SaveAnimInfo(thisLayer, &doc, "blinkAnim", layer._blinkSprite);

		thisLayer->SetAttribute("scaleX", layer._scale.x);
		thisLayer->SetAttribute("scaleY", layer._scale.y);
		thisLayer->SetAttribute("posX", layer._pos.x);
		thisLayer->SetAttribute("posY", layer._pos.y);
		thisLayer->SetAttribute("rot", layer._rot);
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

	auto lastLayers = root->FirstChildElement("lastLayers");
	if (!lastLayers)
		return false;

	_layers.clear();

	auto thisLayer = lastLayers->FirstChildElement("layer");
	while (thisLayer)
	{
		_layers.emplace_back(LayerInfo());

		LayerInfo& layer = _layers.back();

		layer._parent = this;

		layer._name = thisLayer->Attribute("name");
		thisLayer->QueryAttribute("visible", &layer._visible);

		thisLayer->QueryAttribute("talking", &layer._swapWhenTalking);
		thisLayer->QueryAttribute("talkThreshold", &layer._talkThreshold);

		thisLayer->QueryAttribute("useBlink", &layer._useBlinkFrame);
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

		layer._idleImagePath = thisLayer->Attribute("idlePath");
		layer._talkImagePath = thisLayer->Attribute("talkPath");
		layer._blinkImagePath = thisLayer->Attribute("blinkPath");

		layer._idleImage = _textureMan.GetTexture(layer._idleImagePath);
		layer._talkImage = _textureMan.GetTexture(layer._talkImagePath);
		layer._blinkImage = _textureMan.GetTexture(layer._blinkImagePath);

		layer._idleSprite.LoadFromTexture(*layer._idleImage, 1, 1, 1, 1);
		layer._talkSprite.LoadFromTexture(*layer._talkImage, 1, 1, 1, 1);
		layer._blinkSprite.LoadFromTexture(*layer._blinkImage, 1, 1, 1, 1);

		LoadAnimInfo(thisLayer, &doc, "idleAnim", layer._idleSprite);
		LoadAnimInfo(thisLayer, &doc, "talkAnim", layer._talkSprite);
		LoadAnimInfo(thisLayer, &doc, "blinkAnim", layer._blinkSprite);

		layer._blinkTimer.restart();
		layer._isBlinking = false;
		layer._blinkVarDelay = GetRandom11() * layer._blinkVariation;

		thisLayer->QueryAttribute("scaleX", &layer._scale.x);
		thisLayer->QueryAttribute("scaleY", &layer._scale.y);
		thisLayer->QueryAttribute("posX", &layer._pos.x);
		thisLayer->QueryAttribute("posY", &layer._pos.y);
		thisLayer->QueryAttribute("rot", &layer._rot);

		thisLayer = thisLayer->NextSiblingElement("layer");
	}

	_lastSavedLocation = settingsFileName;
	return true;
}

void LayerManager::LayerInfo::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	SpriteSheet* activeSprite = nullptr;

	if (!_idleImage)
		return;

	activeSprite = &_idleSprite;

	float talkFactor = talkLevel / talkMax;
	talkFactor = pow(talkFactor, 0.5);

	bool talking = talkFactor > _talkThreshold;


	if (_blinkImage && !talking && !_isBlinking && _blinkTimer.getElapsedTime().asSeconds() > _blinkDelay + _blinkVarDelay)
	{
		_isBlinking = true;
		_blinkTimer.restart();
		_blinkSprite.Restart();
		_blinkVarDelay = GetRandom11() * _blinkVariation;
	}

	if (_blinkImage && _isBlinking)
	{
		activeSprite = &_blinkSprite;

		if (_blinkTimer.getElapsedTime().asSeconds() > _blinkDuration)
			_isBlinking = false;
	}

	if (_talkImage && !_isBlinking && _swapWhenTalking && talking)
		activeSprite = &_talkSprite;

	sf::Vector2f pos = _pos;

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

		if (!talking && !_isBouncing && _breathFrequency > 0)
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

		float origHeight = activeSprite->Size().y * _scale.y;
		float breathHeight = origHeight + _breathAmount * (_breathHeight / 2);

		breathScale = breathHeight / origHeight;
	}

	_motionHeight += (newMotionHeight - _motionHeight) * 0.3;

	pos.y -= _motionHeight;

	activeSprite->setOrigin({ 0.5f*activeSprite->Size().x, 0.5f*activeSprite->Size().y });
	activeSprite->setScale(_scale * breathScale);
	activeSprite->setPosition({ windowWidth / 2 + pos.x, windowHeight / 2 + pos.y });
	activeSprite->setRotation(_rot);

	activeSprite->Draw(target);

}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{

	if (_animIcon == nullptr)
		_animIcon = _textureMan.GetTexture("anim.png");


	if (_emptyIcon == nullptr)
		_emptyIcon = _textureMan.GetTexture("empty.png");

	sf::Color btnColor = style.Colors[ImGuiCol_Text];

	ImGui::PushID(1000+layerID);
	std::string name = "[" + std::to_string(layerID) + "] " + _name;
	if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed ))
	{
		ImGui::Columns(4, 0, false);
		ImGui::Checkbox("Visible", &_visible);
		ImGui::NextColumn();
		if (ImGui::Button("Move Up"))
			_parent->MoveLayerUp(this);
		ImGui::NextColumn();
		if (ImGui::Button("Move Down"))
			_parent->MoveLayerDown(this);
		ImGui::NextColumn();
		if (ImGui::Button("Delete"))
			_parent->RemoveLayer(this);
		ImGui::Columns();

		ImGui::Separator();

		static imgui_ext::file_browser_modal fileBrowserIdle("Import Idle Sprite");
		static imgui_ext::file_browser_modal fileBrowserTalk("Import Talk Sprite");
		static imgui_ext::file_browser_modal fileBrowserBlink("Import Blink Sprite");

		ImGui::Columns(3, "imagebuttons", false);

		ImGui::TextColored(style.Colors[ImGuiCol_Text], "Idle");
		ImGui::PushID("idleimport");

		sf::Texture* idleIcon = _idleImage == nullptr ? _emptyIcon : _idleImage;
		_importIdleOpen = ImGui::ImageButton(*idleIcon, { 100,100 });
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

		ImGui::PopID();

		ImGui::NextColumn();

		if (_swapWhenTalking)
		{
			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Talk");
			ImGui::PushID("talkimport");
			sf::Texture* talkIcon = _talkImage == nullptr ? _emptyIcon : _talkImage;
			_importTalkOpen = ImGui::ImageButton(*talkIcon, { 100,100 });
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

			ImGui::PopID();
		}

		ImGui::NextColumn();
		
		if (_useBlinkFrame)
		{
			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Blink");
			ImGui::PushID("blinkimport");
			sf::Texture* blinkIcon = _blinkImage == nullptr ? _emptyIcon : _blinkImage;
			_importBlinkOpen = ImGui::ImageButton(*blinkIcon, { 100,100 });
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

			ImGui::PopID();

			
		}
		
		ImGui::Columns();

		ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);
		if (_swapWhenTalking)
		{
			AddResetButton("talkThresh", _talkThreshold, 0.15f, &style);
			ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0, "%.3f", 2.f);
		}
		ImGui::Separator();

		ImGui::Checkbox("Use Blink frame", &_useBlinkFrame);
		if (_useBlinkFrame)
		{
			AddResetButton("blinkdur", _blinkDuration, 0.2f, &style);
			ImGui::SliderFloat("Blink Duration", &_blinkDuration, 0.0, 10.0);
			AddResetButton("blinkdelay", _blinkDelay, 6.f, &style);
			ImGui::SliderFloat("Blink Delay", &_blinkDelay, 0.0, 10.0);
			AddResetButton("blinkvar", _blinkVariation, 4.f, &style);
			ImGui::SliderFloat("Variation", &_blinkVariation, 0.0, 5.0);
		}
		ImGui::Separator();

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
			AddResetButton("bobtime", _bounceFrequency, 0.333f, &style);
			ImGui::SliderFloat("Bounce time", &_bounceFrequency, 0.0, 2.0);
		}
		ImGui::Separator();

		ImGui::Checkbox("Breathing", &_doBreathing);
		if (_doBreathing)
		{
			AddResetButton("breathheight", _breathHeight, 30.f, &style);
			ImGui::SliderFloat("Breath Height", &_breathHeight, 0.0, 500.0);
			AddResetButton("breathfreq", _breathFrequency, 4.f, &style);
			ImGui::SliderFloat("Breath Time", &_breathFrequency, 0.0, 10.f);
		}
		ImGui::Separator();

		AddResetButton("pos", _pos, sf::Vector2f(0.0, 0.0));
		float pos[2] = { _pos.x, _pos.y };
		if (ImGui::SliderFloat2("Position", pos, -1000.0, 1000.f))
		{
			_pos.x = pos[0];
			_pos.y = pos[1];
		}

		AddResetButton("rot", _rot, 0.f);
		ImGui::SliderFloat("Rotation", &_rot, -180.f, 180.f);

		AddResetButton("scale", _scale, sf::Vector2f(1.0, 1.0));
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
			ImGui::SetNextWindowSize({ 400, 200 });
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

		AddResetButton("fpsreset", _animFPS, anim.FPS());
		ImGui::InputFloat("FPS", &_animFPS);

		AddResetButton("framereset", _animFrameSize, { -1, -1 });
		ImGui::InputInt2("Frame Size (auto = [-1,-1])", _animFrameSize.data());

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
