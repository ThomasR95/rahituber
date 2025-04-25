#include "pch.h"

#include  "MainEngine.h"

class MainEngineTest : public testing::Test {
protected:
	MainEngineTest() {
		engine.Init();
	}

	~MainEngineTest()
	{
		engine.Cleanup();
	}


	MainEngine engine;
};

class LayerSetTest : public testing::Test {
protected:
	LayerSetTest() {
		engine.Init();
		engine.layerMan->LoadLayers("LayersForTesting");
	}

	~LayerSetTest()
	{
		engine.Cleanup();
	}


	MainEngine engine;
};

TEST_F(MainEngineTest, LoadsConfig) {
	
	EXPECT_EQ(engine.appConfig->_lastLayerSet, "testLayerSet");
}


TEST_F(MainEngineTest, VersionNumberTest) {

	EXPECT_GT(engine.appConfig->_versionNumber, 0);

	engine.appConfig->_checkUpdateThread->join();

	EXPECT_GT(engine.appConfig->_versionAvailable, 0);
}

TEST_F(LayerSetTest, LayerSetLoading) {

	while (engine.layerMan->IsLoading())
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_EQ(engine.appConfig->_lastLayerSet, "D:\\Rahituber_project\\rahituber\\RahiTuber\\x64\\Debug\\LayersForTesting.xml");

	const auto& layers = engine.layerMan->GetLayers();

	EXPECT_EQ(layers.size(), 8);

	for (int i = 0; i < layers.size(); i++)
	{
		auto& l = layers[i];
		if (l._isFolder == false)
		{
			EXPECT_NE(l._idleImagePath, "");
			EXPECT_NE(l._idleSprite->getTexture(), nullptr);
		}
	}
}
