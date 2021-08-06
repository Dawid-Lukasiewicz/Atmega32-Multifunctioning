/*
 * Wykorzystanie mikrokontrolera do 4 roznych funkcji i zawarcie nich w interaktywnym menu.
 * -Liczby od 0 do 50, sprawdzenie ich parzystosci i czy sa liczbami pierwszymi
 * -Stoper
 * -Zegar do godziny
 * -Miernik, przetwarzanie analogowo-cyfrowe
 * */

#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/sfr_defs.h>
#include <math.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "HD44780.h"

#ifndef _BV
#define _BV(bit)				(1<<(bit))
#endif

#ifndef sbi
#define sbi(reg,bit)		reg |= (_BV(bit))
#endif

#ifndef cbi
#define cbi(reg,bit)		reg &= ~(_BV(bit))
#endif

#ifndef tbi
#define tbi(reg,bit)		reg ^= (_BV(bit))
#endif

#define 	bit_is_set(sfr, bit)   (_SFR_BYTE(sfr) & _BV(bit))
#define 	bit_is_clear(sfr, bit)   (!(_SFR_BYTE(sfr) & _BV(bit)))

// Konfiguruje porty przyciskow klawiatury
void configure_buttons_ports () {
	// Ustawiam porty wierszy jako wejscia
	cbi(DDRD, PD4);
	cbi(DDRD, PD5);
	// Ustawiam porty wierszy na stan wysoki
	sbi(PORTD, PD4);
	sbi(PORTD, PD5);
	// Ustawiam porty kolumn jako wyjscia
	sbi(DDRD, PD6);
	sbi(DDRD, PD7);
	// Ustawiam porty kolumn na stan wysoki
	sbi(PORTD, PD6);
	sbi(PORTD, PD7);
}

// Konfiguruje porty wyswietlacza 7-segmentowego
void SEG_Initialize () {
	// Ustawiam porty jako wyjscia
	DDRC = 0xff;
	// Ustawiam porty na stan niski
	PORTC = 0x00;
}

// Konfiguruje porty diod led
void LED_Initialize () {
	// Ustawiam porty jako wyj�cia
	sbi(DDRD, PD0);
	sbi(DDRD, PD1);
	sbi(DDRD, PD2);
	// Ustawiam porty na stan niski
	cbi(PORTD, PD0);
	cbi(PORTD, PD1);
	cbi(PORTD, PD2);
}

// Konfiguruje licznik dla przerwan z czestotliwoscia 10 Hz
void Timer_Stoper_Initialize () {
	// Wybor trybu pracy CTC z TOP OCR1A
	sbi(TCCR1B, WGM12);
	// Wybor dzielnika czestotliwosci (preskalera 64)
	sbi(TCCR1B, CS10);
	sbi(TCCR1B, CS11);
	cbi(TCCR1B, CS12);
	// Zapis do OCR1A wartosci odpowiadajacej 0,1 s
	OCR1A = 12500;
	// Uruchomienie przerwania OCIE1A
	sbi(TIMSK, OCIE1A);
}

// Konfiguruje licznik dla przerwan z czestotliwoscia 1 Hz
void Timer_Zegar_Initialize () {
	// Wybor trybu pracy CTC z TOP OCR1A
	sbi(TCCR1B, WGM12);
	// Wybor dzielnika czestotliwosci (preskalera 256)
	cbi(TCCR1B, CS10);
	cbi(TCCR1B, CS11);
	sbi(TCCR1B, CS12);
	// Zapis do OCR1A wartosci odpowiadajacej 1 s
	OCR1A = 31250;
	// Uruchomienie przerwania OCIE1A
	sbi(TIMSK, OCIE1A);
}

