#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

#include "SpriteSheet.h"

#include "tinyxml2\tinyxml2.h"

#include <deque>

#include "Config.h"

#include <filesystem>
namespace fs = std::filesystem;

static std::map<std::string, sf::BlendMode> g_blendmodes = {
	{"Normal", sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add,
												sf::BlendMode::One, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add)},
	{"Lighten", sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::Max,
												sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::ReverseSubtract)},
	{"Darken", sf::BlendMode(sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::One, sf::BlendMode::Min,
												sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::ReverseSubtract)},
	{"Add", sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::Add,
												sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::ReverseSubtract)},
	{"Multiply", sf::BlendMode(sf::BlendMode::DstColor, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add,
												     sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::ReverseSubtract)},
	{"Subtract", sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::One, sf::BlendMode::ReverseSubtract,
												sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::ReverseSubtract)},
	{"Overwrite", sf::BlendMode(sf::BlendMode::One, sf::BlendMode::Zero, sf::BlendMode::Add,
												sf::BlendMode::One, sf::BlendMode::Zero, sf::BlendMode::Add)},
	{"Erase", sf::BlendMode(sf::BlendMode::Zero, sf::BlendMode::One, sf::BlendMode::Add,
												sf::BlendMode::Zero, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add)},
};

class TextureManager
{
public:

	sf::Texture* GetTexture(const std::string& path);

	void Reset();

private:
	std::map<std::string, sf::Texture*> _textures;
};

class LayerManager
{
public:

	~LayerManager();

	struct LayerInfo 
	{
		std::string _id = "";

		LayerManager* _parent = nullptr;

		bool _visible = true;
		std::string _name = "Layer";

		bool _swapWhenTalking = true;
		float _talkThreshold = 0.15f;

		bool _useBlinkFrame = true;
		bool _blinkWhileTalking = false;
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
		float _idleTint[4] = { 1,1,1,1 };

		std::string _talkImagePath = "";
		sf::Texture* _talkImage = nullptr;
		float _talkTint[4] = { 1,1,1,1 };

		std::string _blinkImagePath = "";
		sf::Texture* _blinkImage = nullptr;
		float _blinkTint[4] = { 1,1,1,1 };

		std::string _talkBlinkImagePath = "";
		sf::Texture* _talkBlinkImage = nullptr;
		float _talkBlinkTint[4] = { 1,1,1,1 };

		std::shared_ptr<SpriteSheet> _idleSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _talkSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _blinkSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _talkBlinkSprite = std::make_shared<SpriteSheet>();

		SpriteSheet* _activeSprite = nullptr;

		sf::Vector2f _scale = { 1.f, 1.f };
		sf::Vector2f _pos;
		float _rot = 0.0;
		bool _keepAspect = true;

		sf::BlendMode _blendMode = g_blendmodes["Normal"];

		bool _scaleFiltering = false;

		bool _importIdleOpen = false;
		bool _importTalkOpen = false;
		bool _importBlinkOpen = false;
		bool _importTalkBlinkOpen = false;

		bool _spriteIdleOpen = false;
		bool _spriteTalkOpen = false;
		bool _spriteBlinkOpen = false;
		bool _spriteTalkBlinkOpen = false;
		bool _oldSpriteIdleOpen = false;
		bool _oldSpriteTalkOpen = false;
		bool _oldSpriteBlinkOpen = false;
		bool _oldSpriteTalkBlinkOpen = false;
		bool _renamePopupOpen = false;
		std::string _renamingString = "";

		void CalculateDraw(float windowHeight, float windowWidth, float talkLevel, float talkMax);

		void DrawGUI(ImGuiStyle& style, int layerID);

		std::vector<int> _animGrid = { 1, 1 };
		int _animFCount = 1;
		float _animFPS = 12;
		std::vector<float> _animFrameSize = { -1, -1 };

		bool _animsSynced = false;

		void AnimPopup(SpriteSheet& anim, bool& open, bool& oldOpen);

		void SyncAnims(bool sync);

		std::string _motionParent = "";
		float _motionDelay = 0;
		struct MotionLinkData
		{
			sf::Vector2f _scale = { 1.f, 1.f };
			sf::Vector2f _pos = { 0,0 };
			float _rot = 0.0;
		};
		bool _hideWithParent = true;

		std::deque<MotionLinkData> _motionLinkData;

		float _lastTalkFactor = 0.0;

	};

	struct StatesInfo
	{
		bool _active = false;

		sf::Keyboard::Key _key = sf::Keyboard::Unknown;
		bool _ctrl = false;
		bool _shift = false;
		bool _alt = false;

		enum State {
			Hide = 0,
			Show = 1,
			NoChange = 2
		};

		float _timeout = 5.0;
		bool _useTimeout = true;
		bool _toggle = true;
		bool _timedInterval = false;
		float _intervalTime = 2.0;
		float _intervalVariation = 0.0;
		float _currentIntervalTime = 0.0;
		std::map<std::string, State> _layerStates;
		bool _awaitingHotkey = false;

		sf::Clock _timer;
	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawGUI(ImGuiStyle& style, float maxHeight);

	void AddLayer(const LayerInfo* toCopy = nullptr);
	void RemoveLayer(int toRemove);
	void MoveLayerUp(int moveUp);
	void MoveLayerDown(int moveDown);

