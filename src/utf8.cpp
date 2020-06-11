#include "utf8.h"
#include "io.h"

uchar::uchar(u8 a, u8 b, u8 c, u8 d) {
    data[0] = a;
    data[1] = b;
    data[2] = c;
    data[3] = d;
}

uchar::uchar(const char* str): uchar((u8)'\0') {
    data[0] = *str ++;
    for (uint8_t i = 1; i < size(); i ++) data[i] = *str ++;
}

u32 uchar::size() const {
    if (!(data[0] >> 7 & 1)) return 1;
    else if ((data[0] >> 5 & 7) == 6) return 2;
    else if ((data[0] >> 4 & 15) == 14) return 3;
    else if ((data[0] >> 3 & 31) == 30) return 4;
    else return 0;
}

u32 uchar::point() const {
    switch (size()) {
    case 1:
        return data[0];
    case 2:
        return (data[0] & 31) << 6 | (data[1] & 63);
    case 3:
        return (data[0] & 15) << 12
            |  (data[1] & 63) << 6
            |  (data[2] & 63);
    case 4:
        return (data[0] & 7) << 18
            |  (data[1] & 63) << 12
            |  (data[2] & 63) << 6
            |  (data[3] & 63);
    }
    return 0;
}

u8& uchar::operator[](u32 i) {
    return data[i];
}

u8 uchar::operator[](u32 i) const {
    return data[i];
}

bool uchar::operator==(char c) const {
    return data[0] < 128 && c == data[0];
}

bool uchar::operator!=(char c) const {
    return c != data[0];
}

bool uchar::operator==(uchar c) const {
    for (u32 i = 0; i < 4; i ++) {
        if (data[i] != c[i]) return false;
        if (!data[i] && !data[c]) return true;;
    }
    return true;
}

bool uchar::operator!=(uchar c) const {
    return !operator==(c);
}

bool uchar::operator<(uchar c) const {
    for (u32 i = 0; i < 4 && data[i]; i ++) {
        if (data[i] != c[i]) return data[i] < c[i];
    }
    return false;
}

bool uchar::operator>(uchar c) const {
    return !operator<(c) && !operator==(c);
}

bool uchar::operator<=(uchar c) const {
    return operator<(c) || operator==(c);
}

bool uchar::operator>=(uchar c) const {
    return !operator<(c);
}

uchar::operator bool() const {
    return data[0];
}

static const u32 SPACES[] = {
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x00A0,
    0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009,
    0x200A, 0x200B,	0x2028, 0x2029, 0x202D, 0x202E, 0x202F, 0x205F,
    0x3000, 0xFEFF
};
static const u32 NUM_SPACES = sizeof(SPACES) / sizeof(u32);

static const u32 CONTROLS[] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x007F
};
static const u32 NUM_CONTROLS = sizeof(CONTROLS) / sizeof(u32);

static const u32 DIGITS[] = {
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, // ASCII
    0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, // Arabic-Indic
    0x06F0, 0x06F1, 0x06F2, 0x06F3, 0x06F4, 0x06F5, 0x06F6, 0x06F7,
    0x06F8, 0x06F9, // Extended Arabic-Indic
    0x07C0, 0x07C1, 0x07C2, 0x07C3, 0x07C4, 0x07C5, 0x07C6, 0x07C7,
    0x07C8, 0x07C9, // N'ko
    0x0966, 0x0967, 0x0968, 0x0969, 0x096A, 0x096B, 0x096C, 0x096D,
    0x096E, 0x096F, // Devanagari
    0x09E6, 0x09E7, 0x09E8, 0x09E9, 0x09EA, 0x09EB, 0x09EC, 0x09ED,
    0x09EE, 0x09EF, // Bengali
    0x0A66, 0x0A67, 0x0A68, 0x0A69, 0x0A6A, 0x0A6B, 0x0A6C, 0x0A6D,
    0x0A6E, 0x0A6F, // Gurmukhi
    0x0AE6, 0x0AE7, 0x0AE8, 0x0AE9, 0x0AEA, 0x0AEB, 0x0AEC, 0x0AED,
    0x0AEE, 0x0AEF, // Gujarati
    0x0B66, 0x0B67, 0x0B68, 0x0B69, 0x0B6A, 0x0B6B, 0x0B6C, 0x0B6D,
    0x0B6E, 0x0B6F, // Oriya
    0x0BE6, 0x0BE7, 0x0BE8, 0x0BE9, 0x0BEA, 0x0BEB, 0x0BEC, 0x0BED,
    0x0BEE, 0x0BEF, // Tamil
    0x0C66, 0x0C67, 0x0C68, 0x0C69, 0x0C6A, 0x0C6B, 0x0C6C, 0x0C6D,
    0x0C6E, 0x0C6F, // Telugu
    0x0CE6, 0x0CE7, 0x0CE8, 0x0CE9, 0x0CEA, 0x0CEB, 0x0CEC, 0x0CED,
    0x0CEE, 0x0CEF, // Kannada
    0x0D66, 0x0D67, 0x0D68, 0x0D69, 0x0D6A, 0x0D6B, 0x0D6C, 0x0D6D,
    0x0D6E, 0x0D6F, // Malayalam
    0x0DE6, 0x0DE7, 0x0DE8, 0x0DE9, 0x0DEA, 0x0DEB, 0x0DEC, 0x0DED,
    0x0DEE, 0x0DEF, // Sinhala
    0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54, 0x0E55, 0x0E56, 0x0E57,
    0x0E58, 0x0E59, // Thai
    0x0ED0, 0x0ED1, 0x0ED2, 0x0ED3, 0x0ED4, 0x0ED5, 0x0ED6, 0x0ED7,
    0x0ED8, 0x0ED9, // Lao
    0x0F20, 0x0F21, 0x0F22, 0x0F23, 0x0F24, 0x0F25, 0x0F26, 0x0F27,
    0x0F28, 0x0F29, // Tibetan
    0x1040, 0x1041, 0x1042, 0x1043, 0x1044, 0x1045, 0x1046, 0x1047,
    0x1048, 0x1049, // Myanmar
    0x1090, 0x1091, 0x1092, 0x1093, 0x1094, 0x1095, 0x1096, 0x1097,
    0x1098, 0x1099, // Myanmar Shan
    0x1369, 0x136A, 0x136B, 0x136C, 0x136D, 0x136E, 0x136F, 0x1370,
    0x1371, // Ethiopic
    0x17E0, 0x17E1, 0x17E2, 0x17E3, 0x17E4, 0x17E5, 0x17E6, 0x17E7,
    0x17E8, 0x17E9, // Khmer
    0x1810, 0x1811, 0x1812, 0x1813, 0x1814, 0x1815, 0x1816, 0x1817,
    0x1818, 0x1819, // Mongolian	
};
static const u32 NUM_DIGITS = sizeof(DIGITS) / sizeof(u32);

