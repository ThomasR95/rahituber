#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

#include "imgui.h"
#include "imgui-SFML.h"

#include "SpriteSheet.h"

#include "tinyxml2/tinyxml2.h"

#include <deque>

#include "Config.h"

#include "ChatReader.h"

#include "TextureManager.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <thread>

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


enum MotionStretchType {
	MS_None,
	MS_Linear,
	MS_PreserveVolume,
	//MS_Circular,
	MotionStretch_End
};

static const char* const g_motionStretchNames[MotionStretch_End] = {
	"None", "Linear", "Preserve Volume"//, "Circular"
};

class LayerManager
{
public:

	LayerManager() {}

	~LayerManager();

	

	struct LayerInfo 
	{
		std::string _id = "";

		LayerManager* _parent = nullptr;

		bool _isFolder = false;
		std::string _inFolder = "";
		std::vector<std::string> _folderContents;

		bool _visible = true;
		bool _oldVisible = false;
		std::string _name = "Layer";

		bool _swapWhenTalking = false;
		float _talkThreshold = 0.15f;
		bool _restartTalkAnim = false;
		bool _wasTalking = false;
		bool _smoothTalkTint = false;

		bool _useBlinkFrame = false;
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

		BounceType _bounceType = BounceNone;
		sf::Vector2f _bounceMove = { 0.f, 80.f };
		float _bounceRotation = 0.0;
		sf::Vector2f _bounceScale = { 0.0, 0.0 };
		bool _bounceScaleConstrain = true;
		float _bounceFrequency = 0.333;
		bool _isBouncing = false;

		bool _doBreathing = false;
		float _breathFrequency = 4.0;
		bool _isBreathing = false;
		sf::Vector2f _breathAmount = { 0.0f, 0.0f };
		sf::Vector2f _breathScale = { 0.1, 0.1 };
		sf::Vector2f _breathMove = { 0.0, 30.0 };
		float _breathRotation = 0.0;
		ImVec4 _breathTint = { 1,1,1,1 };
		bool _doBreathTint = false;
		bool _breathScaleConstrain = true;
		bool _breathCircular = false;
		bool _breatheWhileTalking = false;

		float _motionX = 0;
		float _motionY = 0;
		sf::Clock _motionTimer;

		bool _scream = false;
		float _screamThreshold = 0.85;
		bool _isScreaming = false;
		bool _screamVibrate = true;
		float _screamVibrateAmount = 5;
		float _screamVibrateSpeed = 1;

		std::string _idleImagePath = u8"";
		sf::Texture* _idleImage = nullptr;
		ImVec4 _idleTint = { 1,1,1,1 };

		std::string _talkImagePath = u8"";
		sf::Texture* _talkImage = nullptr;
		ImVec4 _talkTint = { 1,1,1,1 };

		std::string _blinkImagePath = u8"";
		sf::Texture* _blinkImage = nullptr;
		ImVec4 _blinkTint = { 1,1,1,1 };

		std::string _talkBlinkImagePath = u8"";
		sf::Texture* _talkBlinkImage = nullptr;
		ImVec4 _talkBlinkTint = { 1,1,1,1 };

		std::string _screamImagePath = u8"";
		sf::Texture* _screamImage = nullptr;
		ImVec4 _screamTint = { 1,1,1,1 };

		std::shared_ptr<SpriteSheet> _idleSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _talkSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _blinkSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _talkBlinkSprite = std::make_shared<SpriteSheet>();
		std::shared_ptr<SpriteSheet> _screamSprite = std::make_shared<SpriteSheet>();


		SpriteSheet* _activeSprite = nullptr;

		sf::Vector2f _scale = { 1.f, 1.f };
		sf::Vector2f _pos;
		float _rot = 0.0;
		bool _keepAspect = true;

		sf::Vector2f _pivot = { 0.5f, 0.5f };
		bool _pivotPx = false;

		sf::BlendMode _blendMode = g_blendmodes["Normal"];

		bool _scaleFiltering = false;

		bool _importIdleOpen = false;
		bool _importTalkOpen = false;
		bool _importBlinkOpen = false;
		bool _importTalkBlinkOpen = false;
		bool _importScreamOpen = false;

