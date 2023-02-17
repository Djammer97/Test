#define F_CPU 8000000 
#define AVERAGE 50 //количество итераций для подсчета среднего значения
#define K_OS 16 //Коэффициент делителя напряжения на входе АЦП

#include <avr/io.h>
#include <util/delay.h>
#include <string.h> 
#include <stdio.h>
#include <avr/interrupt.h>
#include "uart.h"

short flag_timer_LED1 = 0; //счетчик для индикации 1 светодиода
short flag_timer_LED2 = 0; //счетчик для индикации 2 светодиода
short work_LED1 = 0; //признак работы 2 светодиода
short work = 0; //признак начала работы(появляется после успешного приема данных)

long eps[3] = {}; //массив ошибок для ПД-регулятора
short mc_work = 25; //количество тактов Таймера2 (T=0.004 c), необходимое для 0.1 с
short mc_finish = 125; //количество тактов Таймера2 (T=0.004 c), необходимое для 0.5 с

//Срабатывает каждые 0.004 с
ISR(TIMER2_COMP_vect)
{
	//Работа негаснущего светодиода
	flag_timer_LED2++;
	if (flag_timer_LED2 % mc_work==0)
	{
		PORTD |= (1<<6);
	}
	if (flag_timer_LED2 % (2*mc_work)==0)
	{
		PORTD &= ~(1<<6);
		flag_timer_LED2=0;
	}
	
	//Работа гаснущего светодиода
	if (work==1 && work_LED1==0)
	{
		PORTD |= (1<<7);
		flag_timer_LED1++;
		if (flag_timer_LED1 % mc_finish ==0)
		{
			work_LED1=1;
			PORTD &= ~(1<<7);
			flag_timer_LED1=0;
		}
	}
}

//Подсчет среднего значения за 50 тактов на входе АЦП с дальнейшем воздействием на частоту ШИМ
void average(long voltage)
{
	long average_voltage = 0; //средний вольтаж за 30 тактов
	short average_counter = 0; //счетчик тактов
	long double summ = 0; //сумма показаний за n тактов
	int delta = 0; //величина, на которую нужно изменить скважность

	ADCSRA |= (1<<ADSC);
	while (average_counter<AVERAGE)
	{
		if (ADCSRA & (1<<ADIF))
		{
			summ+=(((double)ADC)*5*1000/1024*K_OS);
			average_counter++;
			ADCSRA |= (1<<ADIF);
		}
	}
	average_voltage = summ/average_counter;
	eps[1] = voltage - average_voltage; //величина ошибки (для П-регулятора)
	eps[2] = eps[1] - eps[0]; //разница между ошибка за два периода измерений (для Д-регулятора)
	eps[0] = eps[1];
	delta = eps[0]*0.0045+eps[2]*0.018; //ПД-регуляция
	OCR1A += delta;
	if (delta == 0)
	{
		mc_work=mc_finish; //Если изменения прошли, то увеличиваем количество тактов второго светодиода
	}
}

//Т.к. при малых токах на входе ОУ, возникают помехи на выходе, разбиваем весь диапазон на интервалы
//Каждый интервал содержит коэффициент скалирования (который потом учитывается на выходе цепи)
//Таким образом на выходе ОУ формируется напряжние не меньше 7В (на входе не менее 0.4В)
long scale(unsigned int setting)
{
	double K_scale = 0;
	if (setting <=7)
		{
			K_scale = 7059.82;
		} else
		{
			if (setting <= 69)
			{
				K_scale = 901.2;
			}
			else
			{
				if (setting <= 639)
				{
					K_scale = 101;
				}
				else
				{
					if (setting <=5899)
					{
						K_scale = 11;
					}
					else
					{
						if (setting <= 6999)
						{
							K_scale = 2;
						}
						else
						{
							K_scale = 1;
						}
					}
				}
			}
		}
	return setting*K_scale;
}

