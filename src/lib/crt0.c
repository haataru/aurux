extern int main(int argc, char** argv);
extern void exit(int status);

void _start(int argc, char** argv);

void _start(int argc, char** argv) {
    int ret = main(argc, argv);
    exit(ret);
}
