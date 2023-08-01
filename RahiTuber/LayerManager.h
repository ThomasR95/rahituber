#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

#include "SpriteSheet.h"

#include "tinyxml2\tinyxml2.h"

class TextureManager
{
public:
	inline sf::Texture* GetTexture(const std::string& path)
	{
		if (_textures.count(path))
		{
			return &_textures[path];
		}
		else
		{
			_textures[path].loadFromFile(path);
			return &_textures[path];
		}
	}

private:
	std::map<std::string, sf::Texture> _textures;
};

class LayerManager
{
public:

	struct LayerInfo 
	{
		LayerManager* _parent = nullptr;

		bool _visible = true;
		std::string _name = "Layer";

		bool _swapWhenTalking = true;
		float _talkThreshold = 0.15f;

		bool _useBlinkFrame = true;
		float _blinkDuration = 0.2;
		float _blinkDelay = 6.0;
		float _blinkVariation = 4.0;
		sf::Clock _blinkTimer;
		bool _isBlinking = false;
		float _blinkVarDelay = 0;

		enum BounceType
		{
			BounceNone = 0,
			BounceLoudness = 1,
			BounceRegular = 2,
		};

		BounceType _bounceType = BounceLoudness;
		float _bounceHeight = 80;
		float _bounceFrequency = 0.333;
		bool _isBouncing = false;

		bool _doBreathing = true;
		float _breathHeight = 30;
		float _breathFrequency = 4.0;
		bool _isBreathing = false;
		float _breathAmount = 0;

		float _motionHeight = 0;
		sf::Clock _motionTimer;

		std::string _idleImagePath = "";
		sf::Texture* _idleImage = nullptr;

		std::string _talkImagePath = "";
		sf::Texture* _talkImage = nullptr;

		std::string _blinkImagePath = "";
		sf::Texture* _blinkImage = nullptr;

		SpriteSheet _idleSprite;
		SpriteSheet _talkSprite;
		SpriteSheet _blinkSprite;

		sf::Vector2f _scale = { 1.f, 1.f };
		sf::Vector2f _pos;
		float _rot = 0.0;
		bool _keepAspect = true;

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

		void AnimPopup(SpriteSheet& anim, bool& open, bool& oldOpen);

	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawGUI(ImGuiStyle& style, float maxHeight);

	void AddLayer();
	void RemoveLayer(int toRemove);
	void MoveLayerUp(int moveUp);
	void MoveLayerDown(int moveDown);

	void RemoveLayer(LayerInfo* toRemove);
	void MoveLayerUp(LayerInfo* moveUp);
	void MoveLayerDown(LayerInfo* moveDown);

	bool SaveLayers(const std::string& settingsFileName);
	bool LoadLayers(const std::string& settingsFileName);

private:

	std::vector<LayerInfo> _layers;

	std::string _lastSavedLocation = "";

	inline void SaveAnimInfo(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* animName, const SpriteSheet& anim)
	{
		auto animElement = parent->FirstChildElement(animName);
		if (!animElement)
			animElement = parent->InsertFirstChild(doc->NewElement(animName))->ToElement();

		animElement->SetAttribute("gridX", anim.GridSize().x);
		animElement->SetAttribute("gridY", anim.GridSize().y);
		animElement->SetAttribute("frameW", anim.Size().x);
		animElement->SetAttribute("frameH", anim.Size().y);
		animElement->SetAttribute("fps", anim.FPS());
		animElement->SetAttribute("fCount", anim.FrameCount());
	}

	inline void LoadAnimInfo(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* animName, SpriteSheet& anim)
	{
		auto animElement = parent->FirstChildElement(animName);
		if (!animElement)
			return;

		std::vector<int> grid = { 1,1 };
		std::vector<int> frame = { -1,-1 };
		float fps;
		int fCount;
		animElement->QueryAttribute("gridX", &grid[0]);
		animElement->QueryAttribute("gridY", &grid[1]);
		animElement->QueryAttribute("frameW", &frame[0]);
		animElement->QueryAttribute("frameH", &frame[1]);
		animElement->QueryAttribute("fps", &fps);
		animElement->QueryAttribute("fCount",&fCount);

		anim.SetAttributes(fCount, grid[0], grid[1], fps, { frame[0], frame[1] });
	}

};

static sf::Texture* _resetIcon = nullptr;
static sf::Texture* _emptyIcon = nullptr;
static sf::Texture* _animIcon = nullptr;

static TextureManager _textureMan;

template <typename T>
inline void AddResetButton(const char* id, T& value, T resetValue, ImGuiStyle* style = nullptr)
{
	if (_resetIcon == nullptr)
		_resetIcon = _textureMan.GetTexture("reset.png");

	sf::Color btnColor = sf::Color::White;
	if (style)
		btnColor = style->Colors[ImGuiCol_Text];

	ImGui::PushID(id);
	if (ImGui::ImageButton(*_resetIcon, sf::Vector2f(13, 13), -1, sf::Color::Transparent, btnColor))
		value = resetValue;
	ImGui::PopID();
	ImGui::SameLine();
}

inline float GetRandom01()
{
	return (0.002 * (rand() % 500));
}
inline float GetRandom11()
{
	return (0.002 * (rand() % 500 - 250));
}