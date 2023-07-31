
#include "LayerManager.h"
#include "file_browser_modal.h"
#include "tinyxml2\tinyxml2.h"

#include <windows.h>
//#include <shellapi.h>

void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	for (LayerInfo& layer : _layers)
	{
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
		thisLayer->SetAttribute("blinkTime", layer._blinkFrequency);
		thisLayer->SetAttribute("blinkVar", layer._blinkVariation);

		thisLayer->SetAttribute("bobType", layer._bobType);
		thisLayer->SetAttribute("bounceHeight", layer._bounceHeight);
		thisLayer->SetAttribute("bounceTime", layer._bounceFrequency);

		thisLayer->SetAttribute("bobType", layer._doBreathing);
		thisLayer->SetAttribute("bounceHeight", layer._breathHeight);
		thisLayer->SetAttribute("bounceTime", layer._breathFrequency);

		thisLayer->SetAttribute("idlePath", layer._idleImagePath.c_str());
		thisLayer->SetAttribute("talkPath", layer._talkImagePath.c_str());
		thisLayer->SetAttribute("blinkPath", layer._blinkImagePath.c_str());

		thisLayer->SetAttribute("scaleX", layer._scale.x);
		thisLayer->SetAttribute("scaleY", layer._scale.y);
		thisLayer->SetAttribute("posX", layer._pos.x);
		thisLayer->SetAttribute("posY", layer._pos.y);
		thisLayer->SetAttribute("rot", layer._rot);
		thisLayer->SetAttribute("integerPixels", layer._integerPixels);
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
		LayerInfo layer;

		layer._name = thisLayer->Attribute("name");
		thisLayer->QueryAttribute("visible", &layer._visible);

		thisLayer->QueryAttribute("talking", &layer._swapWhenTalking);
		thisLayer->QueryAttribute("talkThreshold", &layer._talkThreshold);

		thisLayer->QueryAttribute("useBlink", &layer._useBlinkFrame);
		thisLayer->QueryAttribute("blinkTime", &layer._blinkFrequency);
		thisLayer->QueryAttribute("blinkVar", &layer._blinkVariation);

		int bobtype = 0;
		thisLayer->QueryIntAttribute("bobType", &bobtype);
		layer._bobType = (LayerInfo::BobbingType)bobtype;
		thisLayer->QueryAttribute("bounceHeight", &layer._bounceHeight);
		thisLayer->QueryAttribute("bounceTime", &layer._bounceFrequency);

		thisLayer->QueryAttribute("bobType", &layer._doBreathing);
		thisLayer->QueryAttribute("bounceHeight", &layer._breathHeight);
		thisLayer->QueryAttribute("bounceTime", &layer._breathFrequency);

		layer._idleImagePath = thisLayer->Attribute("idlePath");
		layer._talkImagePath = thisLayer->Attribute("talkPath");
		layer._blinkImagePath = thisLayer->Attribute("blinkPath");

		layer._idleImage.loadFromFile(layer._idleImagePath);
		layer._talkImage.loadFromFile(layer._talkImagePath);
		layer._blinkImage.loadFromFile(layer._blinkImagePath);

		thisLayer->QueryAttribute("scaleX", &layer._scale.x);
		thisLayer->QueryAttribute("scaleY", &layer._scale.y);
		thisLayer->QueryAttribute("posX", &layer._pos.x);
		thisLayer->QueryAttribute("posY", &layer._pos.y);
		thisLayer->QueryAttribute("rot", &layer._rot);
		thisLayer->QueryAttribute("integerPixels", &layer._integerPixels);

		_layers.push_back(layer);


		_layers.back()._idleSprite.LoadFromTexture(_layers.back()._idleImage, 1, 1, 1, 1);
		_layers.back()._talkSprite.LoadFromTexture(_layers.back()._talkImage, 1, 1, 1, 1);
		_layers.back()._blinkSprite.LoadFromTexture(_layers.back()._blinkImage, 1, 1, 1, 1);

		thisLayer = thisLayer->NextSiblingElement("layer");
	}

	_lastSavedLocation = settingsFileName;
	return true;
}

