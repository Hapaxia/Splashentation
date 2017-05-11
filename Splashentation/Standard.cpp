//////////////////////////////////////////////////////////////////////////////
//
// Splashentation (https://github.com/Hapaxia/Splashentation)
//
// Copyright (c) 2016-2017 M.J.Silk
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software. If you use this software
// in a product, an acknowledgement in the product documentation would be
// appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
// M.J.Silk
// MJSilk2@gmail.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Standard.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <vector>
#include <unordered_map>
#include <algorithm> // for std::find

namespace
{

std::mutex resourceMutex;
std::unordered_map<std::string, sf::Font> fonts;
std::unordered_map<std::string, sf::Texture> textures;

} // namespace

Splashentation::Splashentation(const sf::VideoMode& videoMode, const std::string& name, const unsigned int style, const sf::ContextSettings& contextSettings)
	: m_window(nullptr)
	, m_clock()
	, m_slides()
	, m_playThread()
	, m_playState(PlayState::Ready)
	, m_moveOnToNextSlide(false)
{
	setupWindow(videoMode, name, style, contextSettings);
}

Splashentation::~Splashentation()
{
	priv_waitForThreadToFinish();
}

void Splashentation::clearAllResources()
{
	if (isPlaying())
		return;

	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	fonts.clear();
	textures.clear();
}

void Splashentation::play()
{
	if ((isPlaying()) || (m_slides.size() == 0))
		return;

	m_currentSlideIndex = 0u;
	m_moveOnToNextSlide = false;
	m_slideState = SlideState::In;
	m_playState = PlayState::Playing;
	m_playThread = std::thread(&Splashentation::t_play, this);
}

void Splashentation::next()
{
	std::lock_guard<std::mutex> lockGuard(m_controlsMutex);
	m_moveOnToNextSlide = true;
	priv_setSlideState(SlideState::In);
}

void Splashentation::skip()
{
	std::lock_guard<std::mutex> lockGuard(m_controlsMutex);
	m_controlSkip = true;
}

void Splashentation::quit()
{
	std::lock_guard<std::mutex> lockGuard(m_controlsMutex);
	m_controlQuit = true;
}

