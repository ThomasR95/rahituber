#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

class LayerManager
{
public:

	struct LayerInfo 
	{
		bool _visible = true;
		std::string _name = "Layer";

		bool _swapWhenTalking = true;
		float _talkThreshold = 0.5f;


		bool _useBlinkFrame = true;

		float _blinkFrequency = 4.0;
		float _blinkVariation = 1.0;

		enum BobbingType
		{
			BobNone = 0,
			BobLoudness = 1,
			BobRegular = 2,
		};

		BobbingType _bobType = BobLoudness;
		float _bounceHeight = 100;
		float _bounceFrequency = 0.6;

		bool _doBreathing = true;
		float _breathHeight = 80;
		float _breathFrequency = 2.0;

		std::string _idleImagePath = "";
		sf::Texture _idleImage;

		std::string _talkImagePath = "";
		sf::Texture _talkImage;

		std::string _blinkImagePath = "";
		sf::Texture _blinkImage;

		sf::Sprite _sprite;

		sf::Vector2f _scale = { 1.f, 1.f };
		sf::Vector2f _pos;
		float _rot = 0.0;
		bool _integerPixels = false;

		bool _importIdleOpen = false;
		bool _importTalkOpen = false;
		bool _importBlinkOpen = false;

		void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

		void DrawGUI(ImGuiStyle& style, int layerID);
	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawGUI(ImGuiStyle& style, float maxHeight);

	void AddLayer();
	void RemoveLayer(int toRemove);

	void SaveLayers();

	void SelectLayer(int layer);
	int GetSelectedLayer() { return _selectedLayer; }

private:

	std::vector<LayerInfo> _layers;

	int _selectedLayer;

};

static sf::Texture _resetIcon;

inline void AddResetButton(const char* id, float& value, float resetValue, ImGuiStyle* style = nullptr)
{
	if(_resetIcon.getSize().x == 0)
		_resetIcon.loadFromFile("reset.png");

	sf::Color btnColor = sf::Color::White;
	if (style)
		btnColor = style->Colors[ImGuiCol_Text];

	ImGui::PushID(id);
	if (ImGui::ImageButton(_resetIcon, sf::Vector2f(13, 13), -1, sf::Color::Transparent, btnColor))
		value = resetValue;
	ImGui::PopID();
	ImGui::SameLine();
}