		bool _spriteIdleOpen = false;
		bool _spriteTalkOpen = false;
		bool _spriteBlinkOpen = false;
		bool _spriteTalkBlinkOpen = false;
		bool _spriteScreamOpen = false;
		bool _oldSpriteIdleOpen = false;
		bool _oldSpriteTalkOpen = false;
		bool _oldSpriteBlinkOpen = false;
		bool _oldSpriteTalkBlinkOpen = false;
		bool _oldSpriteScreamOpen = false;
		bool _renamePopupOpen = false;
		bool _renameFirstOpened = false;
		std::string _renamingString = "";

		bool _scrollToHere = false;

		bool AnyPopupOpen()
		{
			return _importIdleOpen || _importTalkOpen || _importBlinkOpen || _importTalkBlinkOpen ||
				_importScreamOpen ||
				_spriteIdleOpen || _spriteTalkOpen || _spriteBlinkOpen || _spriteTalkBlinkOpen || _spriteScreamOpen ||
				_renamePopupOpen;
		}

		void CalculateLayerDepth(std::vector<LayerInfo*>* parents = nullptr);

		bool EvaluateLayerVisibility();

		void DoIndividualMotion(bool talking, bool screaming, float talkAmount, float& rot, sf::Vector2f& motionScale, ImVec4& activeSpriteCol, sf::Vector2f& motionPos);

		void CalculateInheritedMotion(sf::Vector2f& motionScale, sf::Vector2f& motionPos, float& motionRot, float& motionParentRot, ImVec4& motionTint, sf::Vector2f& physicsPos, bool becameVisible, SpriteSheet* lastActiveSprite);

		void DoConstantMotion(sf::Time& frameTime, sf::Vector2f& mpScale, sf::Vector2f& mpPos, float& mpRot);

		void CalculateDraw(float windowHeight, float windowWidth, float talkLevel, float talkMax);

		void DetermineVisibleSprites(bool talking, bool screaming, ImVec4& activeSpriteCol, float& talkAmount);

		void AddMouseMovement(sf::Vector2f& mpPos);

		bool DrawGUI(ImGuiStyle& style, int layerID);

		void ImageBrowsePreviewBtn(bool& openFlag, const char* btnname, sf::Texture* idleIcon, float imgBtnWidth, sf::Color& idleCol, sf::Texture*& texture, std::string& path, SpriteSheet* sprite);

		void DrawThresholdBar(float thresholdLevel, float thresholdTrigger, ImVec2& barPos, float uiScale, float barWidth);

		std::vector<int> _animGrid = { 1, 1 };
		int _animFCount = 1;
		float _animFPS = 12;
		std::vector<float> _animFrameSize = { -1, -1 };

		bool _animsSynced = false;
		bool _animLoop = true;
		bool _restartAnimsOnVisible = false;

		void AnimPopup(SpriteSheet& anim, bool& open, bool& oldOpen);

		void SyncAnims(bool sync);

		int _lastCalculatedDepth = 0;
		std::string _motionParent = "";
		float _motionDelay = 0;
		struct MotionLinkData
		{
			sf::Time _frameTime;
			sf::Vector2f _scale = { 1.f, 1.f };
			sf::Vector2f _pos = { 0,0 };
			sf::Vector2f _physicsPos = { 0,0 };
			ImVec4 _tint;
			float _rot = 0.0;
			float _parentRot = 0.0;
			sf::Vector2f _parentPos = { 0,0 };
		};
		bool _hideWithParent = true;
		bool _inheritTint = false;
		float _motionDrag = 0.f;
		float _motionSpring = 0.f;
		float _distanceLimit = -1.f;
		float _rotationEffect = 0.f;
		sf::Vector2f _lastAccel = { 0.f, 0.f };
		bool _allowIndividualMotion = false;
		bool _physicsIgnorePivots = false;
		MotionStretchType _motionStretch = MS_None;
		sf::Vector2f _motionStretchStrength = { 1.0f, 1.0f };
		sf::Vector2f _stretchScaleMin = { 0.5f, 0.5f };
		sf::Vector2f _stretchScaleMax = { 2.0f, 2.0f };

		std::deque<MotionLinkData> _motionLinkData;

		float _lastTalkFactor = 0.0;

		sf::Clock _frameTimer;
		sf::Clock _physicsTimer;

		sf::Vector2f _lastHeaderScreenPos;
		sf::Vector2f _lastHeaderPos;
		sf::Vector2f _lastHeaderSize;

		sf::Vector2f _constantScale = { 1.f, 1.f };
		sf::Vector2f _constantPos = { 0,0 };
		float _constantRot = 0;
		sf::Vector2f _storedConstantScale = { 1.f, 1.f };
		sf::Vector2f _storedConstantPos = { 0,0 };
		float _storedConstantRot = 0;