void Splashentation::t_play()
{
	std::vector<Slide>::iterator currentSlide{ m_slides.begin() };
	std::vector<Slide>::iterator previousSlide{ m_slides.end() };
	std::unique_ptr<sf::RenderTexture> renderTexture(new sf::RenderTexture);
	m_windowSettingsMutex.lock();
	m_window.reset(new sf::RenderWindow(m_windowSettings.videoMode, m_windowSettings.name, m_windowSettings.style, m_windowSettings.contextSettings));
	renderTexture->create(m_windowSettings.videoMode.width, m_windowSettings.videoMode.height);
	m_windowSettingsMutex.unlock();
	m_window->setFramerateLimit(60);
	bool isComplete{ false };
	m_clockMutex.lock();
	m_clock.restart();
	m_clockMutex.unlock();
	while (!isComplete)
	{
		const bool showCurrentSlide{ currentSlide != m_slides.end() };
		const bool showPreviousSlide{ (m_slideState == SlideState::In) && (previousSlide != m_slides.end()) };

		// prepare a "list" of drawables, sorted by z-index
		std::vector<OrderedDrawable*> currentSortedDrawables;
		std::vector<OrderedDrawable*> previousSortedDrawables;
		
		m_drawablesMutex.lock();
		if (showCurrentSlide)
		{
			currentSortedDrawables.reserve(currentSlide->ids.size());
			for (auto& id : currentSlide->ids)
			{
				if (m_drawables[id].drawable != nullptr)
					currentSortedDrawables.push_back(&m_drawables[id]);
			}
		}
		if (showPreviousSlide)
		{
			previousSortedDrawables.reserve(previousSlide->ids.size());
			for (auto& id : previousSlide->ids)
			{
				if (m_drawables[id].drawable != nullptr)
					previousSortedDrawables.push_back(&m_drawables[id]);
			}
		}
		m_drawablesMutex.unlock();

		std::sort(currentSortedDrawables.begin(), currentSortedDrawables.end(),
			[](const OrderedDrawable* a, const OrderedDrawable* b) { return a->zIndex < b->zIndex; });
		std::sort(previousSortedDrawables.begin(), previousSortedDrawables.end(),
			[](const OrderedDrawable* a, const OrderedDrawable* b) { return a->zIndex < b->zIndex; });

		m_drawablesMutex.lock();
		resourceMutex.lock();

		// prepare overlay for current slide
		if (!showCurrentSlide)
			renderTexture->clear(sf::Color::Black);
		else
		{
			renderTexture->clear(currentSlide->color);
			for (auto& drawable : currentSortedDrawables)
				renderTexture->draw(*(drawable->drawable));
		}
		renderTexture->display();

		// draw slides

		if (!showPreviousSlide)
			m_window->clear(sf::Color::Black);
		else
		{
			m_window->clear(previousSlide->color);
			for (auto& drawable : previousSortedDrawables)
				m_window->draw(*(drawable->drawable));
		}
		//if (showCurrentSlide)
		{
			sf::Sprite renderSprite(renderTexture->getTexture());
			if (showCurrentSlide)
			{
				float alpha{ (priv_getSlideState() == SlideState::In) ? getSlideTime().asSeconds() / currentSlide->transition.asSeconds() : 1.f };
				if (alpha < 0)
					alpha = 0.f;
				else if (alpha > 1)
					alpha = 1.f;
				renderSprite.setColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255.f * alpha)));
			}
			m_window->draw(renderSprite);
		}

		resourceMutex.unlock();
		m_drawablesMutex.unlock();
		m_window->display();

		// handle events
		sf::Event event;
		while (m_window->pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
			{
				m_window->close();
				std::lock_guard<std::mutex> lockGuard(m_playStateMutex);
				m_playState = PlayState::Quit;
				return;
			}
			else if (event.type == sf::Event::MouseButtonPressed)
			{
				bool foundMouseButton{ false };
				for (auto& buttons : currentSlide->mouseButtons)
				{
					if (!priv_processMouseButton(buttons, event.mouseButton.button, foundMouseButton))
						return;
				}
				for (auto& buttons : m_globalMouseButtons)
				{
					if (!priv_processMouseButton(buttons, event.mouseButton.button, foundMouseButton))
						return;
				}
				if (!foundMouseButton)
				{
					// button not found
				}
			}
			else if (event.type == sf::Event::KeyPressed)
			{
				bool foundKey{ false };
				for (auto& key : currentSlide->keys)
				{
					if (!priv_processKey(key, event.key.code, foundKey))
						return;
				}
				for (auto& key : m_globalKeys)
				{
					if (!priv_processKey(key, event.key.code, foundKey))
						return;
				}
				if (!foundKey)
				{
					// key not found
				}
			}
		}

		std::lock_guard<std::mutex> lockGuardClock(m_clockMutex);

		// update
		if (showCurrentSlide && !m_moveOnToNextSlide)
		{
			const SlideState currentSlideState{ priv_getSlideState() };
			if (currentSlideState == SlideState::In)
			{
				if ((currentSlide->transition == sf::Time::Zero) || (m_clock.getElapsedTime() >= currentSlide->transition))
					priv_setSlideState(SlideState::Show);
			}
			else if (currentSlideState == SlideState::Show)
			{
				if ((currentSlide->duration > sf::Time::Zero) && (m_clock.getElapsedTime() >= (currentSlide->transition + currentSlide->duration)))
					next();
			}
		}

		std::lock_guard<std::mutex> lockGuardControls(m_controlsMutex);

		// external controls
		if (m_controlSkip)
		{
			m_window->close();
			std::lock_guard<std::mutex> lockGuardPlayState(m_playStateMutex);
			m_playState = PlayState::Finished;
			return;
		}
		if (m_controlQuit)
		{
			m_window->close();
			std::lock_guard<std::mutex> lockGuardPlayState(m_playStateMutex);
			m_playState = PlayState::Quit;
			return;
		}

		// progression
		if (m_moveOnToNextSlide)
		{
			m_moveOnToNextSlide = false;
			m_clock.restart();
			if (currentSlide == m_slides.begin())
				previousSlide = currentSlide;
			else
				++previousSlide;
			if (++currentSlide == m_slides.end())
				isComplete = true;
			else
			{
				std::lock_guard<std::mutex> lockGuard(m_informationMutex);
				m_currentSlideIndex = currentSlide - m_slides.begin();
			}
		}
	}
	m_window->close();
	std::lock_guard<std::mutex> lockGuard(m_playStateMutex);
	m_playState = PlayState::Finished;
	return;
}

void Splashentation::setupWindow(const sf::VideoMode& videoMode, const std::string& name, const unsigned int style, const sf::ContextSettings& contextSettings)
{
	std::lock_guard<std::mutex> lockGuard(m_windowSettingsMutex);
	m_windowSettings = { videoMode, name, style, contextSettings };
}

