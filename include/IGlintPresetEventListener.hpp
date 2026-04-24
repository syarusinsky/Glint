#ifndef IGLINTPRESETEVENTLISTENER_HPP
#define IGLINTPRESETEVENTLISTENER_HPP

/*******************************************************************
 * An IGlintPresetEventListener specifies a simple interface which
 * a subclass can use to be notified of Glint preset events.
*******************************************************************/

#include "GlintManager.hpp"
#include "IEventListener.hpp"

class GlintPresetEvent : public IEvent
{
	public:
		GlintPresetEvent (const GlintState& preset, unsigned int presetNum, unsigned int channel);
		~GlintPresetEvent() override;

		GlintState getPreset() const { return m_Preset; }
		unsigned int getPresetNum() const { return m_PresetNum; }

	private:
		GlintState m_Preset;
		unsigned int m_PresetNum;
};

class IGlintPresetEventListener : public IEventListener
{
	public:
		virtual ~IGlintPresetEventListener();

		virtual void onGlintPresetChangedEvent (const GlintPresetEvent& preset) = 0;

		void bindToGlintPresetEventSystem();
		void unbindFromGlintPresetEventSystem();

		static void PublishEvent (const GlintPresetEvent& preset);

	private:
		static EventDispatcher<IGlintPresetEventListener, GlintPresetEvent,
					&IGlintPresetEventListener::onGlintPresetChangedEvent> m_EventDispatcher;
};

#endif // IGLINTPRESETLISTENER_HPP
