#ifndef BASIL_SOURCE_H
#define BASIL_SOURCE_H

#include "vec.h"
#include "utf8.h"
#include "io.h"

namespace basil {
    class Source {
        vector<ustring> lines;
        void add(uchar c);
        void checkNewline();
    public:
        Source();
        Source(stream& f);
        explicit Source(const char* path);

        class View {
            const Source* src;
            u32 _line, _column;
        public:
            View(const Source* src_in);
            View(const Source* src_in, u32 line, u32 column);
            
            void rewind();
            uchar read();
            uchar peek() const;
            u32 line() const;
            u32 column() const;
            const Source* source() const;
        };

        void load(stream& f);
        void add(const ustring& line);
        void add(stream& io);
        ustring& line(u32 line);
        const ustring& line(u32 line) const;  
        u32 size() const;
        View view() const;
        View expand(stream& io);
    };
}
    
void print(stream& io, const basil::Source& src);
void print(const basil::Source& src);
void read(stream& io, basil::Source& src);
void read(basil::Source& src);

#endif