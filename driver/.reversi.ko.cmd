cmd_/usr/src/project3/driver/reversi.ko := ld -r -m elf_x86_64  -z max-page-size=0x200000  --build-id  -T ./scripts/module-common.lds -o /usr/src/project3/driver/reversi.ko /usr/src/project3/driver/reversi.o /usr/src/project3/driver/reversi.mod.o;  true