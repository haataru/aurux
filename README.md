# AURUX

32-битное монолитное ядро для архитектуры x86.

## Требования для сборки

Проект написан на стандартных технологиях, поэтому технически его можно собрать под любую ОС (Linux/Windows WSL), установив кросс-компилятор `i686-elf-gcc` и утилиты `mtools`, `dosfstools`, `qemu`. 
Однако, в данный момент сборка оптимизирована и протестирована под **macOS** (Apple Silicon / Intel).

Для macOS необходимо установить следующие пакеты через Homebrew:
```bash
brew install i686-elf-gcc i686-elf-binutils
brew install mtools
brew install dosfstools
brew install qemu
```

## Сборка и запуск

Для удобства сборки и запуска в macOS используется специальный скрипт `build_macos.sh`. 

```bash
# Полная сборка с нуля:
./build_macos.sh clean
./build_macos.sh all

# Запуск в эмуляторе QEMU:
./build_macos.sh run
```

## Утилиты (User Space)

Ядро поддерживает полноценное пользовательское пространство. Доступные утилиты командной строки:
- `cat`, `echo`, `ls`, `mkdir`, `rm` — базовые файловые операции.
- `ps` — вывод списка запущенных процессов.
- `kill <pid>` — завершение процесса по его ID.
- `sleep <ms>` — задержка (сон) текущего процесса.

Поддерживается конвейеризация (например, `ls | cat`) и фоновое исполнение (например, `sleep 5000 &`).

## Документация

- [Архитектура (docs/kernel.md)](docs/kernel.md)
- [Карта памяти (docs/memory_map.md)](docs/memory_map.md)
- [Системные вызовы (docs/syscalls.md)](docs/syscalls.md)
