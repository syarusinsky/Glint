#include "IGlintPresetEventListener.hpp"

// instantiating IGlintPresetEventListener's event dispatcher
EventDispatcher<IGlintPresetEventListener, GlintPresetEvent,
		&IGlintPresetEventListener::onGlintPresetChangedEvent> IGlintPresetEventListener::m_EventDispatcher;

GlintPresetEvent::GlintPresetEvent (const GlintState& preset, unsigned int presetNum, unsigned int channel) :
	IEvent( channel ),
	m_Preset( preset ),
	m_PresetNum( presetNum )
{
}

GlintPresetEvent::~GlintPresetEvent()
{
}

IGlintPresetEventListener::~IGlintPresetEventListener()
{
	this->unbindFromGlintPresetEventSystem();
}

void IGlintPresetEventListener::bindToGlintPresetEventSystem()
{
	m_EventDispatcher.bind( this );
}

void IGlintPresetEventListener::unbindFromGlintPresetEventSystem()
{
	m_EventDispatcher.unbind( this );
}

void IGlintPresetEventListener::PublishEvent (const GlintPresetEvent& preset)
{
	m_EventDispatcher.dispatch( preset );
}