static bool in(u32 code, const u32* array, u32 length) {
    u32 l = 0, h = length - 1;
    while (true) {
        if (code == array[l] || code == array[h]) return true;
        u32 m = (h + l) / 2;
        if (l == m || h == m) return false;
        if (code > array[m]) l = m;
        else if (code < array[m]) h = m;
        else return true;
    }
    return false;
}

bool isspace(uchar c) {
    return in(c.point(), SPACES, NUM_SPACES);
}

bool iscontrol(uchar c) {
    return in(c.point(), CONTROLS, NUM_CONTROLS);
}

bool isdigit(uchar c) {
    return in(c.point(), DIGITS, NUM_DIGITS);
}

bool isalpha(uchar c) {
    return (c[0] >= 0x0041 && c[0] <= 0x005A)
        || (c[0] >= 0x0061 && c[0] <= 0x007A);
}

bool isalnum(uchar c) {
    return isalpha(c) || isdigit(c);
}

bool issym(uchar c) {
    return !isspace(c) && !iscontrol(c);
}

bool isprint(uchar c) {
    return !iscontrol(c);
}

void ustring::free() {
    delete[] data;
}

void ustring::init(u32 size) {
    _size = 0, _capacity = size;
    data = new uchar[_capacity];
}

void ustring::copy(const uchar* s) {
    uchar* dptr = data;
    while (*s) *(dptr ++) = *(s ++), ++ _size;
    *dptr = '\0';
}

void ustring::grow() {
    uchar* old = data;
    init(_capacity * 2);
    copy(old);
    delete[] old;
}

i32 ustring::cmp(const uchar* s) const {
    uchar* dptr = data;
    while (*s && *s == *dptr) ++ s, ++ dptr;
    if (*dptr == *s) return 0;
    else if (*dptr > *s) return 1;
    else return -1;
}

i32 ustring::cmp(const char* s) const {
    uchar* dptr = data;
    while (*s) { 
        uchar c(s);
        if (c != *dptr) break;
        s += c.size(), ++ dptr;
    }
    uchar c(s);
    if (*dptr == c) return 0;
    else if (*dptr > c) return 1;
    else return -1;
}

ustring::ustring() { 
    init(8); 
}

ustring::~ustring() {
    free();
}

ustring::ustring(const ustring& other) {
    init(other._capacity);
    copy(other.data);
}

ustring::ustring(const char* s): ustring() {
    operator+=(s);
}

ustring& ustring::operator=(const ustring& other) {
    if (this != &other) {
        free();
        init(other._capacity);
        copy(other.data);
    }
    return *this;
}

ustring& ustring::operator+=(uchar c) {
    if (!c) return *this;
    if (_size + 1 >= _capacity) grow();
    data[_size ++] = c;
    return *this;
}

ustring& ustring::operator+=(char c) {
    if (!c) return *this;
    while (_size + 1 >= _capacity) grow();
    data[_size ++] = c;
    return *this;
}

