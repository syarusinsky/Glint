#ifndef IGLINTPARAMETEREVENTLISTENER_HPP
#define IGLINTPARAMETEREVENTLISTENER_HPP

#include "IEventListener.hpp"

class GlintParameterEvent : public IEvent
{
	public:
		GlintParameterEvent (float value, unsigned int channel);
		~GlintParameterEvent() override;

		float getValue() const;

	private:
		float m_Value;
};

class IGlintParameterEventListener : public IEventListener
{
	public:
		virtual ~IGlintParameterEventListener();

		virtual void onGlintParameterEvent (const GlintParameterEvent& paramEvent) = 0;

		void bindToGlintParameterEventSystem();
		void unbindFromGlintParameterEventSystem();

		static void PublishEvent (const GlintParameterEvent& paramEvent);

	private:
		static EventDispatcher<IGlintParameterEventListener, GlintParameterEvent,
					&IGlintParameterEventListener::onGlintParameterEvent> m_EventDispatcher;
};

#endif // IGLINTPARAMETEREVENTLISTENER_HPP
