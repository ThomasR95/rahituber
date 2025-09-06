#include "EffectManager.h"

void EffectBase::Restart()
{
}

void EffectBase::PlayEffect(sf::Time dt, std::vector<LayerManager::LayerInfo*>* targetLayers)
{
}

bool EffectBase::IsFinished()
{
	return false;
}

void EffectBase::DrawGUI(ImGuiStyle& style)
{
}

void EffectBase::Step(sf::Time dt, LayerManager::LayerInfo* layer)
{
}

bool EffectManager::CheckEffectTriggers(sf::Event& ev, float audioLevel)
{
	return false;
}

bool EffectManager::UpdateEffects(std::deque<LayerManager::LayerInfo>& layers)
{
	return false;
}

void EffectManager::DrawGUI(ImGuiStyle& style)
{
}
