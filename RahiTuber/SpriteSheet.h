#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/Main.hpp"
#include "SFML/System.hpp"

class SpriteSheet
{
public:
	void Draw(sf::RenderTarget* target);

	void LoadFromTexture(const sf::Texture& tex, int frameCount, int gridX, int gridY, float fps, const sf::Vector2i& frameSize = { -1, -1 });

	void SetAttributes(int frameCount, int gridX, int gridY, float fps, const sf::Vector2i& frameSize = { -1, -1 });

	void setPosition(const sf::Vector2f& pos) { _sprite.setPosition(pos); }
	void setOrigin(const sf::Vector2f& origin) { _sprite.setOrigin(origin); }
	void setRotation(const float& rot) { _sprite.setRotation(rot); }
	void setScale(const sf::Vector2f& scale) { _sprite.setScale(scale); }

	sf::Vector2f getPosition() const { return _sprite.getPosition(); }
	sf::Vector2f getOrigin() const { return _sprite.getOrigin(); }
	float getRotation() const { return _sprite.getRotation(); }
	sf::Vector2f getScale() const { return _sprite.getScale(); }

	void SetColor(float* col) { _sprite.setColor({ sf::Uint8(255*col[0]), sf::Uint8(255*col[1]),sf::Uint8(255*col[2]),sf::Uint8(255*col[3]) }); }
	void SetColor(const sf::Color col) { _sprite.setColor(col); }


	sf::Vector2i Size() const { return _spriteSize; }
	sf::Vector2i GridSize() const { return _gridSize; }
	int FrameCount() const { return _frameRects.size(); }
	float FPS() const { return _fps; }

	void Play() { _playing = true; }
	void Pause() { _playing = false; }
	void Stop() { _currentFrame = 0; _playing = false; }
	void Restart() { Stop(); Play(); }

private:

	sf::Sprite _sprite;

	sf::Vector2i _spriteSize;
	sf::Vector2i _gridSize;

	int _maxFrame = 0;
	int _currentFrame = 0;
	float _fps;
	std::vector<sf::IntRect> _frameRects;

	bool _playing = false;

	sf::Clock _timer;

};

