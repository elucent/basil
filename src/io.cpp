#include "io.h"
#include "str.h"

bool exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    else return fclose(f), true;
}

file::file(const char* fname): file(fopen(fname, "a+")) {
    //
}

file::file(FILE* f_in): f(f_in), done(false) {
    //
}

file::~file() {
    fclose(f);
}

void file::write(u8 c) {
    fputc(c, f);
}

u8 file::read() {
    if (done) return '\0';
    int i = fgetc(f);
    if (i == EOF) done = true;
    return i;
}

u8 file::peek() const {
    if (done) return '\0';
    int i = fgetc(f);
    ungetc(i, f);
    if (i == EOF) return '\0';
    return i;
}

void file::unget(u8 c) {
    ungetc(c, f);
}

file::operator bool() const {
    int i = fgetc(f);
    ungetc(i, f);
    return i != EOF;
}

void buffer::init(u32 size) {
    _start = 0, _end = 0, _capacity = size;
    if (_capacity <= 8) data = buf;
    else data = new u8[_capacity];
}

void buffer::free() {
    if (_capacity > 8) delete[] data;
}

void buffer::copy(u8* other, u32 size, u32 start, u32 end) {
    while (start != end) {
        data[_end ++] = other[start];
        start = (start + 1) & (size - 1);
    }
}

void buffer::grow() {
    u8* old = data;
    u32 oldcap = _capacity;
    u32 oldstart = _start, oldend = _end;
    init(_capacity *= 2);
    copy(old, oldcap, oldstart, oldend);
    if (oldcap > 8) delete[] old;
}

buffer::buffer() {
    init(8);
}

buffer::~buffer() {
    free();
}

buffer::buffer(const buffer& other) {
    init(other._capacity);
    copy(other.data, other._capacity, other._start, other._end);
}

buffer& buffer::operator=(const buffer& other) {
    if (this != &other) {
        free();
        init(other._capacity);
        copy(other.data, other._capacity, other._start, other._end);
    }
    return *this;
}

void buffer::write(u8 c) {
    if (((_end + 1) & (_capacity - 1)) == _start) grow();
    data[_end] = c;
    _end = (_end + 1) & (_capacity - 1);
}

u8 buffer::read() {
    if (_start == _end) return '\0';
    u8 c = data[_start];
    _start = (_start + 1) & (_capacity - 1);
    return c;
}

u8 buffer::peek() const {
    if (_start == _end) return '\0';
    return data[_start];
}

void buffer::unget(u8 c) {
    _start = (_start - 1) & (_capacity - 1);
    while (_start == _end) grow(), _start = (_start - 1) & (_capacity - 1);
    data[_start] = c;
}

u32 buffer::size() const {
    return (i64(_end) - i64(_start)) & (_capacity - 1);
}

u32 buffer::capacity() const {
    return _capacity;
}

buffer::operator bool() const {
    return _start != _end;
}

u8* buffer::begin() {
    return data + _start;
}

const u8* buffer::begin() const {
    return data + _start;
}

u8* buffer::end() {
    return data + _end;
}

const u8* buffer::end() const {
    return data + _end;
}


file _stdin_file(stdin), _stdout_file(stdout);

stream &_stdin = _stdin_file, &_stdout = _stdout_file;

static u32 precision = 5;

void setprecision(u32 p) {
    precision = p;
}

static void print_unsigned(stream& io, u64 n) {
    u64 m = n, p = 1;
    while (m / 10) m /= 10, p *= 10;
    while (p) io.write('0' + (n / p % 10)), p /= 10;
}

static void print_signed(stream& io, i64 n) {
    if (n < 0) io.write('-'), n = -n;
    print_unsigned(io, n);
}

static void print_rational(stream& io, double d) {
    if (d < 0) io.write('-'), d = -d;
    print_unsigned(io, u64(d));
    io.write('.');
    double r = d - u64(d);
    u32 p = precision, zeroes = 0;
    bool isZero = r == 0;
    while (r && p) {
        r *= 10;
        if (u8(r)) {
            isZero = false;
            while (zeroes) io.write('0'), -- zeroes;
            io.write('0' + u8(r));
        }
        else ++ zeroes;
        r -= u8(r);
        -- p;
    }
    if (isZero) io.write('0');
}

void print(stream& io, u8 c) {
    io.write(c);
}

void print(stream& io, u16 n) {
    print_unsigned(io, n);
}

void print(stream& io, u32 n) {
    print_unsigned(io, n);
}

void print(stream& io, u64 n) {
    print_unsigned(io, n);
}

void print(stream& io, i8 c) {
    io.write(c);
}

