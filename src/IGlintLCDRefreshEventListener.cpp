#include "IGlintLCDRefreshEventListener.hpp"

// instantiating IGlintLCDRefreshEventListener's event dispatcher
EventDispatcher<IGlintLCDRefreshEventListener, GlintLCDRefreshEvent,
		&IGlintLCDRefreshEventListener::onGlintLCDRefreshEvent> IGlintLCDRefreshEventListener::m_EventDispatcher;

GlintLCDRefreshEvent::GlintLCDRefreshEvent (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd,
							unsigned int channel) :
	IEvent( channel ),
	m_XStart( xStart ),
	m_YStart( yStart ),
	m_XEnd( xEnd ),
	m_YEnd( yEnd )
{
}

GlintLCDRefreshEvent::~GlintLCDRefreshEvent()
{
}

unsigned int GlintLCDRefreshEvent::getXStart() const
{
	return m_XStart;
}

unsigned int GlintLCDRefreshEvent::getYStart() const
{
	return m_YStart;
}

unsigned int GlintLCDRefreshEvent::getXEnd() const
{
	return m_XEnd;
}

unsigned int GlintLCDRefreshEvent::getYEnd() const
{
	return m_YEnd;
}

IGlintLCDRefreshEventListener::~IGlintLCDRefreshEventListener()
{
	this->unbindFromGlintLCDRefreshEventSystem();
}

void IGlintLCDRefreshEventListener::bindToGlintLCDRefreshEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IGlintLCDRefreshEventListener::unbindFromGlintLCDRefreshEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IGlintLCDRefreshEventListener::PublishEvent (const GlintLCDRefreshEvent& lcdRefreshEvent)
{
	m_EventDispatcher.dispatch( lcdRefreshEvent );
}