sf::Vector2u Splashentation::getWindowSize() const
{
	std::lock_guard<std::mutex> lockGuard(m_windowSettingsMutex);
	return{ m_windowSettings.videoMode.width, m_windowSettings.videoMode.height };
}

void Splashentation::addFont(const std::string& name, sf::Font& font)
{
	if (isPlaying())
		return;

	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	fonts[name] = font;
}

bool Splashentation::loadFont(const std::string& name, const std::string& filename)
{
	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	return (isPlaying() ? false : fonts[name].loadFromFile(filename));
}

void Splashentation::removeFont(const std::string& name)
{
	if (isPlaying())
		return;

	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	fonts.erase(name);
}

sf::Font* Splashentation::getFont(const std::string& name) const
{
	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	return (isPlaying() ? nullptr : &fonts[name]);
}

void Splashentation::addTexture(const std::string& name, sf::Texture& texture)
{
	if (isPlaying())
		return;

	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	textures[name] = texture;
}

bool Splashentation::loadTexture(const std::string& name, const std::string& filename)
{
	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	return (isPlaying() ? false : textures[name].loadFromFile(filename));
}

void Splashentation::removeTexture(const std::string& name)
{
	if (isPlaying())
		return;

	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	textures.erase(name);
}

sf::Texture* Splashentation::getTexture(const std::string& name) const
{
	std::lock_guard<std::mutex> lockGuard(resourceMutex);
	return (isPlaying() ? nullptr : &textures[name]);
}

void Splashentation::addSlide(Slide& slide)
{
	if (isPlaying())
		return;

	m_slides.emplace_back(slide);
}

void Splashentation::clearSlides()
{
	if (isPlaying())
		return;

	m_slides.clear();
}

void Splashentation::addGlobalControlAction(const ControlAction controlAction, const sf::Keyboard::Key key)
{
	if (isPlaying())
		return;

	if (m_globalKeys.count(key) == 0)
		m_globalKeys[key] = controlAction;
}

void Splashentation::removeGlobalControlAction(const sf::Keyboard::Key key)
{
	if (isPlaying())
		return;

	m_globalKeys.erase(key);
}

Splashentation::ControlAction Splashentation::getGlobalControlAction(const sf::Keyboard::Key key) const
{
	if (isPlaying())
		return ControlAction::None;

	const std::unordered_map<sf::Keyboard::Key, ControlAction>::const_iterator result{ m_globalKeys.find(key) };
	return (result == m_globalKeys.end() ? ControlAction::None : result->second);
}

void Splashentation::setGlobalMouseButtons(const ControlAction controlAction, const MouseButtons mouseButtons)
{
	if (isPlaying())
		return;

	m_globalMouseButtons[controlAction] = mouseButtons;
}

Splashentation::MouseButtons Splashentation::getGlobalMouseButtons(const ControlAction controlAction) const
{
	if (isPlaying())
		return MouseButtons::None;

	const std::unordered_map<ControlAction, MouseButtons>::const_iterator result{ m_globalMouseButtons.find(controlAction) };
	return (result == m_globalMouseButtons.end() ? MouseButtons::None : result->second);
}

void Splashentation::addSlideControlAction(const unsigned int slideIndex, const ControlAction controlAction, const sf::Keyboard::Key key)
{
	if (isPlaying())
		return;

	if (m_slides[slideIndex].keys.count(key) == 0)
		m_slides[slideIndex].keys[key] = controlAction;
}

void Splashentation::removeSlideControlAction(const unsigned int slideIndex, const sf::Keyboard::Key key)
{
	if (isPlaying())
		return;

	m_slides[slideIndex].keys.erase(key);
}

Splashentation::ControlAction Splashentation::getSlideControlAction(const unsigned int slideIndex, const sf::Keyboard::Key key) const
{
	if (isPlaying())
		return ControlAction::None;

	const std::unordered_map<sf::Keyboard::Key, ControlAction>::const_iterator result{ m_slides[slideIndex].keys.find(key) };
	return (result == m_slides[slideIndex].keys.end() ? ControlAction::None : result->second);
}

void Splashentation::setSlideMouseButtons(const unsigned int slideIndex, const ControlAction controlAction, const MouseButtons mouseButtons)
{
	if (isPlaying())
		return;

	m_slides[slideIndex].mouseButtons[controlAction] = mouseButtons;
}

Splashentation::MouseButtons Splashentation::getSlideMouseButtons(const unsigned int slideIndex, const ControlAction controlAction) const
{
	if (isPlaying())
		return MouseButtons::None;

	const std::unordered_map<ControlAction, MouseButtons>::const_iterator result{ m_slides[slideIndex].mouseButtons.find(controlAction) };
	return (result == m_slides[slideIndex].mouseButtons.end() ? MouseButtons::None : result->second);
}

