#pragma once

#include "LayerManager.h"

class EffectBase
{
public:

	void Restart();

	void PlayEffect(sf::Time dt, std::vector<LayerManager::LayerInfo*>* targetLayers);

	bool IsFinished();

	virtual void DrawGUI(ImGuiStyle& style);

private:

	virtual void Step(sf::Time dt, LayerManager::LayerInfo* layer);

	sf::Time _duration;
	sf::Time _elapsed;

};

class EffectManager
{
public:
	bool CheckEffectTriggers(sf::Event& ev, float audioLevel);

	bool UpdateEffects(std::deque<LayerManager::LayerInfo>& layers);

	void DrawGUI(ImGuiStyle& style);

	struct EffectQueue {
		std::deque< std::unique_ptr<EffectBase> > _effects;

		bool _triggerFromKBMC;
		sf::Event _kbmcTrigger;

		bool _triggerFromAudio;
		float _triggerAudioLevel;

		std::string _name;
	};


private:
	sf::Clock _timer;

	std::deque<EffectQueue> _effectQueues;
};

