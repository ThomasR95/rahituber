#include "SpriteSheet.h"

void SpriteSheet::Draw(sf::RenderTarget* target, const sf::RenderStates& states)
{
	sf::Time dt = _timer.getElapsedTime();

	const float frametime = 1.0 / _fps;

	if (_playing || _synced)
	{
		if (!_synced && (frametime < dt.asSeconds()))
		{
			_currentFrame++;
			_timer.restart();
			for (SpriteSheet* spr : _syncChildren)
				spr->AdvanceFrame();
		}
		
		if (!_frameRects.empty())
		{
			_currentFrame = _currentFrame % _frameRects.size();
			_sprite.setTextureRect(_frameRects[_currentFrame]);
		}
	}
	
	if(_visible)
		target->draw(_sprite, states);

}

void SpriteSheet::LoadFromTexture(const sf::Texture& tex, int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& size)
{
	_sprite.setTexture(tex, true);

	SetAttributes(frameCount, gridX, gridY, fps, size);
}

void SpriteSheet::SetAttributes(int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& size)
{
	if (_sprite.getTexture() == nullptr)
		return;

	_fps = fps;

	_gridSize = { gridX, gridY };

	sf::Vector2f frameSize(size);
	sf::Vector2u texSize = _sprite.getTexture()->getSize();

	if (frameSize == sf::Vector2f(-1, -1))
	{
		frameSize = sf::Vector2f((float)texSize.x / gridX, (float)texSize.y / gridY);
	}

	_frameRects.clear();

	int fCount = 0;
	for (int y = 0; y < gridY && fCount <= frameCount; y++)
	{
		for (int x = 0; x < gridX && fCount <= frameCount; x++)
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