ustring& ustring::operator+=(const uchar* s) {
    u32 size = 0;
    const uchar* sptr = s;
    while (*sptr) ++ size, ++ sptr;
    while (_size + size >= _capacity) grow();
    uchar* dptr = data + _size;
    sptr = s;
    while (*sptr) *(dptr ++) = *(sptr ++);
    *dptr = '\0';
    _size += size;
    return *this;
}

ustring& ustring::operator+=(const char* s) {
    u32 size = 0;
    const char* sptr = s;
    while (*sptr) {
        uchar c(sptr);
        ++ size; 
        sptr += c.size();
    }
    while (_size + size >= _capacity) grow();
    uchar* dptr = data + _size;
    sptr = s;
    while (*sptr) {
        uchar c(sptr);
        *(dptr ++) = c;
        sptr += c.size();
    }
    *dptr = '\0';
    _size += size;
    return *this;
}

ustring& ustring::operator+=(const ustring& s) {
    return operator+=(s.data);
}

void ustring::pop() {
    _size --;
    data[_size] == '\0';
}

u32 ustring::size() const {
    return _size;
}

u32 ustring::capacity() const {
    return _capacity;
}

uchar ustring::operator[](u32 i) const {
    return data[i];
}

uchar& ustring::operator[](u32 i) {
    return data[i];
}

const uchar* ustring::raw() const {
    return data;
}

bool ustring::operator==(const uchar* s) const {
    return cmp(s) == 0;
}

bool ustring::operator==(const char* s) const {
    return cmp(s) == 0;
}

bool ustring::operator==(const ustring& s) const {
    return cmp(s.data) == 0;
}

bool ustring::operator<(const uchar* s) const {
    return cmp(s) < 0;
}

bool ustring::operator<(const char* s) const {
    return cmp(s) < 0;
}

bool ustring::operator<(const ustring& s) const {
    return cmp(s.data) < 0;
}

bool ustring::operator>(const uchar* s) const {
    return cmp(s) > 0;
}

bool ustring::operator>(const char* s) const {
    return cmp(s) > 0;
}

bool ustring::operator>(const ustring& s) const {
    return cmp(s.data) > 0;
}

bool ustring::operator!=(const uchar* s) const {
    return cmp(s) != 0;
}

bool ustring::operator!=(const char* s) const {
    return cmp(s) != 0;
}

bool ustring::operator!=(const ustring& s) const {
    return cmp(s.data) != 0;
}

bool ustring::operator<=(const uchar* s) const {
    return cmp(s) <= 0;
}

bool ustring::operator<=(const char* s) const {
    return cmp(s) <= 0;
}

bool ustring::operator<=(const ustring& s) const {
    return cmp(s.data) <= 0;
}

bool ustring::operator>=(const uchar* s) const {
    return cmp(s) >= 0;
}

bool ustring::operator>=(const char* s) const {
    return cmp(s) >= 0;
}

bool ustring::operator>=(const ustring& s) const {
    return cmp(s.data) >= 0;
}

ustring operator+(ustring s, uchar c) {
    s += c;
    return s;
}

ustring operator+(ustring s, const char* cs) {
    s += cs;
    return s;
}

ustring operator+(ustring s, const ustring& cs) {
    s += cs;
    return s;
}

ustring escape(const ustring& s) {
    ustring n = "";
    for (u32 i = 0; i < s.size(); i ++) {
        if (s[i] == '\n') n += "\\n";
        else if (s[i] == '\t') n += "\\t";
        else if (s[i] == '\r') n += "\\r";
        else if (s[i] == '\\') n += "\\\\";
        else if (s[i] == '\"') n += "\\\"";
        else if (s[i] == '\0') n += "\\0";
        else n += s[i];
    }
    return n;
}

template<>
u64 hash(const ustring& s) {
    return raw_hash(s.raw(), sizeof(uchar) * s.size());
}

void print(stream& io, uchar c) {
    for (u32 i = 0; i < c.size(); ++ i) io.write(c[i]);
}

void print(uchar c) {
    print(_stdout, c);
}

void print(stream& io, const ustring& s) {
    for (u32 i = 0; i < s.size(); ++ i) print(io, s[i]);
}

void print(const ustring& s) {
    print(_stdout, s);
}

void read(stream& io, uchar& c) {
    char buf[] = { '\0', '\0', '\0', '\0' };
    u32 size = uchar(io.peek()).size();
    for (u32 i = 0; i < size; i ++) buf[i] = io.read();
    c = uchar(buf);
}

void read(uchar& c) {
    read(_stdin, c);
}

void read(stream& io, ustring& s) {
    while (io.peek()) {
        uchar c;
        read(io, c);
        if (isspace(c)) {
            for (i32 i = c.size() - 1; i >= 0; -- i) io.unget(c[i]);
            break;
        }
        s += c;
    }
}

void read(ustring& s) {
    read(_stdin, s);
}
