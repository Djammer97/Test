#include <windows.h>
#include <iostream>
using namespace std;

uint8_t Rx[3] = {};
uint8_t Tx[5] = {};
bool flag=true;
int address = 0;
int voltage = 0;
int crc = 0;

HANDLE hSerial;
DWORD dwSize;
DWORD dwBytesWritten;
BOOL iRet;

void show(uint8_t* data, unsigned char length)
{
    for (int i = 0; i < length; i++)
    {
        cout << (int) data[i] << " ";
    }
    cout << endl;
}

void result(uint8_t* data, unsigned char length)
{
    int counter = 0;
    for (int i = 0; i < length; i++)
    {
        if (data[i] == 1)
        {
            counter++;
        }
    }
    if (counter > 1)
    {
        cout << "Transfer complete" << endl;
        flag = false;
    }
    else
    {
        cout << "Transfer is not complete\n" << endl;
    }
}

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

void WriteCOM()
{
    cout << "input address (1 - 247)" << endl;
    cout << "address=";
    cin >> address;
    cout << endl;
    Tx[0] = address;

    cout << "input voltage (1 - 65535) mV" << endl;
    cout << "voltage=";
    cin >> voltage;
    cout << endl;

    Tx[1] = voltage % 256;
    Tx[2] = (voltage - Tx[1]) / 256;

    crc = crc_chk(Tx, 3);
    Tx[3] = crc % 256;
    Tx[4] = (crc - Tx[3]) / 256;

    show(Tx, 5);

    dwSize = sizeof(Tx);
    
    iRet = WriteFile(hSerial, Tx, dwSize, &dwBytesWritten, NULL);
    cout << dwSize << " Bytes in string. " << dwBytesWritten << " Bytes sended. " << endl;
}

void ReadCOM()
{
    DWORD iSize;
    uint16_t sReceivedChar;
    int counter = 0;
    while (true)
    {
        ReadFile(hSerial, &sReceivedChar, 1, &iSize, 0);  
        if (iSize > 0)
        {
            
            Rx[counter] = sReceivedChar;
            counter++;
            if (counter == 3) {
                break;
            }
        }
    }
    show(Rx, 3);
    
    result(Rx, 3);
}

int main() {
    LPCTSTR sPortName = L"COM1";
    hSerial = ::CreateFile(sPortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            cout << "serial port does not exist.\n";
        }
        cout << "some other error occurred.\n";
    }
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams))
    {
        cout << "getting state error\n";
    }
    dcbSerialParams.BaudRate = CBR_9600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams))
    {
        cout << "error setting serial port state\n";
    }

    while (flag)
    {
        WriteCOM();
        ReadCOM();
    }
    return 0;
}
