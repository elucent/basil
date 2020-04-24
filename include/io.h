#ifndef BASIL_IO_H
#define BASIL_IO_H

#include "defs.h"
#include "stdio.h"

class stream {
public:
    virtual void write(u8 c) = 0;
    virtual u8 read() = 0; 
    virtual u8 peek() const = 0;
    virtual void unget(u8 c) = 0;
    virtual operator bool() const = 0;
};

class file : public stream {
    FILE* f;
    bool done;
public:
    file(const char* fname);
    file(FILE* f_in);
    ~file();
    file(const file& other) = delete;
    file& operator=(const file& other) = delete;

    void write(u8 c) override;
    u8 read() override;
    u8 peek() const override;
    void unget(u8 c) override;
    operator bool() const override;
};

class buffer : public stream {
    u8* data;
    u32 _start, _end, _capacity;
    u8 buf[8];

    void init(u32 size);
    void free();
    void copy(u8* other, u32 size, u32 start, u32 end);
    void grow();
public:
    buffer();
    ~buffer();
    buffer(const buffer& other);
    buffer& operator=(const buffer& other);

    void write(u8 c) override;
    u8 read() override;
    u8 peek() const override;
    void unget(u8 c) override;
    u32 size() const;
    u32 capacity() const;
    operator bool() const override;
    u8* begin();
    const u8* begin() const;
    u8* end();
    const u8* end() const;
};

extern stream &_stdin, &_stdout;
void setprecision(u32 p);

void print(stream& io, u8 c);
void print(stream& io, u16 n);
void print(stream& io, u32 n);
void print(stream& io, u64 n);
void print(stream& io, i8 c);
void print(stream& io, i16 n);
void print(stream& io, i32 n);
void print(stream& io, i64 n);
void print(stream& io, float f);
void print(stream& io, double d);
void print(stream& io, char c);
void print(stream& io, bool b);
void print(stream& io, const u8* s);
void print(stream& io, const char* s);
void print(stream& io, const buffer& b);

void print(u8 c);
void print(u16 n);
void print(u32 n);
void print(u64 n);
void print(i8 c);
void print(i16 n);
void print(i32 n);
void print(i64 n);
void print(float f);
void print(double d);
void print(char c);
void print(bool b);
void print(const u8* s);
void print(const char* s);
void print(const buffer& b);

template<typename T, typename... Args>
void print(stream& io, const T& a, const Args&... args) {
    print(io, a);
    print(io, args...);
}

template<typename T, typename... Args>
void print(const T& a, const Args&... args) {
    print(a);
    print(args...);
}

template<typename S, typename... Args>
void fprint(S& s, const Args&... args) {
    print((stream&)s, args...);
}

template<typename... Args>
void println(stream& io, const Args&... args) {
    print(io, args...);
    print(io, '\n');
}

template<typename... Args>
void println(const Args&... args) {
    print(args...);
    print('\n');
}

template<typename S, typename... Args>
void fprintln(S& s, const Args&... args) {
    println((stream&)s, args...);
}

bool isspace(u8 c);

void read(stream& io, u8& c);
void read(stream& io, u16& n);
void read(stream& io, u32& n);
void read(stream& io, u64& n);
void read(stream& io, i8& c);
void read(stream& io, i16& n);
void read(stream& io, i32& n);
void read(stream& io, i64& n);
void read(stream& io, float& f);
void read(stream& io, double& d);
void read(stream& io, char& c);
void read(stream& io, bool& b);
void read(stream& io, u8* s);
void read(stream& io, char* s);

void read(u8& c);
void read(u16& n);
void read(u32& n);
void read(u64& n);
void read(i8& c);
void read(i16& n);
void read(i32& n);
void read(i64& n);
void read(float& f);
void read(double& d);
void read(char& c);
void read(bool& b);
void read(u8* s);
void read(char* s);

template<typename T, typename... Args>
void read(stream& io, T& t, Args&... args) {
    read(io, t);
    read(io, args...);
}

template<typename T, typename... Args>
void read(T& t, Args&... args) {
    read(t);
    read(args...);
}

template<typename S, typename... Args>
void fread(S& s, Args&... args) {
    read((stream&)s, args...);
}

#endif