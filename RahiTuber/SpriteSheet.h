#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

class SpriteSheet
{
public:
	void Draw(sf::RenderTarget* target, const sf::RenderStates& states = sf::RenderStates::Default);

	void LoadFromTexture(const sf::Texture& tex, int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& frameSize = { -1, -1 });

	void SetAttributes(int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& frameSize = { -1, -1 });

	inline void setPosition(const sf::Vector2f& pos) { _sprite.setPosition(pos); }
	inline void setOrigin(const sf::Vector2f& origin) { _sprite.setOrigin(origin); }
	inline void setRotation(const float& rot) { _sprite.setRotation(rot); }
	inline void setScale(const sf::Vector2f& scale) { _sprite.setScale(scale); }

	inline sf::Vector2f getPosition() const { return _sprite.getPosition(); }
	inline sf::Vector2f getOrigin() const { return _sprite.getOrigin(); }
	inline float getRotation() const { return _sprite.getRotation(); }
	inline sf::Vector2f getScale() const { return _sprite.getScale(); }

	inline void SetColor(float* col) { _sprite.setColor({ sf::Uint8(255*col[0]), sf::Uint8(255*col[1]),sf::Uint8(255*col[2]),sf::Uint8(255*col[3]) }); }
	inline void SetColor(const sf::Color col) { _sprite.setColor(col); }

	inline sf::Vector2f Size() const { return _spriteSize; }
	inline sf::Vector2i GridSize() const { return _gridSize; }
	inline int FrameCount() const { return _frameRects.size(); }
	inline float FPS() const { return _fps; }

	inline void Play() { _playing = true; }
	inline void Pause() { _playing = false; }
	inline void Stop() 
	{ 
		_currentFrame = 0; 
		_playing = false; 
		for (SpriteSheet* c : _syncChildren)
			c->Stop();
	}
	inline void Restart() 
	{ 
		Stop(); 
		Play();
		for (SpriteSheet* c : _syncChildren)
			c->Restart();
	}

	inline void AdvanceFrame(int idx = -1)
	{
		if (_frameRects.empty())
			return;

		if (idx == -1)
			_currentFrame++;
		else
			_currentFrame = idx;

		if (_loop == false && _currentFrame > _maxFrame)
		{
			_currentFrame = _maxFrame;
		}
		else
		{
			_currentFrame = _currentFrame % _frameRects.size();
		}
	}

	inline void AddSync(SpriteSheet* spr)
	{
		if (spr == nullptr)
			return;

		if (std::find(_syncChildren.begin(), _syncChildren.end(), spr) == _syncChildren.end())
		{
			spr->_synced = true;
			_syncChildren.push_back(spr);
		}
			
	}
	inline void RemoveSync(SpriteSheet* spr)
	{
		auto at = std::find(_syncChildren.begin(), _syncChildren.end(), spr);
		if (at != _syncChildren.end())
		{
			spr->_synced = false;
			_syncChildren.erase(at);
		}
	}

	inline void ClearSync()
	{
		for(SpriteSheet* spr : _syncChildren)
			spr->_synced = false;

		_syncChildren.clear();
	}

	inline bool IsSynced() { return _synced; }

	bool _visible = true;
	bool _loop = true;

private:

	sf::Sprite _sprite;

	sf::Vector2f _spriteSize = { 0,0 };
	sf::Vector2i _gridSize = { 1,1 };

	int _maxFrame = 0;
	int _currentFrame = 0;
	float _fps = 12;
	std::vector<sf::IntRect> _frameRects;

	bool _playing = false;

	sf::Clock _timer;

	bool _synced = false;

	std::vector<SpriteSheet*> _syncChildren;

};

