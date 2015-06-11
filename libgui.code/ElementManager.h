#pragma once 
#include "Element.h"
#include "Control.h"

namespace libgui
{
	class ElementManager : public std::enable_shared_from_this<ElementManager>
	{
	public:
		void SetRoot(shared_ptr<Element>);

		shared_ptr<Element> GetRoot();

		bool NotifyMouseMove(int x, int y);
		bool NotifyMouseDown(int x, int y);
		bool NotifyMouseUp(int x, int y);

		bool NotifyTouchMove(double touchX, double touchY);
		bool NotifyTouchDown(double touchX, double touchY);
		bool NotifyTouchUp(double touchX, double touchY);

		void RequestCapture(shared_ptr<Control>);
		void ReleaseCapture();

		const function<void(bool)>& GetSystemCaptureCallback() const;
		void SetSystemCaptureCallback(const function<void(bool)>& systemCaptureCallback);

	private:
		weak_ptr<Control> m_capturedControl;
		weak_ptr<Control> m_activeControl;

		shared_ptr<Element> m_root = nullptr;

		function<void(bool)> m_systemCaptureCallback;

		bool NotifyMove(int x, int y, bool isMouse);
		bool NotifyUp(int x, int y, bool isMouse);

	};
}
