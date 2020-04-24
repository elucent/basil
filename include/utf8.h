#ifndef BASIL_UNICODE_H
#define BASIL_UNICODE_H

#include "defs.h"
#include "hash.h"

struct uchar {
	u8 data[4];
public:
	uchar(u8 a = '\0', u8 b = '\0', u8 c = '\0', u8 d = '\0');
	explicit uchar(const char* str);

	u32 size() const;
	u32 point() const;
	u8& operator[](u32 i);
	u8 operator[](u32 i) const;
	bool operator==(char c) const;
	bool operator!=(char c) const;
	bool operator==(uchar c) const;
	bool operator!=(uchar c) const;
	bool operator<(uchar c) const;
	bool operator>(uchar c) const;
	bool operator<=(uchar c) const;
	bool operator>=(uchar c) const;
	operator bool() const;
};

bool isspace(uchar c);
bool iscontrol(uchar c);
bool isdigit(uchar c);
bool isalpha(uchar c);
bool isalnum(uchar c);
bool issym(uchar c);
bool isprint(uchar c);

class ustring {
    uchar* data;
    u32 _size, _capacity;

    void free();
    void init(u32 size);
    void copy(const uchar* s);
    void grow();
    i32 cmp(const uchar* s) const;
    i32 cmp(const char* s) const;

public:
    ustring();
    ~ustring();
    ustring(const ustring& other);
    ustring(const char* s);
    ustring& operator=(const ustring& other);

    ustring& operator+=(uchar c);
    ustring& operator+=(char c);
    ustring& operator+=(const uchar* s);
    ustring& operator+=(const char* s);
    ustring& operator+=(const ustring& s);
    void pop();
    u32 size() const;
    u32 capacity() const;
    uchar operator[](u32 i) const;
    uchar& operator[](u32 i);
    const uchar* raw() const;
    bool operator==(const uchar* s) const;
    bool operator==(const char* s) const;
    bool operator==(const ustring& s) const;
    bool operator<(const uchar* s) const;
    bool operator<(const char* s) const;
    bool operator<(const ustring& s) const;
    bool operator>(const uchar* s) const;
    bool operator>(const char* s) const;
    bool operator>(const ustring& s) const;
    bool operator!=(const uchar* s) const;
    bool operator!=(const char* s) const;
    bool operator!=(const ustring& s) const;
    bool operator<=(const uchar* s) const;
    bool operator<=(const char* s) const;
    bool operator<=(const ustring& s) const;
    bool operator>=(const uchar* s) const;
    bool operator>=(const char* s) const;
    bool operator>=(const ustring& s) const;
};

ustring operator+(ustring s, uchar c);
ustring operator+(ustring s, const char* cs);
ustring operator+(ustring s, const ustring& cs);
ustring escape(const ustring& s);

template<>
u64 hash(const ustring& s);

void print(stream& io, uchar c);
void print(uchar c);
void print(stream& io, const ustring& s);
void print(const ustring& s);
void read(stream& io, uchar& c);
void read(uchar& c);
void read(stream& io, ustring& s);
void read(stream& io, ustring& s);

#endif