		bool _passRotationToChildLayers = false;

		bool _followMouse = false;
		bool _followElliptical = false;
		bool _mouseUntrackedWhenHidden = true;

		sf::Vector2f _mouseAreaSize = { -1.f, -1.f };
		sf::Vector2f _mouseNeutralPos = { -1.f, -1.f };
		sf::Vector2f _mouseMoveLimits = { 50.f, 50.f };

		ImVec4 _layerColor = { 0,0,0,0 };
	};

	struct StatesInfo
	{
		bool _enabled = true;
		bool _active = false;
		bool _alternateHeld = false;

		sf::Keyboard::Key _key = sf::Keyboard::Unknown;
		bool _ctrl = false;
		bool _shift = false;
		bool _alt = false;

		int _jAxis = -1;
		float _jDir = 0.f;
		int _jButton = -1;
		int _jPadID = -1;
		int _mouseButton = -1;

		bool _renaming = false;
		std::string _name = "";

		enum State {
			Hide = 0,
			Show = 1,
			NoChange = 2
		};

		enum ActiveType {
			Toggle = 0,
			Held = 1,
			Permanent = 2
		};

		float _timeout = 5.0;
		bool _useTimeout = true;
		ActiveType _activeType = Toggle;
		bool _schedule = false;
		float _intervalTime = 2.0;
		float _intervalVariation = 0.0;
		float _currentIntervalTime = 0.0;
		std::map<std::string, State> _layerStates;
		bool _awaitingHotkey = false;

		bool _wasTriggered = false;

		enum CanTrigger {
			TRIGGER_ALWAYS,
			TRIGGER_WHILE_TALKING,
			TRIGGER_WHILE_IDLE
		};

		CanTrigger _canTrigger = TRIGGER_ALWAYS;
		float _threshold = 0.4f;

		bool _axisWasTriggered = false;

		bool _keyIsHeld = false;

		bool _webRequestActive = true;
		std::string _webRequest = "";


		sf::Clock _timer;
	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawOldLayerSetUI();

	void UpdateWindowTitle();

	void CopyFileAndUpdatePath(std::string& filePath, std::filesystem::path targetFolder, std::filesystem::copy_options copyOpts);

	void DoMenuBarLogic();

	void DrawButtonsLayerSetUI();

	void DrawGUI(ImGuiStyle& style, float maxHeight);

	void DrawMenusLayerSetUI();

	LayerInfo* AddLayer(const LayerInfo* toCopy = nullptr, bool isFolder = false, int insertPosition = -1);
	void RemoveLayer(int toRemove);
	void MoveLayerUp(int moveUp);
	void MoveLayerDown(int moveDown);
	void MoveLayerTo(int toMove, int position, bool skipFolders = false);

	bool HandleLayerDrag(float mouseX, float mouseY, bool mousePressed);
	int GetLayerUnderCursor(float mouseX, float mouseY);

	void RemoveLayer(LayerInfo* toRemove);
	void MoveLayerUp(LayerInfo* moveUp);
	void MoveLayerDown(LayerInfo* moveDown);

	void MakePortablePath(std::string& path);

	bool SaveLayers(const std::string& settingsFileName, bool makePortable = false, bool copyImages = false);
	bool LoadLayers(const std::string& settingsFileName);

	bool PendingHotkey() { return _waitingForHotkey; }
	void SetHotkeys(const sf::Event& evt)
	{
		if (evt.type == sf::Event::JoystickMoved && _statesIgnoreStick)
			return;

		_pendingMouseButton = -1;
		if (evt.type == sf::Event::KeyPressed)
		{
			_pendingKey = evt.key.code;
			_pendingCtrl = evt.key.control;
			_pendingShift = evt.key.shift;
			_pendingAlt = evt.key.alt;
		}

		if (evt.type == sf::Event::MouseButtonPressed)
		{
			_pendingMouseButton = (int)evt.mouseButton.button;
			_pendingCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
			_pendingShift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
			_pendingAlt = sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt) || sf::Keyboard::isKeyPressed(sf::Keyboard::RAlt);
		}

		_pendingJPadID = -1;

		//_pendingJStick = evt.joystickMove.joystickId;
		if (evt.type == sf::Event::JoystickMoved)
		{
			_pendingJAxis = evt.joystickMove.axis;
			_pendingJDir = evt.joystickMove.position;
			_pendingJPadID = evt.joystickMove.joystickId;
		}

