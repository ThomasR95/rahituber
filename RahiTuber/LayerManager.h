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
			BobNone,
			BobLoudness,
			BobRegular,
		};

		BobbingType _bobType = BobLoudness;
		float _bounceHeight = 100;
		float _bounceFrequency = 0.6;

		bool _doBreathing = true;
		float _breathHeight = 80;
		float _breathFrequency = 2.0;

		std::string _offImagePath = "";
		sf::Texture _offImage;

		std::string _onImagePath = "";
		sf::Texture _onImage;

		std::string _blinkImagePath = "";
		sf::Texture _blinkImage;

		void Draw(float windowHeight, float windowWidth, float talkLevel, float talkMax);

		void DrawGUI(ImGuiStyle& style);
	};

	void Draw(sf::RenderTarget* target, float windowHeight, float windowWidth, float talkLevel, float talkMax);

	void DrawGUI(ImGuiStyle& style);

	void AddLayer();
	void RemoveLayer(int toRemove);

	void SaveLayers();

	void SelectLayer(int layer);
	int GetSelectedLayer() { return _selectedLayer; }

private:

	std::vector<LayerInfo> _layers;

	int _selectedLayer;

};

