#include "str.h"
#include "io.h"

void string::free() {
    if (_capacity > 16) delete[] data;
}

void string::init(u32 size) {
    if (size > 16) {
        _size = 0, _capacity = size;
        data = new u8[_capacity];
    }
    else data = buf, _size = 0, _capacity = 16;
    *data = '\0';
}

void string::copy(const u8* s) {
    u8* dptr = data;
    while (*s) *(dptr ++) = *(s ++), ++ _size;
    *dptr = '\0';
}

void string::grow() {
    u8* old = data;
    init(_capacity * 2);
    copy(old);
    if (_capacity / 2 > 16) delete[] old;
}

i32 string::cmp(const u8* s) const {
    u8* dptr = data;
    while (*s && *s == *dptr) ++ s, ++ dptr;
    return i32(*dptr) - i32(*s);
}

i32 string::cmp(const char* s) const {
    u8* dptr = data;
    while (*s && *s == *dptr) ++ s, ++ dptr;
    return i32(*dptr) - i32(*s);
}

string::string() { 
    init(16); 
}

string::~string() {
    free();
}

string::string(const string& other) {
    init(other._capacity);
    copy(other.data);
}

string::string(const char* s): string() {
    operator+=(s);
}

string& string::operator=(const string& other) {
    if (this != &other) {
        free();
        init(other._capacity);
        copy(other.data);
    }
    return *this;
}

string& string::operator+=(u8 c) {
    if (_size + 1 >= _capacity) grow();
    data[_size ++] = c;
    data[_size] = '\0';
    return *this;
}

string& string::operator+=(char c) {
    while (_size + 1 >= _capacity) grow();
    data[_size ++] = c;
    data[_size] = '\0';
    return *this;
}

string& string::operator+=(const u8* s) {
    u32 size = 0;
    const u8* sptr = s;
    while (*sptr) ++ size, ++ sptr;
    ++ size; // account for null char
    while (_size + size >= _capacity) grow();
    u8* dptr = data + _size;
    sptr = s;
    while (*sptr) *(dptr ++) = *(sptr ++);
    *dptr = '\0';
    _size += size;
    return *this;
}

string& string::operator+=(const char* s) {
    u32 size = 0;
    const char* sptr = s;
    while (*sptr) ++ size, ++ sptr;
    ++ size; // account for null char
    while (_size + size >= _capacity) grow();
    u8* dptr = data + _size;
    sptr = s;
    while (*sptr) *(dptr ++) = *(sptr ++);
    *dptr = '\0';
    _size += size;
    return *this;
}

string& string::operator+=(const string& s) {
    return operator+=(s.data);
}

u32 string::size() const {
    return _size;
}

u32 string::capacity() const {
    return _capacity;
}

u8 string::operator[](u32 i) const {
    return data[i];
}

u8& string::operator[](u32 i) {
    return data[i];
}

const u8* string::raw() const {
    return data;
}

bool string::operator==(const u8* s) const {
    return cmp(s) == 0;
}

bool string::operator==(const char* s) const {
    return cmp(s) == 0;
}

bool string::operator==(const string& s) const {
    return cmp(s.data) == 0;
}

bool string::operator<(const u8* s) const {
    return cmp(s) < 0;
}

bool string::operator<(const char* s) const {
    return cmp(s) < 0;
}

bool string::operator<(const string& s) const {
    return cmp(s.data) < 0;
}

bool string::operator>(const u8* s) const {
    return cmp(s) > 0;
}

bool string::operator>(const char* s) const {
    return cmp(s) > 0;
}

bool string::operator>(const string& s) const {
    return cmp(s.data) > 0;
}

bool string::operator!=(const u8* s) const {
    return cmp(s) != 0;
}

bool string::operator!=(const char* s) const {
    return cmp(s) != 0;
}

bool string::operator!=(const string& s) const {
    return cmp(s.data) != 0;
}

bool string::operator<=(const u8* s) const {
    return cmp(s) <= 0;
}

bool string::operator<=(const char* s) const {
    return cmp(s) <= 0;
}

bool string::operator<=(const string& s) const {
    return cmp(s.data) <= 0;
}

bool string::operator>=(const u8* s) const {
    return cmp(s) >= 0;
}

bool string::operator>=(const char* s) const {
    return cmp(s) >= 0;
}

bool string::operator>=(const string& s) const {
    return cmp(s.data) >= 0;
}

string operator+(string s, char c) {
    s += c;
    return s;
}

string operator+(string s, const char* cs) {
    s += cs;
    return s;
}

string operator+(string s, const string& cs) {
    s += cs;
    return s;
}

void print(stream& io, const string& s) {
    print(io, s.raw());
}

void print(const string& s) {
    print(_stdout, s);
}

void read(stream& io, string& s) {
    while (isspace(io.peek())) io.read(); // consume leading spaces
    while (io.peek() && !isspace(io.peek())) s += io.read();
}

void read(string& s) {
    read(_stdin, s);
}