#ifndef IGLINTPRESETEVENTLISTENER_HPP
#define IGLINTPRESETEVENTLISTENER_HPP

/*******************************************************************
 * An IGlintPresetEventListener specifies a simple interface which
 * a subclass can use to be notified of Glint preset events.
*******************************************************************/

#include "GlintManager.hpp"
#include "IEventListener.hpp"

enum class GlintPresetEventTypeEnum
{
	LOAD_PRESET,
	SEND_PRESET_REQUEST,
	SEND_ALL_PRESETS_REQUEST,
	ACCEPT_PRESET,
	ACCEPT_ALL_PRESETS,
	DENY_PRESET,
	FINISHED_SENDING_OR_RECEIVING_PRESETS,
};

class GlintPresetEvent : public IEvent
{
	public:
		GlintPresetEvent (const GlintState& preset, unsigned int presetNum, unsigned int channel,
					const GlintPresetEventTypeEnum& type = GlintPresetEventTypeEnum::LOAD_PRESET);
		~GlintPresetEvent() override;

		GlintState getPreset() const { return m_Preset; }
		unsigned int getPresetNum() const { return m_PresetNum; }

		GlintPresetEventTypeEnum getType() const { return m_Type; }

	private:
		GlintState 			m_Preset;
		unsigned int 			m_PresetNum;
		GlintPresetEventTypeEnum 	m_Type;
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