		//_pendingJButtonSID = evt.joystickButton.joystickId;
		if (evt.type == sf::Event::JoystickButtonPressed)
		{
			_pendingJButton = evt.joystickButton.button;
			_pendingJPadID = evt.joystickButton.joystickId;
		}
		else
			_pendingJButton = -1;
	}
	void HandleHotkey(const sf::Event& key, bool keyDown);

	void CheckHotkeys();

	void ResetStates();

	LayerInfo* GetLayer(std::string id, int* idx = nullptr)
	{
		for (int l =0; l < _layers.size(); l++)
		{
			if (_layers[l]._id == id)
			{
				if (idx != nullptr)
					(*idx) = l;
				return &_layers[l];
			}
		}

		return nullptr;
	}

	const std::deque<LayerInfo>& GetLayers()
	{
		return _layers;
	}
	
	std::string LastUsedLayerSet() { return _loadedXMLRelPath; }
	std::string LayerSetName() { return _layerSetName; }
	void SetLayerSet(const std::string& xmlName) { _loadedXMLRelPath = xmlName; }

	void Init(AppConfig* appConf, UIConfig* uiConf);

private:

	bool _loadingFinished = false;
	std::thread* _loadingThread = nullptr;
	std::string _loadingPath = u8"";
	std::string _loadingProgress = u8"";

	bool _copyStateNames = true;

	float _lastTalkLevel = 0.0;
	float _lastTalkMax = 1.0;

	AppConfig* _appConfig = nullptr;
	UIConfig* _uiConfig = nullptr;

	std::vector<StatesInfo> _states;

	std::deque<LayerInfo> _layers;

	sf::RenderTexture _blendingRT;

	ChatReader _chatReader;

	TextureManager* _textureMan;

	bool _statesPassThrough = false;
	bool _statesHideUnaffected = false;
	bool _statesIgnoreStick = false;

	std::string _lastSavedLocation = u8"";
	std::string _loadedXMLRelPath = u8"lastLayers";
	std::string _loadedXMLRelDirectory = u8"";
	std::string _savingXMLPath = u8"";
	std::string _loadedXMLAbsDirectory = u8"";
	std::string _fullLoadedXMLPath = u8"";
	bool _loadedXMLExists = true;
	bool _loadXMLOpen = false;
	bool _saveXMLOpen = false;
	bool _saveAsXMLOpen = false;
	bool _newXMLOpen = false;
	bool _reloadXMLOpen = false;
	bool _makePortableOpen = false;
	bool _saveLayersPortable = false;
	bool _copyImagesPortable = false;
	bool _newLayerOpen = false;
	bool _newFolderOpen = false;
	bool _clearLayersOpen = false;
	bool _editStatesOpen = false;
	bool _clearStatesOpen = false;
	std::string _layerSetName = "lastLayers";

	bool _statesMenuOpen = false;
	bool _oldStatesMenuOpen = false;
	bool _waitingForHotkey = false;
	sf::Keyboard::Key _pendingKey = sf::Keyboard::Unknown;
	bool _pendingCtrl = false;
	bool _pendingShift = false;
	bool _pendingAlt = false;
	sf::Joystick::Axis _pendingJAxis = sf::Joystick::Axis::X;
	float _pendingJDir = 0.f;
	int _pendingJButton = -1;
	int _pendingJPadID = -1;
	int _pendingMouseButton = -1;

	sf::Vector2f _globalScale = { 1.f, 1.f };
	sf::Vector2f _globalPos;
	float _globalRot = 0.0;
	bool _globalKeepAspect = true;

	bool _pivotPreservePosition = true;

	std::map<std::string, bool> _defaultLayerStates;
	std::vector<StatesInfo*> _statesOrder;
	sf::Clock _statesTimer;
	bool _statesDirty = false;
	void DrawStatesGUI();

	void DrawHTTPCopyHelpers(LayerManager::StatesInfo& state, ImVec4& disabledCol, int stateIdx);

	std::string _errorMessage = u8"";

	bool _lastDragMouseDown = false;
	int _draggedLayer = -1;
	sf::Vector2f _layerDragPos = { 0,0 };
	sf::Clock _layerDragTimer;
	bool _dragActive = false;

	std::vector<std::string> _hoveredLayers;

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

