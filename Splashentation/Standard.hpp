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

#ifndef SPLASHENTATION_STANDARD_HPP
#define SPLASHENTATION_STANDARD_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
//#include <initializer_list>
#include <assert.h>

// thread
#include <thread>
#include <mutex>

// SFML
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

namespace sf
{
	
class RenderWindow;
class Font;

} // namespace sf

// Splashentation v1.0.0
class Splashentation
{
public:
	enum class PlayState
	{
		Ready,
		Playing,
		Finished,
		Quit,
	};
	enum class ControlAction
	{
		None,
		Next,
		Skip,
		Quit,
	};
	enum MouseButtons
	{
		None = 0,
		Left = 1,
		Right = 2,
		Middle = 4,
	};
	struct OrderedDrawable
	{
		std::unique_ptr<sf::Drawable> drawable;
		int zIndex;
		OrderedDrawable() : drawable(nullptr), zIndex(0) { }
		template <class drawableT>
		explicit OrderedDrawable(drawableT& newDrawable, const int newZIndex = 0) : zIndex(newZIndex), drawable(new drawableT(newDrawable)) {}
		template <class drawableT>
		explicit OrderedDrawable(std::unique_ptr<drawableT>& newDrawable, const int newZIndex = 0) : zIndex(newZIndex), drawable(std::move(newDrawable)) {}
	};
	class Slide
	{
	public:
		sf::Color color;
		sf::Time duration;
		sf::Time transition;
		std::vector<std::string> ids;
		std::unordered_map<sf::Keyboard::Key, ControlAction> keys;
		std::unordered_map<ControlAction, MouseButtons> mouseButtons;

		Slide() : color(sf::Color::Black), duration(sf::seconds(5.f)), transition(sf::seconds(2.f)) { }
		Slide(Slide& slide) : color(slide.color), duration(slide.duration), transition(slide.transition), ids(slide.ids), keys(slide.keys), mouseButtons(slide.mouseButtons) { }
		void add(const std::string& id) { ids.emplace_back(id); }
		void clear() { ids.clear(); }
	};

	Splashentation(const sf::VideoMode& videoMode = sf::VideoMode(64, 64), const std::string& name = "", unsigned int style = sf::Style::None, const sf::ContextSettings& contextSettings = sf::ContextSettings());
	~Splashentation();

	void clearAllResources();

	void play();
	void next(); // next slide
	void skip();
	void quit();
	void setupWindow(const sf::VideoMode& videoMode = sf::VideoMode(64, 64), const std::string& name = "", unsigned int style = sf::Style::None, const sf::ContextSettings& contextSettings = sf::ContextSettings());
	sf::Vector2u getWindowSize() const;
	void addFont(const std::string& name, sf::Font& font);
	bool loadFont(const std::string& name, const std::string& filename);
	void removeFont(const std::string& name);
	sf::Font* getFont(const std::string& name) const;
	void addTexture(const std::string& name, sf::Texture& texture);
	bool loadTexture(const std::string& name, const std::string& filename);
	void removeTexture(const std::string& name);
	sf::Texture* getTexture(const std::string& name) const;
	void addSlide(Slide& slide);
	void clearSlides();

	void addGlobalControlAction(ControlAction controlAction, sf::Keyboard::Key key);
	void removeGlobalControlAction(sf::Keyboard::Key key);
	ControlAction getGlobalControlAction(sf::Keyboard::Key key) const;
	void setGlobalMouseButtons(ControlAction controlAction, MouseButtons mouseButtons);
	MouseButtons getGlobalMouseButtons(ControlAction controlAction) const;

	void addSlideControlAction(unsigned int slideIndex, ControlAction controlAction, sf::Keyboard::Key key);
	void removeSlideControlAction(unsigned int slideIndex, sf::Keyboard::Key key);
	ControlAction getSlideControlAction(unsigned int slideIndex, sf::Keyboard::Key key) const;
	void setSlideMouseButtons(unsigned int slideIndex, ControlAction controlAction, MouseButtons mouseButtons);
	MouseButtons getSlideMouseButtons(unsigned int slideIndex, ControlAction controlAction) const;

	bool isPlaying() const;
	PlayState getPlayState() const;
	sf::Time getSlideTime() const;
	unsigned int getCurrentSlideIndex() const;



	template <class drawableT>
	void addDrawable(const std::string& id, drawableT& drawable, const int zIndex = 0);

	// zIndex
	void setDrawableZIndex(const std::string& id, int zIndex);

	// transformable
	void setDrawableScale(const std::string& id, sf::Vector2f newScale);
	void setDrawablePosition(const std::string& id, sf::Vector2f newPosition);
	void setDrawableOrigin(const std::string& id, sf::Vector2f newOrigin);
	void setDrawableRotation(const std::string& id, float newRotation);

	// text
	void setDrawableString(const std::string& id, const std::string& newString);

private:
	struct WindowSettings
	{
		sf::VideoMode videoMode;
		std::string name;
		unsigned int style;
		sf::ContextSettings contextSettings;
	} m_windowSettings;

	enum class SlideState
	{
		In,
		Show,
	} m_slideState{ SlideState::In };

	std::unordered_map<std::string, OrderedDrawable> m_drawables;
	std::unique_ptr<sf::RenderWindow> m_window;
	sf::Clock m_clock;
	std::vector<Slide> m_slides;
	PlayState m_playState;
	bool m_moveOnToNextSlide;
	bool m_controlSkip;
	bool m_controlQuit;
	unsigned int m_currentSlideIndex;
	std::unordered_map<sf::Keyboard::Key, ControlAction> m_globalKeys;
	std::unordered_map<ControlAction, MouseButtons> m_globalMouseButtons;





	// thread

	std::thread m_playThread;
	mutable std::mutex m_playStateMutex;
	mutable std::mutex m_windowSettingsMutex;
	mutable std::mutex m_drawablesMutex;
	mutable std::mutex m_clockMutex;
	mutable std::mutex m_controlsMutex;
	mutable std::mutex m_informationMutex;
	mutable std::mutex m_slideStateMutex;

	void t_play();

	void priv_waitForThreadToFinish();
	SlideState priv_getSlideState() const;
	void priv_setSlideState(SlideState slideState);
	bool priv_processKey(std::pair<sf::Keyboard::Key, ControlAction> control, sf::Keyboard::Key key, bool& foundKey);
	bool priv_processMouseButton(std::pair<ControlAction, MouseButtons> control, sf::Mouse::Button mouseButton, bool& foundMouseButton);
};

template <class drawableT>
void Splashentation::addDrawable(const std::string& id, drawableT& drawable, const int zIndex)
{
	// ID must be supplied
	assert(id != "");

	std::lock_guard<std::mutex> lockGuard(m_drawablesMutex);
	m_drawables.emplace(id, OrderedDrawable(drawable, zIndex));
}

#endif // SPLASHENTATION_STANDARD_HPP
