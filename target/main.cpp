#include "../lib/STM32f302x8-HAL/llpd/include/LLPD.hpp"

#include <math.h>

#include "EEPROM_CAT24C64.hpp"
#include "SRAM_23K256.hpp"
#include "SDCard.hpp"
#include "OLED_SH1106.hpp"
#include "AudioBuffer.hpp"
#include "GlintManager.hpp"
#include "GlintUiManager.hpp"
#include "IPotEventListener.hpp"
#include "IButtonEventListener.hpp"
#include "IGlintLCDRefreshEventListener.hpp"
#include "FrameBuffer.hpp"

// assets
#include "GlintMainImage.h"
#include "Smoll.h"

// to disassemble -- arm-none-eabi-objdump -S --disassemble main_debug.elf > disassembled.s

#define SYS_CLOCK_FREQUENCY 72000000

// global variables
volatile bool glintReady = false; // should be set to true after everything has been initialized
AudioBuffer<uint16_t>* audioBufferPtr = nullptr;

// peripheral defines
#define OP_AMP_PORT 		GPIO_PORT::A
#define OP_AMP_INVERT_PIN 	GPIO_PIN::PIN_5
#define OP_AMP_OUTPUT_PIN 	GPIO_PIN::PIN_6
#define OP_AMP_NON_INVERT_PIN 	GPIO_PIN::PIN_7
#define EFFECT1_ADC_PORT 	GPIO_PORT::A
#define EFFECT1_ADC_PIN 	GPIO_PIN::PIN_0
#define EFFECT1_ADC_CHANNEL 	ADC_CHANNEL::CHAN_1
#define EFFECT2_ADC_PORT 	GPIO_PORT::A
#define EFFECT2_ADC_PIN 	GPIO_PIN::PIN_1
#define EFFECT2_ADC_CHANNEL 	ADC_CHANNEL::CHAN_2
#define EFFECT3_ADC_PORT 	GPIO_PORT::A
#define EFFECT3_ADC_PIN 	GPIO_PIN::PIN_2
#define EFFECT3_ADC_CHANNEL 	ADC_CHANNEL::CHAN_3
#define AUDIO_IN_PORT 		GPIO_PORT::A
#define AUDIO_IN_PIN  		GPIO_PIN::PIN_3
#define AUDIO_IN_CHANNEL 	ADC_CHANNEL::CHAN_4
#define EFFECT1_BUTTON_PORT 	GPIO_PORT::B
#define EFFECT1_BUTTON_PIN 	GPIO_PIN::PIN_0
#define EFFECT2_BUTTON_PORT 	GPIO_PORT::B
#define EFFECT2_BUTTON_PIN 	GPIO_PIN::PIN_1
#define SRAM1_CS_PORT 		GPIO_PORT::B
#define SRAM1_CS_PIN 		GPIO_PIN::PIN_12
#define SRAM2_CS_PORT 		GPIO_PORT::B
#define SRAM2_CS_PIN 		GPIO_PIN::PIN_2
#define SRAM3_CS_PORT 		GPIO_PORT::B
#define SRAM3_CS_PIN 		GPIO_PIN::PIN_3
#define SRAM4_CS_PORT 		GPIO_PORT::B
#define SRAM4_CS_PIN 		GPIO_PIN::PIN_4
#define EEPROM1_ADDRESS 	false, false, false
#define EEPROM2_ADDRESS 	true, false, false
#define SDCARD_CS_PORT 		GPIO_PORT::A
#define SDCARD_CS_PIN 		GPIO_PIN::PIN_11
#define OLED_RESET_PORT 	GPIO_PORT::B
#define OLED_RESET_PIN 		GPIO_PIN::PIN_7
#define OLED_DC_PORT 		GPIO_PORT::B
#define OLED_DC_PIN 		GPIO_PIN::PIN_8
#define OLED_CS_PORT 		GPIO_PORT::B
#define OLED_CS_PIN 		GPIO_PIN::PIN_9

