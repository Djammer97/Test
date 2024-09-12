# Задание для собеседования на позицию Embedded-разработчика

## Проект представляет из себя прошивку микроконтроллера семейства ATMega-8.

***

### Функционал

Микроконтроллер принимает уставку напряжения по протоколу **UART** в формате uint16.
После чего формируется **ШИМ**, обратная связь по **АЦП** от которого находится в пределах уставки ±10%.
Переходной процесс происходит **без перерегулирования**.

### Дополнительный функционал

- Прием посылки сопровождается световой индикацией от светодиода LED1
- Нахождение ШИМ вне диапазона уставки (во время переходного процесса) сопровождается частой световой индикацией светодиода LED2
- Прием посылки сопровождается проверкой андресности и целостности
- Для отправки посылки написана программа на C++

***

### Используемые технологии

1) C++ - программа для отправка UART посылки (С++/Send_Modbus)
2) C - прошивка микроконтроллера (Test)
3) Proteus - моделирование и отладка (Test)

***

Документация к проекту с пояснениями представлена в файле - **"Пояснительная записка.docx"**