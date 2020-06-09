/* * * * * * * * * * * * * * * *
 *                             *
 *      Helpful Typedefs       *
 *                             *
 * * * * * * * * * * * * * * * */

#include "stdint.h"

#define nullptr 0

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

/* * * * * * * * * * * * * * * *
 *                             *
 *        System Calls         *
 *                             *
 * * * * * * * * * * * * * * * */

static inline void* mmap(void* addr, u64 len, u64 prot, u64 flags) {
    void* ret;
    
    #define PROT_READ 0x1		
    #define PROT_WRITE 0x2		
    #define PROT_EXEC 0x4		
    #define PROT_NONE 0x0		
    #define PROT_GROWSDOWN 0x01000000	
    #define PROT_GROWSUP 0x02000000	

    #define MAP_SHARED	0x01
    #define MAP_PRIVATE	0x02
    #define MAP_ANONYMOUS 0x20
    asm volatile ("mov $9, %%rax\n\t"
        "mov %1, %%rdi\n\t"
        "mov %2, %%rsi\n\t"
        "mov %3, %%rdx\n\t"
        "mov %4, %%r10\n\t"
        "mov $-1, %%r8\n\t"
        "mov $0, %%r9\n\t"
        "syscall\n\t"
        "mov %%rax, %0"
        : "=r" (ret)
        : "r" (addr), "r" (len), "r" (prot), "r" (flags)
        : "rdi", "rsi", "rdx", "r10", "r9", "r8", "rax"
    );

    return ret;
}

static inline void munmap(void* addr, u64 len) {
    asm volatile ("mov $11, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "mov %1, %%rsi\n\t"
        "syscall\n\t"
        : 
        : "r" (addr), "r" (len)
        : "rdi", "rsi", "rax"
    );
}

static inline void exit(u64 ret) {
    asm volatile ("mov $60, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "syscall\n\t"
        :
        : "r" (ret)
        : "rax", "rdi"
    );
}

static inline void read(u64 fd, char* buf, u64 len) {
    asm volatile ("mov $0, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "mov %1, %%rsi\n\t"
        "mov %2, %%rdx\n\t"
        "syscall\n\t"
        :
        : "r" (fd), "r" (buf), "r" (len)
        : "rax", "rdi", "rsi", "rdx"
    );   
}

static inline void write(u64 fd, const char* text, u64 len) {
    asm volatile ("mov $1, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "mov %1, %%rsi\n\t"
        "mov %2, %%rdx\n\t"
        "syscall\n\t"
        :
        : "r" (fd), "r" (text), "r" (len)
        : "rax", "rdi", "rsi", "rdx"
    );   
}

struct { i64 tv_sec; i64 tv_nsec } spec;

static inline u64 now() {
    asm volatile ("mov $228, %%rax\n\t"
        "mov $1, %%rdi\n\t"
        "mov %0, %%rsi\n\t"
        "syscall\n\t"
        :
        : "r" (&spec)
        : "rax", "rdi", "rsi", "rdx"
    );   
    return spec.tv_nsec + spec.tv_sec * 1000000000ul;
}

/* * * * * * * * * * * * * * * *
 *                             *
 *       Error Handling        *
 *                             *
 * * * * * * * * * * * * * * * */

void _exit(u64 code) {
    exit(code);
}

void _panic(const char* msg) {
    const char* ptr = msg;
    while (*ptr) ptr ++;
    write(0, msg, ptr - msg);
    _exit(1);
}

/* * * * * * * * * * * * * * * *
 *                             *
 *  General-Purpose Allocator  *
 *                             *
 * * * * * * * * * * * * * * * */

const u64 ARENA_SIZE = 0x10000;

typedef struct node_t {
    struct node_t* prev;
    struct node_t* next;
} node;

#define NONE 0              // unused
#define TYPE_128 1          // 16-byte arena
#define TYPE_256 2          // 32-byte arena
#define TYPE_512 3          // 64-byte arena
#define TYPE_1024 4         // 128-byte arena
#define TYPE_2048 5         // 256-byte arena
#define TYPE_4096 6         // 512-byte arena
#define TYPE_8192 7         // 1024-byte arena
#define TYPE_16384 8        // 2048-byte arena
#define TYPE_32768 9        // 4096-byte arena
#define TYPE_65536 10       // 8192-byte arena
#define TYPE_LARGE 11       // > page size arena
#define NUM_TYPES 12

