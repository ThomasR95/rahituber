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

TEST_F(MainEngineTest, LoadsConfig) {
	
	EXPECT_EQ(engine.appConfig->_lastLayerSet, "testLayerSet");
}


TEST_F(MainEngineTest, VersionNumberTest) {

	EXPECT_GT(engine.appConfig->_versionNumber, 0);

	engine.appConfig->_checkUpdateThread->join();

	EXPECT_GT(engine.appConfig->_versionAvailable, 0);
}