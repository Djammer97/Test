#ifndef UART_C

#define BAUD 9600 //битрейт
#define UBRR_value 51 // скорость передачи: 8000000/(9600*16)-1=51.08
#define ADDRESS_DEVICE 100 //адрес МК
#define RX_SIZE 5	//размер массива принятых данных
#define TX_SIZE 3	//размер массива ответа

unsigned char Rx_Modbus [RX_SIZE]= {}; //приняе данные [адрес, вольтаж, вольтаж, контрольная сумма, контрольная сумма]
unsigned char Tx_Modbus[TX_SIZE]={}; // ответ {0 0 0} - провал, {1 1 1} - успех
	
unsigned char TempModbus; //считывание данных с входа
int counter = 0; //счетчик записанных данных
short flag_read = 0; //признак начала ввода

//Прерываение по получению данных на RX 
ISR(USART_RXC_vect) 
{
	TempModbus=UDR;
	if (Rx_Modbus[0]==0)
	{
		if (flag_read==0)
		{
			flag_read = 1;
			if (TempModbus==ADDRESS_DEVICE) //проверка соответствия адреса
			{
				Rx_Modbus[counter]=TempModbus; //заполнение байта адреса
			}
		}
	}
	else
	{
		Rx_Modbus[counter]=TempModbus;//заполнение оставшегося массива данных
	}
	counter++;
}

//настройки UART
void init_UART()
{
	UBRRL = UBRR_value;
	UCSRB |=((1<<TXEN)|(1<< RXEN)|(1<<RXCIE));
	UCSRC |=((1<<UCSZ0)|(1<<UCSZ1)|(1<<URSEL));
}

//Отправка одного элемента массива
void send_UART(unsigned char value)
{
	while (~UCSRA&(1<<UDRE))
	{
	}
	UDR = value;
}

//Отправка массива
void send(uint8_t* Tx)
{
	for (int i=0 ; i <TX_SIZE ; i++)
	{
		send_UART(Tx[i]);
		Tx[i]=0;
	}
	flag_read = 0;
}

//Подсчет контрольной суммы crc16
int crc_chk(uint8_t* data, unsigned char length)
{
	register int j;
	register unsigned int reg_crc = 0xFFFF;
	while (length--)
	{
		reg_crc ^= *data++;
		for (j = 0; j < 8; j++)
		{
			if (reg_crc & 0x01)
			{
				reg_crc = (reg_crc >> 1) ^ 0xA001;
			}
			else
			{
				reg_crc = reg_crc >> 1;
			}
		}
	}
	return reg_crc;
}
#endif

