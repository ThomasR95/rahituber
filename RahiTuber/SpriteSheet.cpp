#include "SpriteSheet.h"

void SpriteSheet::Draw(sf::RenderTarget* target)
{
	sf::Time dt = _timer.getElapsedTime();

	if (_playing)
	{
		if (1.0 / _fps < dt.asSeconds())
		{
			_currentFrame++;
			_timer.restart();
		}
			
		_currentFrame = _currentFrame % _frameRects.size();
		_sprite.setTextureRect(_frameRects[_currentFrame]);
	}
	
	target->draw(_sprite);

}

void SpriteSheet::LoadFromTexture(const sf::Texture& tex, int frameCount, int gridX, int gridY, float fps, const sf::Vector2i& size)
{
	_sprite.setTexture(tex, true);

	SetAttributes(frameCount, gridX, gridY, fps, size);
}

void SpriteSheet::SetAttributes(int frameCount, int gridX, int gridY, float fps, const sf::Vector2i& size)
{
	if (_sprite.getTexture() == nullptr)
		return;

	_fps = fps;

	_gridSize = { gridX, gridY };

	sf::Vector2i frameSize = size;
	sf::Vector2u texSize = _sprite.getTexture()->getSize();

	if (frameSize == sf::Vector2i(-1, -1))
	{
		frameSize = sf::Vector2i(texSize.x / gridX, texSize.y / gridY);
	}

	_frameRects.clear();

	int fCount = 0;
	for (int x = 0; x < gridX && fCount <= frameCount; x++)
	{
		for (int y = 0; y < gridY && fCount <= frameCount; y++)
		{
			_frameRects.push_back(sf::IntRect(x * frameSize.x, y * frameSize.y, frameSize.x, frameSize.y));
			fCount++;
		}
	}

	_sprite.setTextureRect(_frameRects[0]);
	_currentFrame = 0;
	_timer.restart();
	_maxFrame = _frameRects.size() - 1;
	_spriteSize = frameSize;

	_playing = true;
}
