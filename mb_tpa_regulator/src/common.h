#ifndef COMMON_H
#define COMMON_H

#define SET(x, y) ((x) |= (1 << (y)))
#define CHECK(x,y) (((x) & (1 << (y))) ? 1 : 0)
#define CLEAR(x,y) ((x) &= ~(1 << (y)))

#define PTRC_BUFFER_SIZE 4096
#define TMG_BUFFER_SIZE 4096

#endif // COMMON_H