// Konfiguruje przetwornik dla pomiaru napiecia
void ADC_Initialize () {
	// Konfiguracja napiecia referencyjnego na AVCC
	sbi(ADMUX, REFS0);
	cbi(ADMUX, REFS1);
	// Konfiguracja czestotliwosci sygnalu taktujacego na 62,5 kHz
	sbi(ADCSRA, ADPS0);
	sbi(ADCSRA, ADPS1);
	sbi(ADCSRA, ADPS2);
	// Uruchomienie przetwornika
	sbi(ADCSRA, ADEN);
	// Konfiguracja, ktora ustawia ADC0, czyli PA0 jako pin do pomiaru
	cbi(ADMUX, MUX0);
	cbi(ADMUX, MUX1);
	cbi(ADMUX, MUX2);
	cbi(ADMUX, MUX3);
	cbi(ADMUX, MUX4);
}

// Zwraca numer wcisnietego przycisku
int pressed_button () {
	int column_index = 7;
	int row_index = 4;
	int button_index = 1;
	while (column_index > 5) {
		cbi(PORTD, column_index);
		while (row_index < 6) {
			if (bit_is_clear(PIND, row_index)) {
				sbi(PORTD, column_index);
				return button_index;
			} else {
				button_index++;
			}
			row_index++;
		}
		row_index = 4;
		sbi(PORTD, column_index);
		column_index--;
	}
	return -1; // Jesli zaden przycisk nie jest wcisniety
}

// Realizacja pierwszej funkcji - info
// Wyswietla kominukat powitalny na 4 s
void info () {
	LCD_Clear();
	LCD_GoTo(0, 0);
	LCD_WriteText("Program PTM 2021");
	_delay_ms(4000);
}

// Tablica cyfr dla wyswietlacza 7-segmentowego
char digits[10] = {0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
		0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011};

// Wyswietla wybrana cyfre na wyswietlaczu 7-segmentowym
void SEG_ShowDigit (int digit_to_show) {
	PORTC = digits[digit_to_show];
}

// Wyswietla linie na wyswietlaczu 7-segmentowym
void SEG_ShowLine () {
	PORTC = 0b0000001;
}

// Sprawdza czy dana liczba jest liczba pierwsza
int is_first_number (int number) {
	if (number < 2) {
		return 0;
	}
	for (int i = 2; (i * i) <= number; i++) {
		if ((number % i) == 0) {
			return 0;
		}
	}
	return 1;
}

// Realizacja drugiej funkcji - liczby
void liczby () {
	int number = 0; // Na poczatku jest wyswietlana liczba 0
	char text[3]; // Zmienna przechowujaca wyswietlana liczbe
	do {
		// Wyswietlanie liczby na wyswietlaczu LCD
		LCD_Clear();
		sprintf(text, "%d", number);
		LCD_WriteText(text);
		_delay_ms(250);
		if (pressed_button() == 1) {	// Jesli wcisnieto przycisk 'U' to,
			if (number < 50) {	// jesli liczba jest mniejsza od 50 to,
				number++;	// jest zwiekszana
			}
		}
		if (pressed_button() == 2) {	// Jesli wcisnieto przycisk 'D' to,
			if (number > 0) {	// jesli liczba jest wieksza od 0 to,
				number--; // jest zmiejszana
			}
		}
		if (number <= 9) {	// Jesli liczba jest mniejsza od 10 to,
			SEG_ShowDigit(number);	// jest wyswietlana na wyswietlaczu 7-segmentowym
		} else {	// Jesli liczba jest wieksza lub rowna 10 to,
			SEG_ShowLine();	// linia jest wyswietlana na wyswietlaczu 7-segmentowym
		}
		if ((number % 2) == 0) { // Jesli liczba jest parzysta to,
			cbi(PORTD, PD1);	// gasnie dioda LED2
			sbi(PORTD, PD0);	// i zaswieca sie dioda LED1
		} else {				 // Jesli liczba jest nieparzysta to,
			cbi(PORTD, PD0);	// gasnie dioda LED1
			sbi(PORTD, PD1);	// i zaswieca sie dioda LED2
		}
		if (is_first_number(number)) { // Jesli liczba jest liczba pierwsza to,
			sbi(PORTD, PD2);	// zaswieca sie dioda LED3
		} else {
			cbi(PORTD, PD2);	// w przeciwnym przypadku gasnie
		}
	} while (pressed_button() != 4); // Jesli wcisnieto przycisk 'X', to funkcja konczy dzialanie
	// Zgaszenie wyswietlacza 7-segmentowego
	PORTC = 0b0000000;
	// Zgaszenie diod LED1, LED2 i LED3
	cbi(PORTD, PD0);
	cbi(PORTD, PD1);
	cbi(PORTD, PD2);
	return;
}