typedef struct arena_t {
    struct arena_t* prev;
    struct arena_t* next;
    node* nextpos;
    node* lastpos;
    void* end;
    u64 count;
    u64 type;
    u8 padding[8];
} arena;

static arena* new_arena(u64 type, arena* prev, arena* next) {
    // allocate region
    u8* ptr = mmap(nullptr,
                   ARENA_SIZE * 2,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS);

    // align to arena size within region
    arena* a = (arena*)((u64)(ptr + ARENA_SIZE) & ~(ARENA_SIZE - 1));

    // trim region to arena size
    if ((u8*)a - ptr) 
        munmap(ptr, (u8*)a - ptr);
    if ((ptr + ARENA_SIZE) - (u8*)a) 
        munmap((u8*)a + ARENA_SIZE, (ptr + ARENA_SIZE) - (u8*)a);

    u64 align = 8 << a->type; // compute element size
    if (align < sizeof(arena)) align = sizeof(arena);
    
    // initialize arena
    a->prev = prev;
    a->next = next;
    if (a->prev) a->prev->next = a;
    if (a->next) a->next->prev = a;
    a->lastpos = a->nextpos = (node*)((u8*)a + align);
    a->nextpos->prev = a->nextpos->next = nullptr;
    a->end = (u8*)a + ARENA_SIZE;
    a->count = 0;
    a->type = type;
    return a;
}

#define defarena(N) \
static arena* arena##N = nullptr; \
static void* alloc##N() { \
    if (!arena##N) arena##N = new_arena(TYPE_##N, nullptr, nullptr); \
    arena##N->count ++; \
    /* if space is open before end of region... */\
    if (arena##N->nextpos->next) { \
        /* detach node from linked list */\
        node* n = arena##N->nextpos; \
        n->next->prev = n->prev; \
        /* target next node in linked list */\
        arena##N->nextpos = n->next; \
        return n; \
    } \
    else { \
        node* n = arena##N->nextpos; \
        /* bump nextpos by N / 8 bytes */\
        arena##N->lastpos = arena##N->nextpos = (node*)((u8*)arena##N->nextpos + (N / 8)); \
        /* create new arena if no more room */\
        if (arena##N->nextpos >= (node*)arena##N->end) { \
            arena##N->nextpos = nullptr; \
            arena##N = new_arena(TYPE_##N, arena##N, nullptr); \
        } \
        else { \
            arena##N->nextpos->prev = n->prev; \
            arena##N->nextpos->next = nullptr; \
        } \
        return n; \
    } \
} \
static void free##N(arena* a, void* p) { \
    node* n = p; \
    if (a->count < arena##N->count) arena##N = a; \
    /* if we're deleting before the end */\
    if (a->lastpos == (node*)((u8*)p + N)) { \
        /* change lastpos */\
        if (a->lastpos >= (node*)a->end) { \
            n->next = a->nextpos; \
            n->prev = nullptr; \
            a->nextpos = n; \
        } \
        else { \
            n->prev = a->lastpos->prev; \
            if (n->prev) n->prev->next = n; \
            n->next = nullptr; \
        } \
        a->lastpos = n; \
    } \
    else { \
        /* change nextpos */\
        n->prev = nullptr; \
        n->next = a->nextpos; \
        if (n->next) n->next->prev = n->prev; \
        a->nextpos = n; \
    } \
}

defarena(128);
defarena(256);
defarena(512);
defarena(1024);
defarena(2048);
defarena(4096);
defarena(8192);
defarena(16384);
defarena(32768);
defarena(65536);

static void* alloclarge(u64 size) {
    size = size + sizeof(arena);
    size = (size + ARENA_SIZE - 1) & ~(ARENA_SIZE - 1);
    u8* p = mmap(nullptr, size + ARENA_SIZE, 
                    PROT_READ | PROT_WRITE, 
                    MAP_PRIVATE | MAP_ANONYMOUS);
    // align to arena size within region
    arena* a = (arena*)((u64)(p + ARENA_SIZE) & ~(ARENA_SIZE - 1));

    // trim region to arena size
    if ((u8*)a - p) 
        munmap(p, (u8*)a - p);
    if ((p + ARENA_SIZE) - (u8*)a) 
        munmap((u8*)a + ARENA_SIZE, (p + ARENA_SIZE) - (u8*)a);
    
    a->prev = nullptr;
    a->next = nullptr;
    a->count = 1;
    a->end = (u8*)a + size;
    a->type = TYPE_LARGE;
    a->lastpos = a->end;
    a->nextpos = nullptr;
    return (u8*)a + sizeof(arena);
}