	inline void SaveColor(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* colorName, const ImVec4& col)
	{
		auto colElement = parent->FirstChildElement(colorName);
		if (!colElement)
			colElement = parent->InsertFirstChild(doc->NewElement(colorName))->ToElement();

		colElement->SetAttribute("r", col.x);
		colElement->SetAttribute("g", col.y);
		colElement->SetAttribute("b", col.z);
		colElement->SetAttribute("a", col.w);
	}

	inline void LoadColor(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* colorName, ImVec4& col)
	{
		auto colElement = parent->FirstChildElement(colorName);
		if (!colElement)
			return;

		colElement->QueryAttribute("r", &col.x);
		colElement->QueryAttribute("g", &col.y);
		colElement->QueryAttribute("b", &col.z);
		colElement->QueryAttribute("a", &col.w);
	}

	inline void SaveAnimInfo(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* animName, const SpriteSheet& anim, bool animsSynced = false)
	{
		int fcount = anim.FrameCount();
		auto animElement = parent->FirstChildElement(animName);

		bool hasAnim = fcount > 1 || anim.GridSize() != sf::Vector2i(1, 1) || animsSynced == true;

		if (animElement && !hasAnim)
		{
			parent->DeleteChild(animElement);
			return;
		}

		if (!animElement && hasAnim)
			animElement = parent->InsertFirstChild(doc->NewElement(animName))->ToElement();
		else if(!animElement)
			return;

		animElement->SetAttribute("gridX", anim.GridSize().x);
		animElement->SetAttribute("gridY", anim.GridSize().y);
		animElement->SetAttribute("frameW", anim.Size().x);
		animElement->SetAttribute("frameH", anim.Size().y);
		animElement->SetAttribute("fps", anim.FPS());
		animElement->SetAttribute("fCount", fcount);
		animElement->SetAttribute("loop", anim._loop);
	}

	inline void LoadAnimInfo(tinyxml2::XMLElement* parent, tinyxml2::XMLDocument* doc, const char* animName, SpriteSheet& anim)
	{
		auto animElement = parent->FirstChildElement(animName);
		if (!animElement)
			return;

		int fCount;
		animElement->QueryAttribute("fCount", &fCount);

		std::vector<int> grid = { 1,1 };
		std::vector<float> frame = { -1,-1 };
		float fps;
		
		animElement->QueryAttribute("gridX", &grid[0]);
		animElement->QueryAttribute("gridY", &grid[1]);
		animElement->QueryAttribute("frameW", &frame[0]);
		animElement->QueryAttribute("frameH", &frame[1]);
		animElement->QueryAttribute("fps", &fps);
		animElement->QueryAttribute("loop", &anim._loop);

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
static sf::Texture* _newFileIcon = nullptr;
static sf::Texture* _openFileIcon = nullptr;
static sf::Texture* _saveIcon = nullptr;
static sf::Texture* _saveAsIcon = nullptr;
static sf::Texture* _makePortableIcon = nullptr;
static sf::Texture* _reloadIcon = nullptr;
static sf::Texture* _newLayerIcon = nullptr;
static sf::Texture* _newFolderIcon = nullptr;
static sf::Texture* _statesIcon = nullptr;


template <typename T>
inline void AddResetButton(const char* id, T& value, T resetValue, AppConfig* appConfig, ImGuiStyle* style = nullptr, bool enabled = true, ImVec2* cursorPos = nullptr, T* zeroValue = nullptr)
{
	float btnSize = ImGui::GetFont()->FontSize * ImGui::GetIO().FontGlobalScale;

	ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
	if (style)
		col = style->Colors[ImGuiCol_Text];

	sf::Color btnColor = { sf::Uint8(255 * col.x), sf::Uint8(255 * col.y), sf::Uint8(255 * col.z) };

	ImGui::PushID(id);
	if (ImGui::ImageButton(id, *_resetIcon, sf::Vector2f(btnSize, btnSize), sf::Color::Transparent, btnColor))
		if (enabled)
			value = resetValue;
	ImGui::PopID();
	
	if (zeroValue != nullptr)
	{
		ImGui::SameLine();
		ImGui::PushID(id + 1);
		if (ImGui::Button("0"))
			if (enabled)
				value = {};
		ImGui::PopID();
	}
	
	if (cursorPos)
	{
		*cursorPos = ImGui::GetCursorPos();
		cursorPos->x += btnSize + (style ? style->ItemSpacing.x : 0);
	}

	ImGui::SameLine(0);

}

inline float GetRandom01()
{
	return (0.002f * (rand() % 500));
}
inline float GetRandom11()
{
	return (0.002f * (rand() % 500 - 250));
}
