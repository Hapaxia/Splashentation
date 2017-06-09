//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Splashentation - "Loading Splash" *EXAMPLE*
//
//  by Hapaxia (https://github.com/Hapaxia)
//
//
//  A simple example of a loading splash screen window with progress bar.
//
//
//    Controls:
//
//  Escape key          Quit
//
//
//  Please note that this example makes use of C++11 features
//    and also requires the SFML library (http://www.sfml-dev.org)
//
//////////////////////////////////////////////////////////////////////////////////////////////



#include <SFML/Graphics.hpp>
#include <Splashentation.hpp>
#include <vector>
#include <string>
#include <fstream>

int main()
{
	srand(0);

	// size of window for loading splash screen
	const sf::Vector2u loadingSplashWindowSize{ 800u, 600u };

	// prepare vector of filenames to load
	struct FileInfo
	{
		std::string name;
		std::size_t size;
	};
	std::vector<FileInfo> fileInfos;

	// add 100 filenames to the vector (from a choice of 3)
	for (unsigned int i{ 0u }; i < 100; ++i)
	{
		unsigned int r{ static_cast<unsigned int>(rand() % 100) };
		if (r < 5)
			fileInfos.emplace_back(FileInfo{ "resources/fonts/arial.ttf", 0 }); // much longer than the other two
		else if (r < 50)
			fileInfos.emplace_back(FileInfo{ "resources/images/sfml-logo-small.png", 0 });
		else
			fileInfos.emplace_back(FileInfo{ "resources/images/The Sun.jpg", 0 });
	}

	// set up splashentation
	Splashentation loadingSplash;
	loadingSplash.loadFont("arial", "resources/fonts/arial.ttf");
	loadingSplash.loadTexture("sfml logo", "resources/images/sfml-logo-small.png");
	loadingSplash.loadTexture("sun photo", "resources/images/The Sun.jpg");
	loadingSplash.setupWindow(sf::VideoMode(loadingSplashWindowSize.x, loadingSplashWindowSize.y), "WINDOW");
	loadingSplash.addGlobalControlAction(Splashentation::ControlAction::Quit, sf::Keyboard::Key::Escape);

	// prepare drawables
	sf::Text progressText;
	sf::RectangleShape progressBar;
	sf::RectangleShape progressBarOutline;
	progressText.setFont(*loadingSplash.getFont("arial"));
	progressText.setPosition({ loadingSplashWindowSize.x / 2.f, 500.f });
	progressText.setString("PROGRESS: 100%");
	progressText.setOrigin({ progressText.getLocalBounds().left + progressText.getLocalBounds().width / 2.f, progressText.getLocalBounds().top + progressText.getLocalBounds().height / 2.f });
	progressText.setString("PROGRESS: 0%");
	progressBar.setSize({ 400.f, 50.f });
	progressBar.setOrigin({ 0.f, progressBar.getSize().y / 2.f });
	progressBar.setPosition({ (loadingSplashWindowSize.x - progressBar.getSize().x) / 2.f, 500.f });
	progressBarOutline = progressBar;
	progressBar.setFillColor(sf::Color::Blue);
	progressBar.setScale({ 0.f, 1.f });
	progressBarOutline.setFillColor(sf::Color(0, 0, 128, 128));
	progressBarOutline.setOutlineColor(sf::Color::White);
	progressBarOutline.setOutlineThickness(5.f);
	sf::Sprite sfmlLogoSprite(*loadingSplash.getTexture("sfml logo"));
	sfmlLogoSprite.setPosition({ 234.f, 123.f }); // place sfml logo over the sun
	sf::RectangleShape sunPhotoSprite;
	sunPhotoSprite.setTexture(loadingSplash.getTexture("sun photo"));
	sunPhotoSprite.setSize(sf::Vector2f(loadingSplashWindowSize));

	// add drawables to splashentation
	loadingSplash.addDrawable("progress bar", progressBar);
	loadingSplash.addDrawable("progress bar outline", progressBarOutline);
	loadingSplash.addDrawable("progress text", progressText);
	loadingSplash.addDrawable("sfml logo", sfmlLogoSprite);
	loadingSplash.addDrawable("sun photo", sunPhotoSprite);

	// prepare single slide
	Splashentation::Slide slide;
	slide.add("sun photo");
	slide.add("progress bar");
	slide.add("progress bar outline");
	slide.add("progress text");

	// add first slide to splashentation
	slide.duration = sf::seconds(1.f);
	loadingSplash.addSlide(slide);

	// add second slide to presentation
	slide.duration = sf::Time::Zero; // no timer
	slide.add("sfml logo");
	loadingSplash.addSlide(slide);

	// add empty slide to allow final transition
	slide.clear();
	slide.duration = sf::seconds(0.0001f);
	slide.transition = sf::seconds(0.5f); // quick fade out
	loadingSplash.addSlide(slide);

	// play splashentation
	loadingSplash.play();

	// calculate file sizes
	for (auto& fileInfo : fileInfos)
	{
		std::ifstream file;
		file.open(fileInfo.name, std::ios::in | std::ios::binary | std::ios::ate);
		fileInfo.size = static_cast<std::size_t>(file.tellg());
	}

	// calculate total files size
	std::size_t totalFilesSize{ 0u };
	for (auto& fileInfo : fileInfos)
		totalFilesSize += fileInfo.size;

	// file-based parallel code
	std::size_t currentFileAccumulation{ 0u };
	for (auto& fileInfo : fileInfos)
	{
		// update progress bar
		const float ratio{ static_cast<float>(currentFileAccumulation) / totalFilesSize };
		loadingSplash.setDrawableScale("progress bar", { ratio, 1.f });
		loadingSplash.setDrawableString("progress text", "PROGRESS: " + std::to_string(static_cast<unsigned int>(std::ceil(ratio * 100.f))) + "%");

		// quitting leaves the loop immediately
		if (loadingSplash.getPlayState() == Splashentation::PlayState::Quit)
			break;

		// load each file 1000 times to slow down the process and simulate larger files
		for (unsigned int i{ 0u }; i < 1000; ++i)
		{
			// load file into memory and then discard memory (at end of loop)
			std::ifstream file;
			file.open(fileInfo.name, std::ios::in | std::ios::binary);
			std::unique_ptr<char[]> data;
			data.reset(new char[fileInfo.size]);
			file.read(data.get(), fileInfo.size);
		}

		// increase current total by last loaded file
		currentFileAccumulation += fileInfo.size;
	}

	// if Splashentation was quit (closed or Escape key pressed), quit program normally
	if (loadingSplash.getPlayState() == Splashentation::PlayState::Quit)
		return EXIT_SUCCESS;
	else
	{

		// ensure correct slide has been reached before progressing and that its transition has ended
		while ((loadingSplash.getCurrentSlideIndex() < 1) || (loadingSplash.getSlideTime() < sf::seconds(2.f)))
			sf::sleep(sf::seconds(0.1f));

		// progress Splashentation to end normally since all files have completed
		loadingSplash.next();
	}



	// ...
	// main application here
	// ...



	return EXIT_SUCCESS;
}