static void freelarge(void* p) {
    arena* a = (arena*)((u64)p & ~(ARENA_SIZE - 1));
    munmap(a, (u8*)a->end - (u8*)a);
}

static arena** arenas[NUM_TYPES] = {
    nullptr,    // NONE
    &arena128,  // 128
    &arena256,    // 256
    &arena512,    // 512
    &arena1024,   // 1024
    &arena2048,   // 2048
    &arena4096,   // 4096
    &arena8192,   // 8192
    &arena16384,  // 16384
    &arena32768,  // 32768
    &arena65536,  // 65536
    nullptr,    // LARGE
};

void* _alloc(u64 size) {
    if (size <= 16) return alloc128();
    else if (size <= 32) return alloc256();
    else if (size <= 64) return alloc512();
    else if (size <= 128) return alloc1024();
    else if (size <= 256) return alloc2048();
    else if (size <= 512) return alloc4096();
    else if (size <= 1024) return alloc8192();
    else if (size <= 2048) return alloc16384();
    else if (size <= 4096) return alloc32768();
    else if (size <= 8192) return alloc65536();
    else return alloclarge(size);
}

void _free(void* p) {
    // find current arena
    arena* a = (arena*)((u64)p & ~(ARENA_SIZE - 1));

    if (a->type == TYPE_LARGE) return freelarge(p);

    a->count --;

    // free if empty
    if (a->count == 0) {
        if (a->prev) a->prev->next = a->next;
        if (a->next) a->next->prev = a->prev;

        // change base arena for allocation if we're deleting it
        if (arenas[a->type] && a == *arenas[a->type]) {
            if (a->next) *arenas[a->type] = a->next;
            else if (a->prev) *arenas[a->type] = a->prev;
            else *arenas[a->type] = nullptr;
        }
        munmap(a, ARENA_SIZE);
        return;
    }

    switch (a->type) {
        case TYPE_128:
            return free128(a, p);
        case TYPE_256:
            return free256(a, p);
        case TYPE_512:
            return free512(a, p);
        case TYPE_1024:
            return free1024(a, p);
        case TYPE_2048:
            return free2048(a, p);
        case TYPE_4096:
            return free4096(a, p);
        case TYPE_8192:
            return free8192(a, p);
        case TYPE_16384:
            return free16384(a, p);
        case TYPE_32768:
            return free32768(a, p);
        case TYPE_65536:
            return free65536(a, p);
        default:
            return;
    }
}

/* * * * * * * * * * * * * * * *
 *                             *
 *   Ref-Counting Utilities    *
 *                             *
 * * * * * * * * * * * * * * * */

#define IMUT_FLAG 1
#define NONRC_FLAG 2

i64 _rccount(void* ref) {
    return *(i64*)((u8*)ref - 8);
}

void _rcinc(void* ref) {
    i64 count = *(u64*)((u8*)ref - 8) + 16; // incr count
    if (!(count & IMUT_FLAG)) *(u64*)((u8*)ref - 8) = count;
}

void _printi64(i64 count);

void _rcdec(void* ref) {
    i64 count = *(i64*)((u8*)ref - 8) - 16;
    if (count & IMUT_FLAG) return;
    else if (count <= 0 && !(count & NONRC_FLAG)) _free((u8*)ref - 8);
    else *(i64*)((u8*)ref - 8) = count;
}

void* _rccopy(void* dst, void* src) {
    _rcinc(src);
    dst = src;
    return dst;
}

void* _rcassign(void* dst, void* src) {
    _rcinc(src);
    void* prev = dst;
    dst = src;
    _rcdec(prev);
    return dst;
}

void* _rcnew(u64 size) {
    size += 8;
    u8* ptr = _alloc(size);
    *(i64*)ptr = 16; // 1 ref, no flags
    return ptr + 8;
}

/* * * * * * * * * * * * * * * *
 *                             *
 *      String Functions       *
 *                             *
 * * * * * * * * * * * * * * * */

void* _strnew(u64 length, u8* data) {
    void* str = _rcnew(length + 8); // includes 8 bytes for string length
    *(u64*)str = length; // store length
    if (data) for (u64 i = 0; i < length; i ++) {
        ((u8*)str)[i + 8] = data[i]; // copy string data
    }
    return str;
}

