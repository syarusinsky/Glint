#ifndef IGLINTLCDREFRESHEVENTLISTENER_HPP
#define IGLINTLCDREFRESHEVENTLISTENER_HPP

#include "IEventListener.hpp"

class GlintLCDRefreshEvent : public IEvent
{
	public:
		GlintLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
						unsigned int channel);
		~GlintLCDRefreshEvent() override;

		unsigned int getXStart() const;
		unsigned int getYStart() const;
		unsigned int getXEnd() const;
		unsigned int getYEnd() const;

	private:
		unsigned int m_XStart;
		unsigned int m_YStart;
		unsigned int m_XEnd;
		unsigned int m_YEnd;
};

class IGlintLCDRefreshEventListener : public IEventListener
{
	public:
		virtual ~IGlintLCDRefreshEventListener();

		virtual void onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent) = 0;

		void bindToGlintLCDRefreshEventSystem();
		void unbindFromGlintLCDRefreshEventSystem();

		static void PublishEvent (const GlintLCDRefreshEvent& lcdRefreshEvent);

	private:
		static EventDispatcher<IGlintLCDRefreshEventListener, GlintLCDRefreshEvent,
					&IGlintLCDRefreshEventListener::onGlintLCDRefreshEvent> m_EventDispatcher;
};

#endif // IGLINTLCDREFRESHEVENTLISTENER_HPP