void print(stream& io, i16 n) {
    print_signed(io, n);
}

void print(stream& io, i32 n) {
    print_signed(io, n);
}

void print(stream& io, i64 n) {
    print_signed(io, n);
}

void print(stream& io, float f) {
    print_rational(io, f);
}

void print(stream& io, double d) {
    print_rational(io, d);
}

void print(stream& io, char c) {
    io.write(c);
}

void print(stream& io, bool b) {
    print(io, b ? "true" : "false");
}

void print(stream& io, const u8* s) {
    while (*s) io.write(*(s ++));
}

void print(stream& io, const char* s) {
    while (*s) io.write(*(s ++));
}

void print(u8 c) {
    print(_stdout, c);
}

void print(u16 n) {
    print(_stdout, n);
}

void print(u32 n) {
    print(_stdout, n);
}

void print(u64 n) {
    print(_stdout, n);
}

void print(i8 c) {
    print(_stdout, c);
}

void print(i16 n) {
    print(_stdout, n);
}

void print(i32 n) {
    print(_stdout, n);
}

void print(i64 n) {
    print(_stdout, n);
}

void print(char c) {
    print(_stdout, c);
}

void print(bool b) {
    print(_stdout, b);
}

void print(float f) {
    print(_stdout, f);
}

void print(double d) {
    print(_stdout, d);
}

void print(const u8* s) {
    print(_stdout, s);
}

void print(const char* s) {
    print(_stdout, s);
}

void print(stream& io, const buffer& b) {
    for (u8 c : b) io.write(c);
}

void print(const buffer& b) {
    print(_stdout, b);
}

bool isspace(u8 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static u64 read_unsigned(stream& io) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    u64 result = 0;
    while (isspace(io.peek())) io.read();
    while (io.peek()) {
        u8 c = io.peek();
        if (isspace(c)) break;
        else if (c < '0' || c > '9') {
            while (io.peek() && !isspace(io.peek())) io.read();
            return 0;
        }
        result *= 10;
        result += (c - '0');
        io.read();
    }
    return result;
}

static i64 read_signed(stream& io) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    if (io.peek() == '-') return io.read(), -read_unsigned(io);
    else return read_unsigned(io);
}

static double read_float(stream& io) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    buffer b;
    while (io.peek() && !isspace(io.peek()) && io.peek() != '.') {
        b.write(io.read());
    } 
    if (io.peek() != '.') return read_unsigned((stream&)b);
    else io.read();
    u64 i;
    fread(b, i);
    double pow = 0.1;
    double result = i;
    while (io.peek() && !isspace(io.peek())) {
        result += (io.read() - '0') * pow;
        pow *= 0.1;
    }
    return result;
}

void read(stream& io, u8& c) {
    c = io.read();
}

void read(stream& io, u16& n) {
    n = read_unsigned(io);
}

void read(stream& io, u32& n) {
    n = read_unsigned(io);
}

void read(stream& io, u64& n) {
    n = read_unsigned(io);
}

void read(stream& io, i8& c) {
    c = io.read();
}

void read(stream& io, i16& n) {
    n = read_signed(io);
}

void read(stream& io, i32& n) {
    n = read_signed(io);
}

void read(stream& io, i64& n) {
    n = read_signed(io);
}

void read(stream& io, float& f) {
    f = read_float(io);
}

void read(stream& io, double& d) {
    d = read_float(io);
}

void read(stream& io, char& c) {
    c = io.read();
}

void read(stream& io, bool& c) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    string s;
    while (!isspace(io.peek())) s += io.read();
    if (s == "true") c = true;
    else if (s == "false") c = false;
}

void read(stream& io, u8* s) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    while (u8 c = io.read()) *(s ++) = c;
}

void read(stream& io, char* s) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    while (u8 c = io.read()) *(s ++) = c;
}

void read(u8& c) {
    read(_stdin, c);
}

void read(u16& n) {
    read(_stdin, n);
}

void read(u32& n) {
    read(_stdin, n);
}

void read(u64& n) {
    read(_stdin, n);
}

void read(i8& c) {
    read(_stdin, c);
}

void read(i16& n) {
    read(_stdin, n);
}

void read(i32& n) {
    read(_stdin, n);
}

void read(i64& n) {
    read(_stdin, n);
}

void read(float& f) {
    read(_stdin, f);
}

void read(double& d) {
    read(_stdin, d);
}

void read(char& c) {
    read(_stdin, c);
}

void read(bool& b) {
    read(_stdin, b);
}

void read(u8* s) {
    read(_stdin, s);
}

void read(char* s) {
    read(_stdin, s);
}
