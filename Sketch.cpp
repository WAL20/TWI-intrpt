#include <Arduino.h>

#define OLEDaddr  0x3c
#define MPUaddr 0x68
#define busy (deviceAddr || (TWCR & 1<<TWSTO))

uint8_t OLEDconfig[] = {29, 0x00, 0xae, 0xd5, 0x80, 0xa8, 0x1f,
							0xd3, 0x00, 0x40, 0x00, 0x8d, 0x14,
							0x20, 0x00, 0xa0, 0x01, 0xc8, 0xda,
							0x02, 0x81, 0x7f, 0xd9, 0xf1, 0xdb,
							0x40, 0xa4, 0xa6, 0xaf,
						0};

uint8_t MPUreset[] = {3, 0x6b, 0x00,
						0};

uint8_t MPUread[] = {3, 0x3b, 0x06,
						0x80+6, 0,0,0,0,0,0};

uint8_t * buffer;
volatile uint8_t deviceAddr = 0;
uint8_t idx;

void setup() {
	DDRD |= (1<<PORTD5);
	TWSR = 0 ;
	TWBR = 72 ;

	deviceAddr = MPUaddr<<1;
	buffer = MPUreset;
	TWCR = (1<<TWEN) | (1<<TWSTA) | (1<<TWINT) | (1<<TWIE);

	while (busy);
	deviceAddr = OLEDaddr<<1;
	buffer = OLEDconfig;
	TWCR = (1<<TWEN) | (1<<TWSTA) | (1<<TWINT) | (1<<TWIE);
	}

void loop() {
	if (!busy) {
		deviceAddr = MPUaddr<<1;
		buffer = MPUread;
		TWCR = (1<<TWEN) | (1<<TWSTA) | (1<<TWINT) | (1<<TWIE);
		}
	PORTD |= (1<<PORTD5);
	_delay_us(20);
	PORTD &= ~(1<<PORTD5);
	_delay_us(20);
	}

ISR(TWI_vect) {
	switch (TWSR & 0xF8) {
		case(0x08):						// START condition has been set.
		case(0x10):						// Repeated START has been set.
			idx = 1;
			TWDR = deviceAddr;
			TWCR = (1<<TWEN) | (1<<TWINT) | (1<<TWIE);
			break;
		case(0x18):						// ACK received after sending SLA+W.
		case(0x28):						// ACK received after sending DATA.
			if (idx < buffer[0]) {
				TWDR = buffer[idx++] ;
				TWCR = (1<<TWEN) | (1<<TWINT) | (1<<TWIE);
				break;
				}
			else {
				buffer += idx;
				if (buffer[0] & 0x80)
					deviceAddr |= 1;
				if (buffer[0] & 0x7f) {
					TWCR = (1<<TWEN) | (1<<TWINT) | (1<<TWIE) | (1<<TWSTA);
					break;
					}
				}
		case(0x00):						// Bus error. See ATmega328P Datasheet section 21.7.5.
		case(0x20):						// NACK received after sending SLA+W.
		case(0x30):						// NACK received after sending DATA.
			deviceAddr = 0;
			TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN);
			break;
		case (0x50):							// Data byte received, ACK returned.
			buffer[idx++] = TWDR;
		case (0x40):							// SLA+R sent, ACK received.
			if (idx<(buffer[0] & 0x7f))
				TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWIE) | (1<<TWEA);
			else
				TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWIE);
			break;
		case (0x58):					// Data byte received, NACK returned.
			buffer[idx] = TWDR;
		default:
		case (0x48):					// SLA+R sent, NACK received.
			deviceAddr = 0;
			TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN);
		case(0x38):					// Arbitration lost.
		case (0xf8):	// See section 21.7.5. Nothing is happening - do nothing.
			break;		// TWINT is not set - interrupt shouldn't have happened.
		}
	}