volatile int function_flag, s1; // Zmienne wykorzystywane w funkcji przerwan wewnetrznych

// Realizacja trzeciej funkcji - stoper
void stoper () {
	Timer_Stoper_Initialize(); // Konfiguruje licznik dla przerwan z czestotliwoscia 10 Hz
	int ok_counter = 0, m1 = 0, s3 = 0, s2 = 0; // Kolejno: licznik wcisniec przycisku 'O', minuty i coraz mniejsze czesci sekundy
	function_flag = 1; // Flaga wykorzstywana w funkcji przerwan wewnetrznych
	s1 = 0;
	char time[6]; // Zmienna przechowujaca wyswietlany czas
	do {
		// Wyswietlanie czasu na wyswietlaczu LCD
		LCD_Clear();
		sprintf(time, "%d:%d%d%d", m1, s3, s2, s1);
		LCD_WriteText(time);
		_delay_ms(250);
		if (pressed_button() == 3 && ok_counter == 0) { // Jesli wcisnieto przycisk 'O' po raz pierwszy to,
			m1 = 0; s3 = 0, s2 = 0, s1 = 0; // nastepuje wyzerowanie czasu i uruchomienie stopera
			// Wyswietlanie czasu na wyswietlaczu LCD
			LCD_Clear();
			sprintf(time, "%d:%d%d%d", m1, s3, s2, s1);
			LCD_WriteText(time);
			_delay_ms(250);
			ok_counter++;
			while (pressed_button() == 3) { // Czekanie na puszczenie wcisnietego przycisku
			}
			sei(); // Wlaczenie przerwan
		}
		if (pressed_button() == 3 && ok_counter == 1) { // Jesli wcisnieto przycisk 'O' po raz drugi, to nastepuje zatrzymanie czasu
			cli(); // Wylaczenie przerwan
			ok_counter = 0;
			while (pressed_button() == 3) { // Czekanie na puszczenie wcisnietego przycisku
			}
		}
		// Przeliczanie czasu
		if (s1 > 9) {
			s2++;
			s1 = 0;
		}
		if (s2 > 9) {
			s3++;
			s2 = 0;
		}
		if (s3 > 5) {
			m1++;
			s3 = 0;
		}
	} while (pressed_button() != 4); // Jesli wcisnieto przycisk 'X', to funkcja konczy dzialanie
	cbi(PORTD, PD0); // Zgaszenie diody LED1
	cli(); // Wylaczenie przerwan
	return;
}

// Realizacja czwartej funkcji - zegar
void zegar () {
	Timer_Zegar_Initialize(); // Konfiguruje licznik dla przerwan z czestotliwoscia 1 Hz
	int m1 = 0, m2 = 0, s2 = 0; // m1m2:s1s2
	function_flag = 2; // Flaga wykorzstywana w funkcji przerwan wewnetrznych
	s1 = 0;
	char time[6]; // Zmienna przechowujaca wyswietlany czas
	do {
		// Wyswietlanie sekund na wyswietlaczu 7-segmentowym
		SEG_ShowDigit(s1);
		// Wyswietlanie czasu na wyswietlaczu LCD
		LCD_Clear();
		sprintf(time, "%d%d:%d%d", m2, m1, s2, s1);
		LCD_WriteText(time);
		_delay_ms(250);
		// Przeliczanie czasu
		if (s1 > 9) {
			s2++;
			s1 = 0;
		}
		if (s2 > 5) {
			m1++;
			s2 = 0;
		}
		if (m1 > 9) {
			m2++;
			m1 = 0;
		}
		if (m2 > 5) { // Jesli przekroczono czas 59:59, to nastepuje wyzerowanie czasu
			s1 = 0;
			s2 = 0;
			m1 = 0;
			m2 = 0;
		}
		if (bit_is_set(PIND, PD2)) { // Jesli dioda LED3 jest zaswiecona to,
			_delay_ms(200);
			cbi(PORTD, PD2);	// nastepuje zgaszenie jej po czasie 200 ms
		}
		sei(); // Wlaczenie przerwan
	} while (pressed_button() != 4); // Jesli wcisnieto przycisk 'X', to funkcja konczy dzialanie
	cbi(PORTD, PD2); // Zgaszenie diody LED3
	PORTC = 0x00; // Zgaszenie wyswietlacza 7-segmentowego
	cli(); // Wylaczenie przerwan
	return;
}