// a simple class to handle lcd refresh events
class Oled_Manager : public IGlintLCDRefreshEventListener
{
	public:
		Oled_Manager (uint8_t* displayBuffer) :
			m_Oled( SPI_NUM::SPI_2, OLED_CS_PORT, OLED_CS_PIN, OLED_DC_PORT, OLED_DC_PIN, OLED_RESET_PORT, OLED_RESET_PIN ),
			m_DisplayBuffer( displayBuffer )
		{
			LLPD::gpio_output_setup( OLED_CS_PORT, OLED_CS_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_CS_PORT, OLED_CS_PIN, true );
			LLPD::gpio_output_setup( OLED_DC_PORT, OLED_DC_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_DC_PORT, OLED_DC_PIN, true );
			LLPD::gpio_output_setup( OLED_RESET_PORT, OLED_RESET_PIN, GPIO_PUPD::NONE, GPIO_OUTPUT_TYPE::PUSH_PULL,
							GPIO_OUTPUT_SPEED::HIGH, false );
			LLPD::gpio_output_set( OLED_RESET_PORT, OLED_RESET_PIN, true );

			m_Oled.begin();

			m_Oled.setRefreshRatePrescaler( REFRESH_RATE_PRESCALE::BY_10 );

			this->bindToGlintLCDRefreshEventSystem();
		}
		~Oled_Manager() override
		{
			this->unbindFromGlintLCDRefreshEventSystem();
		}

		void onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent) override
		{
			unsigned int columnStart = lcdRefreshEvent.getXStart();
			unsigned int rowStart = lcdRefreshEvent.getYStart();
			unsigned int columnEnd = lcdRefreshEvent.getXEnd();
			unsigned int rowEnd = lcdRefreshEvent.getYEnd();
			if ( columnEnd - columnStart == SH1106_LCDWIDTH && rowEnd - rowStart == SH1106_LCDHEIGHT )
			{
				m_Oled.displayFullRowMajor( m_DisplayBuffer );
			}
			else
			{
				m_Oled.displayPartialRowMajor( m_DisplayBuffer, rowEnd, columnEnd, rowStart, columnStart );
			}
		}

	private:
		Oled_SH1106 	m_Oled;
		uint8_t* 	m_DisplayBuffer;
};

// these pins are unconnected on Gen_FX_SYN Rev 2 development board, so we disable them as per the ST recommendations
void disableUnusedPins()
{
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_13, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_14, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::C, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_8, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_12, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::A, GPIO_PIN::PIN_15, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );

	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_2, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_3, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_4, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_5, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
	LLPD::gpio_output_setup( GPIO_PORT::B, GPIO_PIN::PIN_6, GPIO_PUPD::PULL_DOWN, GPIO_OUTPUT_TYPE::PUSH_PULL,
					GPIO_OUTPUT_SPEED::LOW );
}

