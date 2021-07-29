#include "IGlintParameterEventListener.hpp"

// instantiating IGlintParameterEventListener's event dispatcher
EventDispatcher<IGlintParameterEventListener, GlintParameterEvent,
		&IGlintParameterEventListener::onGlintParameterEvent> IGlintParameterEventListener::m_EventDispatcher;

GlintParameterEvent::GlintParameterEvent (float value, unsigned int channel) :
	IEvent( channel ),
	m_Value( value )
{
}

GlintParameterEvent::~GlintParameterEvent()
{
}

float GlintParameterEvent::getValue() const
{
	return m_Value;
}

IGlintParameterEventListener::~IGlintParameterEventListener()
{
	this->unbindFromGlintParameterEventSystem();
}

void IGlintParameterEventListener::bindToGlintParameterEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IGlintParameterEventListener::unbindFromGlintParameterEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IGlintParameterEventListener::PublishEvent (const GlintParameterEvent& paramEvent)
{
	m_EventDispatcher.dispatch( paramEvent );
}
