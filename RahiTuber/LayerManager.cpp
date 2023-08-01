#include "LayerManager.h"
#include "file_browser_modal.h"

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

void LayerManager::LayerInfo::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	_sprite.setTexture(_idleImage, true);

	float talkFactor = talkLevel / talkMax;

	//TODO blink chance
	float blinkTime = 100;
	if (_useBlinkFrame && blinkTime == 0)
		_sprite.setTexture(_blinkImage, true);

	if (_swapWhenTalking && talkFactor > _talkThreshold)
		_sprite.setTexture(_talkImage, true);

	_sprite.setOrigin(_sprite.getTextureRect().width / 2, _sprite.getTextureRect().height / 2);
	_sprite.setScale(_scale);
	_sprite.setPosition(windowWidth / 2 + _pos.x, windowHeight / 2 + _pos.y);
	_sprite.setRotation(_rot);

	target->draw(_sprite);

}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style, int layerID)
{
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
			_idleImage.loadFromFile(_idleImagePath);

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
				_talkImage.loadFromFile(_talkImagePath);

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
				_blinkImage.loadFromFile(_blinkImagePath);
			
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


		float pos[2] = { _pos.x, _pos.y };
		if (ImGui::SliderFloat2("Position", pos, 0.0, 1000.f))
		{
			_pos.x = pos[0];
			_pos.y = pos[1];
		}

		ImGui::SliderFloat("Rotation", &_rot, -180.f, 180.f);

		float scale[2] = {_scale.x, _scale.y};
		if (ImGui::SliderFloat2("Scale", scale, 0.0, 10.f))
		{
			_scale.x = scale[0];
			_scale.y = scale[1];
		}
		ImGui::Checkbox("Integer Pixels", &_integerPixels);

		ImGui::Separator();
	}
	ImGui::PopID();
}
