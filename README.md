# AURUX

32-битное монолитное ядро для архитектуры x86.

## Требования для сборки

- `i686-linux-gnu-gcc`
- `i686-linux-gnu-as`
- `i686-linux-gnu-ld`
- `mtools`
- `dosfstools`
- `qemu-system-i386`

## Сборка и запуск

```bash
make all
make run
```

Очистка проекта от артефактов сборки:
```bash
make clean
```

## Документация

- [Архитектура (docs/kernel.md)](docs/kernel.md)
- [Карта памяти (docs/memory_map.md)](docs/memory_map.md)
- [Системные вызовы (docs/syscalls.md)](docs/syscalls.md)
