#include "SpriteSheet.h"

void SpriteSheet::Draw(sf::RenderTarget* target, const sf::RenderStates& states)
{
	sf::Time dt = _timer.getElapsedTime();

	const float frametime = 1.0f / _fps;

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
			if (_loop == false && _currentFrame > _maxFrame)
			{
				_currentFrame = _maxFrame;
			}
			else
			{
				_currentFrame = _currentFrame % _frameRects.size();
				_sprite.setTextureRect(_frameRects[_currentFrame]);
			}
		}
	}
	
	if(_visible)
		target->draw(_sprite, states);

}

void SpriteSheet::LoadFromTexture(const sf::Texture& tex, int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& size)
{
	bool autoSize = size == sf::Vector2f(-1, -1);

	if(autoSize)
		_sprite.setTexture(tex, true);

	SetAttributes(frameCount, gridX, gridY, fps, size);

	if(!autoSize)
		_sprite.setTexture(tex, false);
}

void SpriteSheet::SetAttributes(int frameCount, int gridX, int gridY, float fps, const sf::Vector2f& size)
{
	_fps = fps;

	gridX = std::max(1, gridX);
	gridY = std::max(1, gridY);
	frameCount = std::max(1, frameCount);

	_gridSize = { gridX, gridY };
	sf::Vector2f frameSize(size);

	if (frameSize == sf::Vector2f(-1, -1))
	{
		if (_sprite.getTexture() == nullptr)
			return;

		sf::Vector2u texSize = _sprite.getTexture()->getSize();
		frameSize = sf::Vector2f((float)texSize.x / gridX, (float)texSize.y / gridY);
	}

	_frameRects.clear();

	int fCount = 0;
	for (int y = 0; y < gridY && fCount < frameCount; y++)
	{
		for (int x = 0; x < gridX && fCount < frameCount; x++)
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
