#ifndef COMMON_H
#define COMMON_H

#define SET(x, y) ((x) |= (1 << (y)))
#define CHECK(x,y) (((x) & (1 << (y))) ? 1 : 0)
#define CLEAR(x,y) ((x) &= ~(1 << (y)))

#endif // COMMON_H
