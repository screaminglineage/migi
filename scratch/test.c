#include "migi.h"

int main() {
    Temp tmp = arena_temp();
    arena_temp_release(tmp);
    return 0;
}
