#include "pch.h"
#include "CppUnitTest.h"

#include "MainEngine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace RahiTuberTests
{
	TEST_CLASS(RahiTuberTests)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
			MainEngine engine;
			engine.Init();

			Assert::IsNotNull(engine.appConfig);

			engine.Cleanup();
		}
	};
}