// Pomiar napiecia
uint16_t ADC_Measure () {
	// Rozpoczenie pomiaru
	sbi(ADCSRA, ADSC);
	// Oczekiwanie na koniec pomiaru
	while (bit_is_set(ADCSRA, ADSC)) {
	}
	// Zwrocenie wyniku pomiaru
	return (uint16_t) ADC;
}

// Konwersja wyniku pomiaru napiecia do odpowiedniej postaci
uint16_t ADC_Convert (uint16_t measured_value) {
	// Przeliczenie wyniku pomiaru na odpowiednia postac
	double converted_value = ((double) measured_value * 5 * 100) / 1022;
	// Zwrocenie wyniku pomiaru w odpowiedniej postaci
	return (uint16_t) converted_value;
}

// Wyswietla zmierzone napiecie na wyswietlaczu LCD
void ADC_DisplayMeasured (uint16_t measured_value) {
	char  displayed_value[5]; // Zmienna przechowujaca wyswietlane napiecie
	sprintf(displayed_value, "%d", measured_value);
	LCD_WriteText(displayed_value);
}

// Wyswietla zmierzone napicie po konwersji do odpowiedniej postaci
void ADC_DisplayConverted (uint16_t converted_value) {
	char displayed_value[7], helper[4]; // Kolejno: zmienna przechowujaca wyswietlane napiecie, zmienna pomocnicza
	if (converted_value >= 100) {
		sprintf(helper, "%d", converted_value);
		displayed_value[0] = helper[0];
		displayed_value[1] = '.';
		displayed_value[2] = helper[1];
		displayed_value[3] = helper[2];
		displayed_value[4] = ' ';
		displayed_value[5] = 'V';
		displayed_value[6] = '\0';
	} else if (converted_value >= 10) {
		sprintf(displayed_value, "0.%d V", converted_value);
	} else {
		sprintf(displayed_value, "0.0%d V", converted_value);
	}
	LCD_WriteText(displayed_value);
}

// Realizacja piatej funkcji - miernik
void miernik () {
	uint16_t measured_value, converted_value;
	do {
		measured_value = ADC_Measure(); // Pomiar napiecia
		converted_value = ADC_Convert(measured_value); // Konwersja zmierzonego napiecia do odpowiedniej postaci
		// Wyswietlanie zmierzonego napiecia
		LCD_Clear();
		// Przed konwersja w pierwszym wierszu wyswietlacza LCD
		LCD_GoTo(0, 0);
		ADC_DisplayMeasured(measured_value);
		// Po konwersji w drugim wierszu wyswietlacza LCD
		LCD_GoTo(0, 1);
		ADC_DisplayConverted(converted_value);
		_delay_ms(250);
	} while (pressed_button() != 4); // Jesli wcisnieto przycisk 'X', to funkcja konczy dzialanie
	return;
}

