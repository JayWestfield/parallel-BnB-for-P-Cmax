#include <cstdint>

class FastRandom {
private:
    uint32_t seed;

public:
    FastRandom(uint32_t s) : seed(s) {}

    uint32_t next() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return seed;
    }

    // Funktion um den Bereich von 0 bis max ohne modulo zu begrenzen
    uint32_t nextInRange(uint32_t max) {
        // Der Bereich von 0 bis max wird skaliert, indem wir die höchstmögliche Zufallszahl durch max + 1 teilen
        return next() % (max + 1);
    }
};