i64 _strcmp(void* a, void* b) {
    u64 asize = *(u64*)a, bsize = *(u64*)b;
    a = (u8*)a + 8, b = (u8*)b + 8;
    u8* ai = a, *bi = b;
    while (ai < (u8*)a + asize && bi < (u8*)b + bsize) {
        if (*ai != *bi) return (i64)*ai - (i64)*bi;
        ++ ai, ++ bi;
    }
    return (i64)asize - (i64)bsize;
}

u64 _strlen(void* str) {
    return *(u64*)str;
}

void* _strcopyref(void* dst, void* src) {
    _rccopy(dst, src);
}

void* _strassign(void* dst, void* src) {
    _rcassign(dst, src);
}

void* _strcat(void* a, void* b) {
    void* dst = _strnew(_strlen(a) + _strlen(b), nullptr);
    u8* writ = (u8*)dst + 8;
    for (u64 i = 0; i < _strlen(a); i ++) *writ++ = ((u8*)a)[i + 8];
    for (u64 i = 0; i < _strlen(b); i ++) *writ++ = ((u8*)b)[i + 8];
    return dst;
}

void* _strcpy(void* str) {
    return _strnew(_strlen(str), (u8*)str + 8);
}

u8 _strget(void* str, u64 i) {
    if (i > _strlen(str)) _panic("Runtime error: out-of-bounds access of string.");
    return ((u8*)str)[i + 8];
}

void _strset(void** str, u64 i, u8 ch) {
    if (i > _strlen(str)) _panic("Runtime error: out-of-bounds access of string.");
    if (_rccount(str) & IMUT_FLAG) *str = _strcpy(*str);
    ((u8*)*str)[i] = ch;
}

/* * * * * * * * * * * * * * * *
 *                             *
 *       List Functions        *
 *                             *
 * * * * * * * * * * * * * * * */

void* _lshead(void* list) {
    if (!list) _panic("Runtime error: tried to get head of empty list.");
    return (u8*)list + 8;
}

void* _lstail(void* list) {
    if (!list) _panic("Runtime error: tried to get tail of empty list.");
    return *(void**)list;
}

u64 _lsempty(void* list) {
    return list ? 1 : 0;
}

void* _cons(u64 size, void* next) {
    u8* dst = _rcnew(size + 8); // include next in size
    *(void**)dst = next;
    return dst;
}

/* * * * * * * * * * * * * * * *
 *                             *
 *      Input and Output       *
 *                             *
 * * * * * * * * * * * * * * * */

void _printi64(i64 i) {
    static u8 buf[32];
    static u8* writ = buf;
    writ = buf;

    if (i < 0) *writ++ = '-', i = -i;
    i64 m = i, p = 1;
    while (m / 10) m /= 10, p *= 10;
    while (p) *writ++ = '0' + (i / p % 10), p /= 10;

    write(0, buf, writ - buf);
}

void _printu64(u64 u) {
    static u8 buf[32];
    static u8* writ;
    writ = buf;

    u64 m = u, p = 1;
    while (m / 10) m /= 10, p *= 10;
    while (p) *writ++ = '0' + (u / p % 10), p /= 10;

    write(0, buf, writ - buf);
}

void _printf64(double d) {
    static u8 buf[64];
    static u8* writ;
    writ = buf;

    u64 u = (u64)d; // before decimal point
    u64 m = u, p = 1;
    while (m / 10) m /= 10, p *= 10;
    while (p) *writ++ = '0' + (u / p % 10), p /= 10;

    *writ++ = '.';

    double r = d - u; // after decimal point
    p = 10;
    u32 zeroes = 0;
    int isZero = r == 0 ? 1 : 0;
    while (r && p) {
        r *= 10;
        if ((u8)r) {
            isZero = 0;
            while (zeroes) *writ++ = '0', -- zeroes;
            *writ++ = '0' + (u8)r;
        }
        else ++ zeroes;
        r -= (u8)r;
        -- p;
    }
    if (isZero) *writ++ = '0';

    write(0, buf, writ - buf);
}

i64 prev = 0;

void _printstr(void* str) {
    // if (!prev) prev = now();
    // else {
    //     // write(0, "time since last print:\n\0\0", 23);
    //     u64 n = now();
    //     _printf64((n - prev) / 1000000000.0);
    //     prev = n;
    // }
    write(0, (u8*)str + 8, _strlen(str));
}

void _printbool(i8 b) {
    if (b) write(0, "true", 4);
    else write(0, "false", 5);
}

/* * * * * * * * * * * * *
 *                       *
 *      Utilities        *
 *                       *
 * * * * * * * * * * * * */

u64 _now() {
    return now();
}