int main(void)
{
	int crc_RX = 0; //контрольная сумма принятого пакета
	int crc_TX = 0; //контрольная сумма пересчитанная
	unsigned int setting = 0; //переданное напряжение уставки
	long voltage = 0; //напряжение , которое нужно получить на выходе ОУ
	short PWM_on = 0; //признак включения ШИМ
	int counter_switcher = 0; //задержка переключения цепей на выходе 
	int counter_ADW = 0; //задержка между подсчетом среднего значения

	//Настройка портов
	DDRD |= (1<<7)|(1<<6);
	PORTD &= ~((1<<7)|(1<<6));
	DDRB = 0b11111111;
	PORTB = 0b00000000;
	DDRC &= ~(1<<0);
	
	init_UART();
	
	//Настройка Таймера2 (Работа в режиме CTC, делитель частоты 256, величина совпадения 124 такта)
	TCCR2 &= ~(1<<WGM20);
	TCCR2 |= (1<<WGM21);
	TCCR2 &= ~(1<<CS20);
	TCCR2 |= (1<<CS22)|(1<<CS21);
	TCNT2 = 0;
	OCR2 = 124;
	TIMSK |= (1<<OCIE2);
	TIMSK &= ~(1<<TOIE2);	
	
	//Настройка АЦП (Работа в Free-running режиме, предделитель частоты 64 (АЦП работает с чатотой 125кгЦ),
	//				работа от внешнего 5 вольтного истоничника питания, правостороннее выравнивание, ввод ADC0)  
	ADCSRA |= (1<<ADEN);
	ADCSRA |= (1<<ADFR);
	ADCSRA &= ~(1<<ADPS0);
	ADCSRA |= (1<<ADPS1)|(1<<ADPS2);
	
	ADMUX |= (1<<REFS0);
	ADMUX &= ~(1<<REFS1);
	ADMUX &= ~(1<<ADLAR);
	ADMUX &= ~((1<<MUX0)|(1<<MUX1)|(1<<MUX2)|(1<<MUX3));
	
	sei();
	
	while (1)
	{
		//В случае, если счетчик равен размеру RX, все данные переданы.
		//Считывается контрольная сумма из массива и сравнивается с перерасчетом
		//В случае совпадения адреса и контрольных сумм формируется массив из трех 1, которые посылаются в ответ
		//Формируется признак начала работы
		if(counter==RX_SIZE)
		{
			crc_RX = crc_chk(Rx_Modbus, 3);
			crc_TX = Rx_Modbus[3] + Rx_Modbus[4] * 256;
			if ((crc_RX==crc_TX) && (Rx_Modbus[0]==ADDRESS_DEVICE))
			{
				for (int i=0; i<TX_SIZE; i++)
				{
					Tx_Modbus[i] = 1;
				}
				work = 1;
				work_LED1 = 0;
				setting = Rx_Modbus[1] + Rx_Modbus[2] * 256;
				mc_work=mc_finish/5;
			}
			send(Tx_Modbus);
			for (int i=0; i <RX_SIZE; i++)
			{
				Rx_Modbus[i] = 0;
			}
			crc_RX = 0;
			crc_TX = 0;
			counter = 0;
			eps[0]=0;
			eps[1]=0;
			eps[2]=0;
		}	
		
		if (work==1)
		{
			//Включение ШИМ, работающего в режиме с коррекцией частоты на первом таймере, без предделителя
			//Частота ШИМ 8000000/2/2000/1=2000 Гц
			if (PWM_on==0)
			{
				TCCR1A &= ~(1<<COM1A0);
				TCCR1A |= (1<<COM1A1);
				TCCR1A &= ~(1<<WGM10);
				TCCR1A &= ~(1<<WGM11);
				TCCR1B &= ~(1<<WGM12);
				TCCR1B |= (1<<WGM13);
				
				TCCR1B |= (1<<CS10);
				TCCR1B &= ~(1<<CS11);
				TCCR1B &= ~(1<<CS12);
				
				OCR1A = 150;
				ICR1 = 2000;
				PWM_on = 1;
			}
			
			//Чтобы избежать падения напряжения до 0 при изменении уставки во время выполнения программы, при переходе из одного интервала в другой
			//сначала открывается вторая линия, а только после этого закрывается первая
			counter_switcher++;
			if (setting <=7)
			{
				PORTB |= (1<<7);
				if (counter_switcher % 100 == 0)
				{
					PORTB &= ~((1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<6));
					counter_switcher = 0;
				}
			} else
			{
				if (setting <= 69)
				{
					PORTB |= (1<<6);
				if (counter_switcher % 100 == 0)
				{					
					PORTB &= ~((1<<2)|(1<<3)|(1<<4)|(1<<5)|(1<<7));
					counter_switcher = 0;
				}
				}
				else
				{
					if (setting <= 639)
					{
						PORTB |= (1<<5);
						if (counter_switcher % 100 == 0)
						{
						PORTB &= ~((1<<2)|(1<<3)|(1<<4)|(1<<6)|(1<<7));
						counter_switcher = 0;
						}
					}
					else
					{
						if (setting <=5899)
						{
							PORTB |= (1<<4);
							if (counter_switcher % 100 == 0)
							{	
							PORTB &= ~((1<<2)|(1<<3)|(1<<5)|(1<<6)|(1<<7));
							counter_switcher = 0;
							}
						}
						else
						{
							if (setting <= 6999)
							{
								PORTB |= (1<<3);
								if (counter_switcher % 100 == 0)
								{
									PORTB &= ~((1<<2)|(1<<4)|(1<<5)|(1<<6)|(1<<7));
									counter_switcher = 0;
								}
							}
							else
							{
								PORTB |= (1<<2);
								if (counter_switcher % 100 == 0)
								{
								PORTB &= ~((1<<3)|(1<<4)|(1<<5)|(1<<6)|(1<<7));
								counter_switcher = 0;
								}
							}
						}
					}
				}
			}
			voltage=scale(setting);
			
			//Чтобы не захватывать переходной процесс поставлена небольшая задержка между замерами
			if (counter_ADW==40)
			{
				average(voltage);
				counter_ADW = 0;
			}
			counter_ADW++;
		}
	}
}