int main () {
	// Konfiguracja portow przyciskow klawiatury
	configure_buttons_ports();
	// Konfiguracja portow wyswietlacza 7-segmentowego
	SEG_Initialize();
	// Konfiguracja portow diod led
	LED_Initialize();
	// Konfiguracja przetwornika dla pomiaru napiecia
	ADC_Initialize ();
	// Konfiguracja wyswietlacza LCD
	LCD_Initalize();
	LCD_Home();

	// Wyswietlenie komunikatu powitalnego
	info();
	// Wyswietlenie menu
	LCD_Clear();
	LCD_WriteText("Menu");
	_delay_ms(2000);

	int position = 0;

	// Obsluga menu
	while (1) {
		switch (position) {
		case 0: // Pierwsza pozycja/funkcja - info
			// Wyswietlanie nazwy opcji/funkcji
			LCD_Clear();
			LCD_WriteText("Info");
			_delay_ms(250);
			if (pressed_button() == 3) { 	// Jesli wcisnieto przycisk 'O' to,
				info();	// odpowiednia funkcja jest wywolywana
				LCD_Clear();
				LCD_WriteText("Info");
				_delay_ms(250);
			}
			break;
		case 1: // Druga pozycja/funkcja - liczby
			// Wyswietlanie nazwy opcji/funkcji
			LCD_Clear();
			LCD_WriteText("Liczby");
			_delay_ms(250);
			if (pressed_button() == 3) {	// Jesli wcisnieto przycisk 'O' to,
				liczby();	// odpowiednia funkcja jest wywolywana
				LCD_Clear();
				LCD_WriteText("Liczby");
				_delay_ms(250);
			}
			break;
		case 2: // Trzecia pozycja/funkcja - stoper
			// Wyswietlanie nazwy opcji/funkcji
			LCD_Clear();
			LCD_WriteText("Stoper");
			_delay_ms(250);
			if (pressed_button() == 3) {	// Jesli wcisnieto przycisk 'O' to,
				stoper();	// odpowiednia funkcja jest wywolywana
				LCD_Clear();
				LCD_WriteText("Stoper");
				_delay_ms(250);
			}
			break;
		case 3: // Czwarta pozycja/funkcja - zegar
			// Wyswietlanie nazwy opcji/funkcji
			LCD_Clear();
			LCD_WriteText("Zegar");
			_delay_ms(250);
			if (pressed_button() == 3) {	// Jesli wcisnieto przycisk 'O' to,
				zegar();	// odpowiednia funkcja jest wywolywana
				LCD_Clear();
				LCD_WriteText("Zegar");
				_delay_ms(250);
			}
			break;
		case 4: // Piata pozycja/funkcja - miernik
			// Wyswietlanie nazwy opcji/funkcji
			LCD_Clear();
			LCD_WriteText("Miernik");
			_delay_ms(250);
			if (pressed_button() == 3) {	// Jesli wcisnieto przycisk 'O' to,
				miernik();	// odpowiednia funkcja jest wywolywana
				LCD_Clear();
				LCD_WriteText("Miernik");
				_delay_ms(250);
			}
			break;
		}
		if (pressed_button() == 1) {	// Jesli przewijamy menu w gore, czyli wcisnieto przycisk 'U'
			if (position == 0) {	// Jesli jestesmy na poczatku to,
				position = 4;	// idziemy na koniec
			} else {
				position--;	// w przeciwnym przypadku idziemy w gore
			}
			while (pressed_button() == 1) {	// Czekanie na puszczenie wcisnietego przycisku
			}
		}
		if (pressed_button() == 2) {	// Jesli przewijamy menu w dol, czyli wcisnieto przycisk 'D'
			if (position == 4) {	// Jesli jestesmy na koncu to,
				position = 0;	// idziemy na poczatek
			} else {
				position++;	// w przeciwnym przypadku idziemy w dol
			}
			while (pressed_button() == 2) {	// Czekanie na puszczenie wcisnietego przycisku
			}
		}
	}
	return 0;
}

// Realizacja funkcji przerwan wewnetrznych
ISR (TIMER1_COMPA_vect) {
	// Zwieksza licznik sekund lub milisekund w zaleznosci od tego, w ktorej funkcji
	// (stoper lub zegar) zostanie wywolana
	s1++;
	if (function_flag == 1 && s1 > 9) { // Jesli zostala wywolana funkcja stoper to,
		tbi(PORTD, PD0);	// co sekunde zmienia stan diody LED1 na przeciwny
	}
	if (function_flag == 2) {	// Jesli zostala wywolana funkcja zegar to,
		sbi(PORTD, PD2);	// co sekunde zaswieca diode LED3
	}
}
