#pragma once

#include <SFML/System/Clock.hpp>

class ffwdClock
{
public:
	sf::Time getElapsedTime()
	{
		sf::Time res = _clock.getElapsedTime();
		res += _ffwd;
		return res;
	}

	void ffwd(const sf::Time& delta)
	{
		_ffwd = delta;
	}

	sf::Time restart()
	{
		sf::Time res = _clock.restart();
		_ffwd = sf::seconds(0);
		return res;
	}

private:
	sf::Time _ffwd = sf::seconds(0);
	sf::Clock _clock;
};