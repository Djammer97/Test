#define F_CPU 8000000 
#define AVERAGE 50 //���������� �������� ��� �������� �������� ��������
#define K_OS 16 //����������� �������� ���������� �� ����� ���

#include <avr/io.h>
#include <util/delay.h>
#include <string.h> 
#include <stdio.h>
#include <avr/interrupt.h>
#include "uart.h"

short flag_timer_LED1 = 0; //������� ��� ��������� 1 ����������
short flag_timer_LED2 = 0; //������� ��� ��������� 2 ����������
short work_LED1 = 0; //������� ������ 2 ����������
short work = 0; //������� ������ ������(���������� ����� ��������� ������ ������)

long eps[3] = {}; //������ ������ ��� ��-����������
short mc_work = 25; //���������� ������ �������2 (T=0.004 c), ����������� ��� 0.1 �
short mc_finish = 125; //���������� ������ �������2 (T=0.004 c), ����������� ��� 0.5 �

//����������� ������ 0.004 �
ISR(TIMER2_COMP_vect)
{
	//������ ����������� ����������
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
	
	//������ ��������� ����������
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

//������� �������� �������� �� 50 ������ �� ����� ��� � ���������� ������������ �� ������� ���
void average(long voltage)
{
	long average_voltage = 0; //������� ������� �� 30 ������
	short average_counter = 0; //������� ������
	long double summ = 0; //����� ��������� �� n ������
	int delta = 0; //��������, �� ������� ����� �������� ����������

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
	eps[1] = voltage - average_voltage; //�������� ������ (��� �-����������)
	eps[2] = eps[1] - eps[0]; //������� ����� ������ �� ��� ������� ��������� (��� �-����������)
	eps[0] = eps[1];
	delta = eps[0]*0.0045+eps[2]*0.018; //��-���������
	OCR1A += delta;
	if (delta == 0)
	{
		mc_work=mc_finish; //���� ��������� ������, �� ����������� ���������� ������ ������� ����������
	}
}

//�.�. ��� ����� ����� �� ����� ��, ��������� ������ �� ������, ��������� ���� �������� �� ���������
//������ �������� �������� ����������� ������������ (������� ����� ����������� �� ������ ����)
//����� ������� �� ������ �� ����������� ��������� �� ������ 7� (�� ����� �� ����� 0.4�)
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
	int crc_RX = 0; //����������� ����� ��������� ������
	int crc_TX = 0; //����������� ����� �������������
	unsigned int setting = 0; //���������� ���������� �������
	long voltage = 0; //���������� , ������� ����� �������� �� ������ ��
	short PWM_on = 0; //������� ��������� ���
	int counter_switcher = 0; //�������� ������������ ����� �� ������ 
	int counter_ADW = 0; //�������� ����� ��������� �������� ��������

	//��������� ������
	DDRD |= (1<<7)|(1<<6);
	PORTD &= ~((1<<7)|(1<<6));
	DDRB = 0b11111111;
	PORTB = 0b00000000;
	DDRC &= ~(1<<0);
	
	init_UART();
	
	//��������� �������2 (������ � ������ CTC, �������� ������� 256, �������� ���������� 124 �����)
	TCCR2 &= ~(1<<WGM20);
	TCCR2 |= (1<<WGM21);
	TCCR2 &= ~(1<<CS20);
	TCCR2 |= (1<<CS22)|(1<<CS21);
	TCNT2 = 0;
	OCR2 = 124;
	TIMSK |= (1<<OCIE2);
	TIMSK &= ~(1<<TOIE2);	
	
	//��������� ��� (������ � Free-running ������, ������������ ������� 64 (��� �������� � ������� 125���),
	//				������ �� �������� 5 ��������� ����������� �������, �������������� ������������, ���� ADC0)  
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
		//� ������, ���� ������� ����� ������� RX, ��� ������ ��������.
		//����������� ����������� ����� �� ������� � ������������ � ������������
		//� ������ ���������� ������ � ����������� ���� ����������� ������ �� ���� 1, ������� ���������� � �����
		//����������� ������� ������ ������
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
			//��������� ���, ����������� � ������ � ���������� ������� �� ������ �������, ��� ������������
			//������� ��� 8000000/2/2000/1=2000 ��
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
			
			//����� �������� ������� ���������� �� 0 ��� ��������� ������� �� ����� ���������� ���������, ��� �������� �� ������ ��������� � ������
			//������� ����������� ������ �����, � ������ ����� ����� ����������� ������
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
			
			//����� �� ����������� ���������� ������� ���������� ��������� �������� ����� ��������
			if (counter_ADW==40)
			{
				average(voltage);
				counter_ADW = 0;
			}
			counter_ADW++;
		}
	}
}