int main(void)
{
	// dma may still be running from the last reset
	LLPD::adc_dma_stop();
	LLPD::dac_dma_stop();

	// set system clock to PLL with HSE (16MHz / 2) as input, so 72MHz system clock speed
	LLPD::rcc_clock_setup( RCC_CLOCK_SOURCE::EXTERNAL, true, RCC_PLL_MULTIPLY::BY_9, SYS_CLOCK_FREQUENCY );

	// prescale APB1 by 2, since the maximum clock speed is 36MHz
	LLPD::rcc_set_periph_clock_prescalers( RCC_AHB_PRES::BY_1, RCC_APB1_PRES::AHB_BY_2, RCC_APB2_PRES::AHB_BY_1 );

	// enable all gpio clocks
	LLPD::gpio_enable_clock( GPIO_PORT::A );
	LLPD::gpio_enable_clock( GPIO_PORT::B );
	LLPD::gpio_enable_clock( GPIO_PORT::C );
	LLPD::gpio_enable_clock( GPIO_PORT::F );

	// USART setup
	LLPD::usart_init( USART_NUM::USART_3, USART_WORD_LENGTH::BITS_8, USART_PARITY::EVEN, USART_CONF::TX_AND_RX,
				USART_STOP_BITS::BITS_1, SYS_CLOCK_FREQUENCY, 9600 );
	LLPD::usart_log( USART_NUM::USART_3, "Gen_FX_SYN starting up -----------------------------" );

	// disable the unused pins
	disableUnusedPins();

	// i2c setup (72MHz source 1000KHz clock 0x00A00D26)
	LLPD::i2c_master_setup( I2C_NUM::I2C_2, 0x00A00D26 );
	LLPD::usart_log( USART_NUM::USART_3, "I2C initialized..." );

	// spi init (36MHz SPI2 source 18MHz clock)
	LLPD::spi_master_init( SPI_NUM::SPI_2, SPI_BAUD_RATE::APB1CLK_DIV_BY_2, SPI_CLK_POL::LOW_IDLE, SPI_CLK_PHASE::FIRST,
				SPI_DUPLEX::FULL, SPI_FRAME_FORMAT::MSB_FIRST, SPI_DATA_SIZE::BITS_8 );
	LLPD::usart_log( USART_NUM::USART_3, "spi initialized..." );

	// audio timer setup (for 40 kHz sampling rate at 72 MHz system clock)
	LLPD::tim6_counter_setup( 0, 1800, 40000 );
	LLPD::tim3_counter_setup( 0, 1800, 40000 );
	LLPD::tim6_counter_enable_interrupts();
	LLPD::usart_log( USART_NUM::USART_3, "tim6 initialized..." );
	LLPD::usart_log( USART_NUM::USART_3, "tim3 initialized..." );

	// set up audio buffer for use with adc and dac dma
	AudioBuffer<uint16_t> audioBuffer;
	audioBufferPtr = &audioBuffer;

	// DAC setup
	// LLPD::dac_init( true ); // for interrupt based audio
	LLPD::dac_init_use_dma( true, ABUFFER_SIZE * 2, (uint16_t*) audioBuffer.getBuffer1() );
	LLPD::usart_log( USART_NUM::USART_3, "dac initialized..." );

	// Op Amp setup
	LLPD::gpio_analog_setup( OP_AMP_PORT, OP_AMP_INVERT_PIN );
	LLPD::gpio_analog_setup( OP_AMP_PORT, OP_AMP_OUTPUT_PIN );
	LLPD::gpio_analog_setup( OP_AMP_PORT, OP_AMP_NON_INVERT_PIN );
	LLPD::opamp_init();
	LLPD::usart_log( USART_NUM::USART_3, "op amp initialized..." );

	// audio timer start
	LLPD::tim6_counter_start();
	LLPD::tim3_counter_start();
	LLPD::tim3_sync_to_tim6();
	LLPD::usart_log( USART_NUM::USART_3, "tim6 started..." );
	LLPD::usart_log( USART_NUM::USART_3, "tim3 started..." );

	// ADC setup (note, this must be done after the tim6_counter_start() call since it uses the delay function)
	LLPD::rcc_pll_enable( RCC_CLOCK_SOURCE::INTERNAL, false, RCC_PLL_MULTIPLY::NONE );
	LLPD::gpio_analog_setup( EFFECT1_ADC_PORT, EFFECT1_ADC_PIN );
	LLPD::gpio_analog_setup( EFFECT2_ADC_PORT, EFFECT2_ADC_PIN );
	LLPD::gpio_analog_setup( EFFECT3_ADC_PORT, EFFECT3_ADC_PIN );
	LLPD::gpio_analog_setup( AUDIO_IN_PORT, AUDIO_IN_PIN );
	LLPD::adc_init( ADC_CYCLES_PER_SAMPLE::CPS_19p5 );
	// for interrupt based audio
	LLPD::adc_set_channel_order( false, 4, ADC_CHANNEL::CHAN_INVALID, nullptr, 0,
					EFFECT1_ADC_CHANNEL, EFFECT2_ADC_CHANNEL, EFFECT3_ADC_CHANNEL, AUDIO_IN_CHANNEL );
	LLPD::adc_set_channel_order( true, 4, AUDIO_IN_CHANNEL, (uint32_t*) audioBuffer.getBuffer1(), ABUFFER_SIZE * 2,
					EFFECT1_ADC_CHANNEL, EFFECT2_ADC_CHANNEL, EFFECT3_ADC_CHANNEL, AUDIO_IN_CHANNEL );
	LLPD::usart_log( USART_NUM::USART_3, "adc initialized..." );

	// pushbutton setup
	LLPD::gpio_digital_input_setup( EFFECT1_BUTTON_PORT, EFFECT1_BUTTON_PIN, GPIO_PUPD::PULL_UP );
	LLPD::gpio_digital_input_setup( EFFECT2_BUTTON_PORT, EFFECT2_BUTTON_PIN, GPIO_PUPD::PULL_UP );

	// SRAM setup and test
	std::vector<Sram_23K256_GPIO_Config> spiGpioConfigs;
	spiGpioConfigs.emplace_back( SRAM1_CS_PORT, SRAM1_CS_PIN );
	spiGpioConfigs.emplace_back( SRAM2_CS_PORT, SRAM2_CS_PIN );
	spiGpioConfigs.emplace_back( SRAM3_CS_PORT, SRAM3_CS_PIN );
	spiGpioConfigs.emplace_back( SRAM4_CS_PORT, SRAM4_CS_PIN );
	Sram_23K256_Manager srams( SPI_NUM::SPI_2, spiGpioConfigs );
	SharedData<uint8_t> sramValsToWrite = SharedData<uint8_t>::MakeSharedData( 3 );
	sramValsToWrite[0] = 25; sramValsToWrite[1] = 16; sramValsToWrite[2] = 8;
	srams.writeToMedia( sramValsToWrite, 45 );
	srams.writeToMedia( sramValsToWrite, 45 + Sram_23K256::SRAM_SIZE );
	srams.writeToMedia( sramValsToWrite, 45 + Sram_23K256::SRAM_SIZE * 2 );
	srams.writeToMedia( sramValsToWrite, 45 + Sram_23K256::SRAM_SIZE * 3 );
	SharedData<uint8_t> sram1Verification = srams.readFromMedia( 3, 45 );
	SharedData<uint8_t> sram2Verification = srams.readFromMedia( 3, 45 + Sram_23K256::SRAM_SIZE );
	SharedData<uint8_t> sram3Verification = srams.readFromMedia( 3, 45 + Sram_23K256::SRAM_SIZE * 2 );
	SharedData<uint8_t> sram4Verification = srams.readFromMedia( 3, 45 + Sram_23K256::SRAM_SIZE * 3 );
	if ( sram1Verification[0] == 25 && sram1Verification[1] == 16 && sram1Verification[2] == 8 &&
			sram2Verification[0] == 25 && sram2Verification[1] == 16 && sram2Verification[2] == 8 &&
			sram3Verification[0] == 25 && sram3Verification[1] == 16 && sram3Verification[2] == 8 &&
			sram4Verification[0] == 25 && sram4Verification[1] == 16 && sram4Verification[2] == 8 )
	{
		LLPD::usart_log( USART_NUM::USART_3, "srams verified..." );
	}
	else
	{
		LLPD::usart_log( USART_NUM::USART_3, "WARNING!!! srams failed verification..." );
	}
	srams.setSequentialMode( true );

	LLPD::usart_log( USART_NUM::USART_3, "Gen_FX_SYN setup complete, entering while loop -------------------------------" );

	GlintManager glintManager( &srams );
	GlintUiManager glintUiManager( Smoll_data, GlintMainImage_data );

	Oled_Manager oled( glintUiManager.getFrameBuffer()->getPixels() );
	LLPD::usart_log( USART_NUM::USART_3, "oled initialized..." );

	// initial drawing of the UI
	glintUiManager.draw();

	audioBuffer.registerCallback( &glintManager );

	glintReady = true;

	// comment out for interrupt based audio
	LLPD::tim6_counter_disable_interrupts();
	LLPD::dac_dma_start();
	LLPD::adc_dma_start();

	// TODO interestingly, compiling this and loading on my desktop after having loaded an image that uses dma, causes the
	// newly loaded image to WWDG_Interrupt...
	// I need to check if this is also the case with images built with my laptop's toolchain. If it is the case, do I need
	// to reset the dma registers first or something? Something's funky
	//
	// now I've found out that I'm getting a hard fault, possibly with MSTKERR (though I need to verify this)
	// when I comment out glintManager initialization and registering it with the audio buffer, no hard fault
	// is this maybe just a stack overflow? Need to check by removing things from glint manager
	//
	// Okay, I finally have the answer, the issue is that dma (for both dac and adc) needs to be stopped before programming.
	// The programmer must use some of that memory, so dma must corrupt it during programming. The current work around for
	// this is to step until you get past adc_dma_stop() and dac_dma_stop(), then load the program since dma won't be
	// corrupting anymore. This really needs to be fixed on Aki-delay and the template project as well though, so a more
	// robust solution will be needed. Possibly a custom Reset_Handler?

	while ( true )
	{
		// No button interactions
		// if ( ! LLPD::gpio_input_get(EFFECT1_BUTTON_PORT, EFFECT1_BUTTON_PIN) )
		// {
		// 	IButtonEventListener::PublishEvent(
		// 			ButtonEvent(BUTTON_STATE::PRESSED, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT_BTN_1)) );
		// }
		// else
		// {
		// 	IButtonEventListener::PublishEvent(
		// 			ButtonEvent(BUTTON_STATE::RELEASED, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT_BTN_1)) );
		// }

		// if ( ! LLPD::gpio_input_get(EFFECT2_BUTTON_PORT, EFFECT2_BUTTON_PIN) )
		// {
		// 	IButtonEventListener::PublishEvent(
		// 			ButtonEvent(BUTTON_STATE::PRESSED, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT_BTN_2)) );
		// }
		// else
		// {
		// 	IButtonEventListener::PublishEvent(
		// 			ButtonEvent(BUTTON_STATE::RELEASED, static_cast<unsigned int>(BUTTON_CHANNEL::EFFECT_BTN_2)) );
		// }

		uint16_t pot1Val = LLPD::adc_get_channel_value( EFFECT1_ADC_CHANNEL );
		float pot1Percentage = static_cast<float>( pot1Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(pot1Percentage, static_cast<unsigned int>(POT_CHANNEL::DECAY_TIME)) );

		uint16_t pot2Val = LLPD::adc_get_channel_value( EFFECT2_ADC_CHANNEL );
		float pot2Percentage = static_cast<float>( pot2Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(pot2Percentage, static_cast<unsigned int>(POT_CHANNEL::MOD_RATE)) );

		uint16_t pot3Val = LLPD::adc_get_channel_value( EFFECT3_ADC_CHANNEL );
		float pot3Percentage = static_cast<float>( pot3Val ) * ( 1.0f / 4095.0f );
		IPotEventListener::PublishEvent( PotEvent(pot3Percentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );

		// code for interrupt based audio
		// audioBuffer.pollToFillBuffers();

		// comment out for interrupt based audio
		const unsigned int numDacTransfersLeft = LLPD::dac_dma_get_num_transfers_left();
		bool buffer1Filled = ! audioBuffer.buffer1IsNextToWrite();

		if ( buffer1Filled && numDacTransfersLeft < ABUFFER_SIZE )
		{
			// fill buffer 2
			audioBuffer.triggerCallbacksOnNextPoll( false );
			audioBuffer.pollToFillBuffers();
		}
		else if ( ! buffer1Filled && numDacTransfersLeft >= ABUFFER_SIZE )
		{
			// fill buffer 1
			audioBuffer.triggerCallbacksOnNextPoll( true );
			audioBuffer.pollToFillBuffers();
		}
	}
}

extern "C" void TIM6_DAC_IRQHandler (void)
{
	if ( ! LLPD::tim6_isr_handle_delay() ) // if not currently in a delay function,...
	{
		// code for interrupt based audio
		// if ( glintReady )
		// {
		// 	uint16_t adcVal = LLPD::adc_get_channel_value( ADC_CHANNEL::CHAN_4 );

		// 	uint16_t outVal = audioBufferPtr->getNextSample( adcVal );

		// 	LLPD::dac_send( outVal );

		// 	LLPD::adc_perform_conversion_sequence();
		// }
	}

	LLPD::tim6_counter_clear_interrupt_flag();
}

extern "C" void USART3_IRQHandler (void)
{
	/*
	// loopback test code for usart recieve
	uint16_t data = LLPD::usart_receive( USART_NUM::USART_3 );
	LLPD::usart_transmit( USART_NUM::USART_3, data );
	*/
}

extern "C" void HardFault_Handler (void)
{
	while (1)
	{
		LLPD::usart_log( USART_NUM::USART_3, "Hard Faulting -----------------------------" );
	}
}

extern "C" void MemManage_Handler (void)
{
	while (1)
	{
		LLPD::usart_log( USART_NUM::USART_3, "Mem Manage Faulting -----------------------------" );
	}
}

extern "C" void BusFault_Handler (void)
{
	while (1)
	{
		LLPD::usart_log( USART_NUM::USART_3, "Bus Faulting -----------------------------" );
	}
}

extern "C" void UsageFault_Handler (void)
{
	while (1)
	{
		LLPD::usart_log( USART_NUM::USART_3, "Usage Faulting -----------------------------" );
	}
}
