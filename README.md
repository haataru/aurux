# TurbanOS

32-bit x86 kernel.

## Требования

```
gcc-i686-linux-gnu
binutils-i686-linux-gnu
qemu-system-x86
```

## Сборка

```
make
```

## Запуск

```
qemu-system-i386 -kernel kernel.bin
```

## Устройство

Ядро инициализирует VGA, память, файловую систему, клавиатуру, RTC. Запускает shell.

### Файловая система

In-memory, 64 файла/директории, до 4096 байт на файл, вложенность до 4 уровней.

### Драйверы

VGA (текстовый режим 80x25), PS/2 клавиатура, RTC.

### Shell

Команды: help, ls, cd, pwd, cat, touch, mkdir, rm, echo, time, date, clear, reboot, mem.

## Структура

```
src/
├── kernel/      # ядро, прерывания
├── drivers/    # vga, keyboard, rtc
├── memory/     # kmalloc, kfree
├── fs/         # файловая система
├── shell/      # командная оболочка
└── lib/        # string, printf
```
