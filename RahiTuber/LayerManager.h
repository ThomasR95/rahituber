#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

#include "SpriteSheet.h"

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

		SpriteSheet _idleSprite;
		SpriteSheet _talkSprite;
		SpriteSheet _blinkSprite;

		sf::Vector2f _scale = { 1.f, 1.f };
		sf::Vector2f _pos;
		float _rot = 0.0;
		bool _keepAspect = true;
		bool _integerPixels = false;

		bool _importIdleOpen = false;
		bool _importTalkOpen = false;
		bool _importBlinkOpen = false;

		bool _spriteIdleOpen = false;
		bool _spriteTalkOpen = false;
		bool _spriteBlinkOpen = false;
		bool _oldSpriteIdleOpen = false;
		bool _oldSpriteTalkOpen = false;
		bool _oldSpriteBlinkOpen = false;

		void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

		void DrawGUI(ImGuiStyle& style, int layerID);


		std::vector<int> _animGrid = { 1, 1 };
		int _animFCount = 1;
		float _animFPS = 12;
		std::vector<int> _animFrameSize = { -1, -1 };

		void AnimPopup(SpriteSheet& anim, const sf::Texture& tex, bool& open, bool& oldOpen);
	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawGUI(ImGuiStyle& style, float maxHeight);

	void AddLayer();
	void RemoveLayer(int toRemove);

	bool SaveLayers(const std::string& settingsFileName);
	bool LoadLayers(const std::string& settingsFileName);

	void SelectLayer(int layer);
	int GetSelectedLayer() { return _selectedLayer; }

private:

	std::vector<LayerInfo> _layers;

	int _selectedLayer;

	std::string _lastSavedLocation = "";

};

static sf::Texture _resetIcon;

static sf::Texture _animIcon;

template <typename T>
inline void AddResetButton(const char* id, T& value, T resetValue, ImGuiStyle* style = nullptr)
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