bool Splashentation::isPlaying() const
{
	return (getPlayState() == PlayState::Playing);
}

Splashentation::PlayState Splashentation::getPlayState() const
{
	std::lock_guard<std::mutex> lockGuard(m_playStateMutex);
	return m_playState;
}

sf::Time Splashentation::getSlideTime() const
{
	std::lock_guard<std::mutex> lockGuard(m_clockMutex);
	return m_clock.getElapsedTime();
}

unsigned int Splashentation::getCurrentSlideIndex() const
{
	std::lock_guard<std::mutex> lockGuard(m_informationMutex);
	return m_currentSlideIndex;
}



// z index

void Splashentation::setDrawableZIndex(const std::string& id, const int newZIndex)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	m_drawables[id].zIndex = newZIndex;
}



// transformable

void Splashentation::setDrawableScale(const std::string& id, const sf::Vector2f newScale)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	dynamic_cast<sf::Transformable*>(m_drawables[id].drawable.get())->setScale(newScale);
}

void Splashentation::setDrawablePosition(const std::string& id, const sf::Vector2f newPosition)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	dynamic_cast<sf::Transformable*>(m_drawables[id].drawable.get())->setPosition(newPosition);
}

void Splashentation::setDrawableOrigin(const std::string& id, const sf::Vector2f newOrigin)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	dynamic_cast<sf::Transformable*>(m_drawables[id].drawable.get())->setOrigin(newOrigin);
}

void Splashentation::setDrawableRotation(const std::string& id, const float newRotation)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	dynamic_cast<sf::Transformable*>(m_drawables[id].drawable.get())->setRotation(newRotation);
}



// text

void Splashentation::setDrawableString(const std::string& id, const std::string& newString)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	static_cast<sf::Text*>(m_drawables[id].drawable.get())->setString(newString);
}



// PRIVATE

void Splashentation::priv_waitForThreadToFinish()
{
	if (m_playThread.joinable())
		m_playThread.join();
}

Splashentation::SlideState Splashentation::priv_getSlideState() const
{
	std::lock_guard<std::mutex> lockGuard(m_slideStateMutex);
	return m_slideState;
}

void Splashentation::priv_setSlideState(SlideState slideState)
{
	std::lock_guard<std::mutex> lockGuard(m_slideStateMutex);
	m_slideState = slideState;
}

bool Splashentation::priv_processKey(const std::pair<sf::Keyboard::Key, ControlAction> control, const sf::Keyboard::Key key, bool& foundKey)
{
	if ((foundKey) || (control.first != key))
		return true;

	std::lock_guard<std::mutex> lockGuard(m_playStateMutex);
	switch (control.second)
	{
	case ControlAction::Quit:
		m_window->close();
		m_playState = PlayState::Quit;
		return false;
	case ControlAction::Skip:
		m_window->close();
		m_playState = PlayState::Finished;
		return false;
	case ControlAction::Next:
		if (priv_getSlideState() == SlideState::Show)
		{
			foundKey = true;
			next();
		}
		break;
	case ControlAction::None:
	default:
		;
	}
	return true;
}

bool Splashentation::priv_processMouseButton(const std::pair<ControlAction, MouseButtons> control, const sf::Mouse::Button mouseButton, bool& foundMouseButton)
{
	if (foundMouseButton)
		return true;

	MouseButtons mouseButtons
	{
		static_cast<MouseButtons>
		(((mouseButton == sf::Mouse::Button::Left) ? MouseButtons::Left : MouseButtons::None) |
		((mouseButton == sf::Mouse::Button::Right) ? MouseButtons::Right : MouseButtons::None) |
		((mouseButton == sf::Mouse::Button::Middle) ? MouseButtons::Middle : MouseButtons::None))
	};
	if ((control.second & mouseButtons) == mouseButtons)
	{
		std::lock_guard<std::mutex> lockGuard(m_playStateMutex);
		switch (control.first)
		{
		case ControlAction::Quit:
			m_window->close();
			m_playState = PlayState::Quit;
			return false;
		case ControlAction::Skip:
			m_window->close();
			m_playState = PlayState::Finished;
			return false;
		case ControlAction::Next:
			if (priv_getSlideState() == SlideState::Show)
			{
				foundMouseButton = true;
				next();
			}
			break;
		case ControlAction::None:
		default:
			;
		}
	}
	return true;
}