void LayerManager::LayerInfo::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	SpriteSheet* activeSprite = nullptr;

	activeSprite = &_idleSprite;

	float talkFactor = talkLevel / talkMax;

	//TODO blink chance
	float blinkTime = 100;
	if (_useBlinkFrame && blinkTime == 0)
		activeSprite = &_blinkSprite;

	if (_swapWhenTalking && talkFactor > _talkThreshold)
		activeSprite = &_talkSprite;

	sf::Vector2f pos = _pos;

	switch (_bobType)
	{
	case LayerManager::LayerInfo::BobNone:
		break;
	case LayerManager::LayerInfo::BobLoudness:
		pos.y -= _bounceHeight * std::fmax(0.f, (talkFactor - _talkThreshold)/(1.0 - _talkThreshold));
		break;
	case LayerManager::LayerInfo::BobRegular:
		break;
	default:
		break;
	}

	activeSprite->setOrigin({ 0.5f*activeSprite->Size().x, 0.5f*activeSprite->Size().y });
	activeSprite->setScale(_scale);
	activeSprite->setPosition({ windowWidth / 2 + pos.x, windowHeight / 2 + pos.y });
	activeSprite->setRotation(_rot);

	activeSprite->Draw(target);

}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{

	if (_animIcon.getSize().x == 0)
		_animIcon.loadFromFile("anim.png");

	sf::Color btnColor = style.Colors[ImGuiCol_Text];

	ImGui::PushID(1000+layerID);
	std::string name = "[" + std::to_string(layerID) + "] " + _name;
	if (ImGui::CollapsingHeader(name.c_str(), name.c_str(), true, false ))
	{
		ImGui::Checkbox("Visible", &_visible);

		static imgui_ext::file_browser_modal fileBrowserIdle("Import Idle Sprite");
		static imgui_ext::file_browser_modal fileBrowserTalk("Import Talk Sprite");
		static imgui_ext::file_browser_modal fileBrowserBlink("Import Blink Sprite");

		ImGui::Columns(3, "imagebuttons", false);

		ImGui::TextColored(style.Colors[ImGuiCol_Text], "Idle");
		ImGui::PushID("idleimport");

		_importIdleOpen = ImGui::ImageButton(_idleImage, { 100,100 });
		if (fileBrowserIdle.render(_importIdleOpen, _idleImagePath))
		{
			_idleImage.loadFromFile(_idleImagePath);
			_idleSprite.LoadFromTexture(_idleImage, 1, 1, 1, 1);
		}

		ImGui::SameLine(116);
		ImGui::PushID("idleanimbtn");
		_spriteIdleOpen |= ImGui::ImageButton(_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
		AnimPopup(_idleSprite, _idleImage, _spriteIdleOpen, _oldSpriteIdleOpen);
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
			_importTalkOpen = ImGui::ImageButton(_talkImage, { 100,100 });
			fileBrowserTalk.SetStartingDir(chosenDir);
			if (fileBrowserTalk.render(_importTalkOpen, _talkImagePath))
			{
				_talkImage.loadFromFile(_talkImagePath);
				_talkSprite.LoadFromTexture(_talkImage, 1, 1, 1, 1);
			}
			
			ImGui::SameLine(116);
			ImGui::PushID("talkanimbtn");
			_spriteTalkOpen |= ImGui::ImageButton(_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			AnimPopup(_talkSprite, _talkImage, _spriteTalkOpen, _oldSpriteTalkOpen);
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
			_importBlinkOpen = ImGui::ImageButton(_blinkImage, { 100,100 });
			fileBrowserBlink.SetStartingDir(chosenDir);
			if (fileBrowserBlink.render(_importBlinkOpen, _blinkImagePath))
			{
				_blinkImage.loadFromFile(_blinkImagePath);
				_blinkSprite.LoadFromTexture(_blinkImage, 1, 1, 1, 1);
			}

			ImGui::SameLine(116);
			ImGui::PushID("blinkanimbtn");
			_spriteBlinkOpen |= ImGui::ImageButton(_animIcon, sf::Vector2f(20, 20), 0, sf::Color::Transparent, btnColor);
			AnimPopup(_blinkSprite, _blinkImage, _spriteBlinkOpen, _oldSpriteBlinkOpen);
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
			AddResetButton("talkThresh", _talkThreshold, 0.5f, &style);
			ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0);
		}
		ImGui::Separator();

		ImGui::Checkbox("Use Blink frame", &_useBlinkFrame);
		if (_useBlinkFrame)
		{
			AddResetButton("blinkfreq", _blinkFrequency, 4.f, &style);
			ImGui::SliderFloat("Blink Time", &_blinkFrequency, 0.0, 10.0);
			AddResetButton("blinkvar", _blinkVariation, 1.f, &style);
			ImGui::SliderFloat("Variation", &_blinkVariation, 0.0, 5.0);
		}
		ImGui::Separator();

		std::vector<const char*> bobOptions = { "None", "Loudness", "Regular" };
		if (ImGui::BeginCombo("Bobbing", bobOptions[_bobType]))
		{
			if (ImGui::Selectable("None", _bobType == BobNone))
				_bobType = BobNone;
			if (ImGui::Selectable("Loudness", _bobType == BobLoudness))
				_bobType = BobLoudness;
			if (ImGui::Selectable("Regular", _bobType == BobRegular))
				_bobType = BobRegular;
			ImGui::EndCombo();
		}
		if (_bobType != BobNone)
		{
			AddResetButton("bobheight", _bounceHeight, 100.f, &style);
			ImGui::SliderFloat("Bob height", &_bounceHeight, 0.0, 500.0);
		}
		ImGui::Separator();

		ImGui::Checkbox("Breathing", &_doBreathing);
		if (_doBreathing)
		{
			AddResetButton("breathheight", _breathHeight, 80.f, &style);
			ImGui::SliderFloat("Breath Height", &_breathHeight, 0.0, 500.0);
			AddResetButton("breathfreq", _breathFrequency, 2.f, &style);
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
		ImGui::Checkbox("Integer Pixels", &_integerPixels);

		ImGui::Separator();
	}
	ImGui::PopID();
}

void LayerManager::LayerInfo::AnimPopup(SpriteSheet& anim, const sf::Texture& tex, bool& open, bool& oldOpen)
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
		ImGui::InputInt2("Sheet Columns/Rows", _animGrid.data());

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
			anim.LoadFromTexture(tex, _animFCount, _animGrid[0], _animGrid[1], _animFPS, { _animFrameSize[0], _animFrameSize[1] });
			open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor(4);

		ImGui::PopItemWidth();
	
		ImGui::EndPopup();
	}
}
