#include "Precompiled.h"
#include "CppUnitTest.h"
#include "Button.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace libgui;
using namespace std;

namespace libguitest
{
	TEST_CLASS(ButtonTests)
	{
	public:

		TEST_METHOD(WhenEntering_StateIsHot)
		{
			auto btn = make_shared<Button>();
			btn->NotifyEnter();

			Assert::AreEqual(true, btn->IsHot());
		}

		TEST_METHOD(WhenLeaving_StateIsNotHot)
		{
			auto btn = make_shared<Button>();
			btn->NotifyEnter();
			btn->NotifyLeave();
			Assert::AreEqual(false, btn->IsHot());
		}

		TEST_METHOD(WhenDowning_StateIsHotAndPressed)
		{
			auto btn = make_shared<Button>();
			btn->NotifyDown();
			Assert::AreEqual(true, btn->IsHot());
			Assert::AreEqual(true, btn->IsPressed());
		}

		TEST_METHOD(WhenUpping_StateIsNotPressed)
		{
			auto btn = make_shared<Button>();
			btn->NotifyUp();
			Assert::AreEqual(false, btn->IsPressed());
		}

		TEST_METHOD(WhenDownAndLeaving_StateIsHotAndNotPressed)
		{
			auto btn = make_shared<Button>();
			btn->NotifyDown();
			btn->NotifyLeave();
			Assert::AreEqual(true, btn->IsHot());
			Assert::AreEqual(false, btn->IsPressed());
		}

		TEST_METHOD(WhenDownAndReturning_StateIsHotAndPressed)
		{
			auto btn = make_shared<Button>();
			btn->NotifyDown();
			btn->NotifyLeave();
			btn->NotifyEnter();
			Assert::AreEqual(true, btn->IsHot());
			Assert::AreEqual(true, btn->IsPressed());
		}

		TEST_METHOD(WhenLeftAndUpping_StateIsNothing)
		{
			auto btn = make_shared<Button>();
			btn->NotifyDown();
			btn->NotifyLeave();
			btn->NotifyUp();
			Assert::AreEqual(false, btn->IsHot());
			Assert::AreEqual(false, btn->IsPressed());

		}
	};
}