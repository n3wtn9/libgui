#pragma once

#include "Resources.h"

namespace libgui_sample_windows
{
	class SharedResources : public Resources
	{
	public:
		static SharedResources* Get()
		{
			static SharedResources resources;
			return &resources;
		}

		ID2D1SolidColorBrush* LightGrayBrush;
		ID2D1SolidColorBrush* GrayBrush;
		ID2D1SolidColorBrush* LightRedBrush;

		virtual HRESULT Create(ID2D1Factory* factory, ID2D1HwndRenderTarget* target)
		{
			HRESULT hr;
			hr = target->CreateSolidColorBrush(
				D2D1::ColorF(0xCDCDCD),
				&GrayBrush
				);

			if (SUCCEEDED(hr))
			{
				hr = target->CreateSolidColorBrush(
					D2D1::ColorF(0xEBEBEB),
					&LightGrayBrush
					);
			}

			if (SUCCEEDED(hr))
			{
				hr = target->CreateSolidColorBrush(
					D2D1::ColorF(0xFFD2D4),
					&LightRedBrush
					);
			}

			return hr;
		}

		virtual void Discard()
		{
			SafeRelease(&GrayBrush);
			SafeRelease(&LightGrayBrush);
			SafeRelease(&LightRedBrush);
		}

	private:
		// Singleton: Disallow creating an instance of the class directly
		SharedResources() {}
		// Singleton: Disallow creating an instance of the class directly
		SharedResources(SharedResources const&) = delete;
		// Singleton: Disallow creating an instance of the class directly
		void operator=(SharedResources const&) = delete;
	};
}