	void RemoveLayer(LayerInfo* toRemove);
	void MoveLayerUp(LayerInfo* moveUp);
	void MoveLayerDown(LayerInfo* moveDown);

	bool SaveLayers(const std::string& settingsFileName);
	bool LoadLayers(const std::string& settingsFileName);

	bool PendingHotkey() { return _waitingForHotkey; }
	void SetHotkeys(const sf::Keyboard::Key& key, bool ctrl, bool shift, bool alt)
	{
		_pendingKey = key;
		_pendingCtrl = ctrl;
		_pendingShift = shift;
		_pendingAlt = alt;
	}
	void HandleHotkey(const sf::Keyboard::Key& key, bool ctrl, bool shift, bool alt);
	void ResetStates();

	LayerInfo* GetLayer(std::string id) 
	{
		for (auto& layer : _layers)
			if (layer._id == id)
				return &layer;

		return nullptr;
	}

	const std::vector<LayerInfo>& GetLayers()
	{
		return _layers;
	}
	
	std::string LastUsedLayerSet() { return _loadedXML; }
	void SetLayerSet(const std::string& xmlName) { _loadedXML = xmlName; }

	AppConfig* _appConfig = nullptr;

private:

	std::vector<StatesInfo> _states;

	std::vector<LayerInfo> _layers;

	sf::RenderTexture _blendingRT;

	std::string _lastSavedLocation = "";
	std::string _loadedXML = "lastLayers";
	std::string _loadedXMLPath = "";
	std::string _fullLoadedXMLPath = "";
	bool _loadedXMLExists = true;
	bool _loadXMLOpen = false;

	bool _statesMenuOpen = false;
	bool _oldStatesMenuOpen = false;
	bool _waitingForHotkey = false;
	sf::Keyboard::Key _pendingKey = sf::Keyboard::Unknown;
	bool _pendingCtrl = false;
	bool _pendingShift = false;
	bool _pendingAlt = false;

	sf::Vector2f _globalScale = { 1.f, 1.f };
	sf::Vector2f _globalPos;
	float _globalRot = 0.0;
	bool _globalKeepAspect = true;

	std::map<std::string, bool> _defaultLayerStates;
	std::vector<StatesInfo*> _statesOrder;
	sf::Clock _statesTimer;
	bool _statesDirty = false;
	void DrawStatesGUI();

	std::string _errorMessage = "";

	void AppendStateToOrder(StatesInfo* state)
	{
		for (StatesInfo* searchKey : _statesOrder)
			if (state == searchKey)
				return;

		_statesOrder.push_back(state);
	}

	void RemoveStateFromOrder(StatesInfo* state)
	{
		auto stateIt = _statesOrder.begin();
		while (stateIt != _statesOrder.end())
		{
			if (state == *stateIt)
			{
				_statesOrder.erase(stateIt);
				break;
			}
			stateIt++;
		}
	}

	void SaveDefaultStates()
	{
		bool anyActive = AnyStateActive();
		AnyStateActive();
		if (!anyActive)
		{
			for (auto& l : _layers)
			{
				_defaultLayerStates[l._id] = l._visible;
			}
		}
	}

	bool AnyStateActive()
	{
		for (auto& state : _states)
			if (state._active)
				return true;

		_statesOrder.clear();
		return false;
	}

	inline void SaveColor(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* colorName, const float* col)
	{
		auto colElement = parent->FirstChildElement(colorName);
		if (!colElement)
			colElement = parent->InsertFirstChild(doc->NewElement(colorName))->ToElement();

		colElement->SetAttribute("r", col[0]);
		colElement->SetAttribute("g", col[1]);
		colElement->SetAttribute("b", col[2]);
		colElement->SetAttribute("a", col[3]);
	}

	inline void LoadColor(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* colorName, float* col)
	{
		auto colElement = parent->FirstChildElement(colorName);
		if (!colElement)
			return;

		colElement->QueryAttribute("r", &col[0]);
		colElement->QueryAttribute("g", &col[1]);
		colElement->QueryAttribute("b", &col[2]);
		colElement->QueryAttribute("a", &col[3]);
	}

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
		std::vector<float> frame = { -1,-1 };
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
static sf::Texture* _upIcon = nullptr;
static sf::Texture* _dnIcon = nullptr;
static sf::Texture* _editIcon = nullptr;
static sf::Texture* _delIcon = nullptr;
static sf::Texture* _dupeIcon = nullptr;


static TextureManager _textureMan;

template <typename T>
inline void AddResetButton(const char* id, T& value, T resetValue, ImGuiStyle* style = nullptr, bool enabled = true)
{
	if (_resetIcon == nullptr)
		_resetIcon = _textureMan.GetTexture("res/reset.png");

	ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		if(style)
		col = style->Colors[ImGuiCol_Text];

	sf::Color btnColor = { sf::Uint8(255 * col.x), sf::Uint8(255 * col.y), sf::Uint8(255 * col.z) };

	ImGui::PushID(id);
	if (ImGui::ImageButton(*_resetIcon, sf::Vector2f(13, 13), -1, sf::Color::Transparent, btnColor))
		if(enabled)
			value = resetValue;
	ImGui::PopID();
	ImGui::SameLine();
}

inline float GetRandom01()
{
	return (0.002f * (rand() % 500));
}
inline float GetRandom11()
{
	return (0.002f * (rand() % 500 - 250));
}