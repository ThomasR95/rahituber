#include "LayerManager.h"

void LayerManager::Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
	for (LayerInfo& layer : _layers)
	{
		if(layer._visible)
			layer.Draw(windowHeight, windowWidth, talkLevel, talkMax);
	}
}

void LayerManager::DrawGUI(ImGuiStyle& style)
{
	ImGui::PushID("layermanager");

	if (ImGui::Button("Add Layer"))
		AddLayer();

	ImGui::Separator();

	ImGui::BeginChildFrame(ImGuiID(10001), ImVec2(-1, -1), ImGuiWindowFlags_AlwaysVerticalScrollbar);

	for (auto& layer : _layers)
	{
		layer.DrawGUI(style);
	}

	ImGui::EndChildFrame();

	ImGui::PopID();
}

void LayerManager::AddLayer()
{
	_layers.push_back(LayerInfo());
}

void LayerManager::LayerInfo::Draw(float windowHeight, float windowWidth, float talkLevel, float talkMax)
{
}

void LayerManager::LayerInfo::DrawGUI(ImGuiStyle& style)
{
	if (ImGui::CollapsingHeader(_name.c_str(), _name.c_str(), true, false ))
	{
		ImGui::Checkbox("Visible", &_visible);
		ImGui::Checkbox("Swap when Talking", &_swapWhenTalking);
		ImGui::SliderFloat("Talk Threshold", &_talkThreshold, 0.0, 1.0);
	}
}
