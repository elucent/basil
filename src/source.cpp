#include "source.h"

namespace basil {
    void Source::add(uchar c) {
        if (c == '\t') lines.back() += "    "; // expand tabs to 4 spaces
        else lines.back() += c;
        if (c == '\n') lines.push("");
    }

    void Source::checkNewline() {
        if (lines.size() == 0) {
            lines.push("\n");
            return;
        }
        i64 i = lines.size() - 1;
        while (i > 0 && lines[i].size() == 0) -- i;
        if (lines[i].size() == 0) {
            lines[i] += '\n';
        }
        else if (lines[i][lines[i].size() - 1] != '\n') add('\n');
    }

    Source::Source() {
        lines.push("");
    }

    Source::Source(const char* path) {
        lines.push("");
        file f(path, "r");
        load(f);
    }

    Source::Source(stream& f) {
        lines.push("");
        load(f);
    }

    Source::View::View(const Source* src_in): src(src_in), _line(0), _column(0) {
        //  
    }

    Source::View::View(const Source* src_in, u32 line, u32 column): 
        src(src_in), _line(line), _column(column) {
        //  
    }

    void Source::View::rewind() {
        if (_column == 0 && _line > 0) {
            -- _line, _column = src->lines[_line].size();
        }
        else if (_column > 0) -- _column;
    }

    uchar Source::View::read() {
        if (_line >= src->lines.size()) return '\0';
        if (_line == src->lines.size() - 1 
            && _column >= src->line(_line).size()) return '\0';
        uchar c = src->lines[_line][_column];
        ++ _column;
        if (_column >= src->lines[_line].size() && _line < src->size() - 1) {
            _column = 0, ++ _line;
        } 
        return c;
    }

    uchar Source::View::peek() const {
        if (_line >= src->lines.size()) return '\0';
        if (_line == src->lines.size() - 1 
            && _column >= src->line(_line).size()) return '\0';
        return src->lines[_line][_column];
    }

    u32 Source::View::line() const {
        return _line + 1;
    }

    u32 Source::View::column() const {
        return _column + 1;
    }

    const Source* Source::View::source() const {
        return src;
    }

    void Source::load(stream& f) {
        uchar c;
        while (f.peek()) read(f, c), add(c);
        checkNewline();
    }

    void Source::add(const ustring& line) {
        for (u32 i = 0; i < line.size(); ++ i) add(line[i]);
        checkNewline();
    }

    ustring& Source::line(u32 line) {
        return lines[line];
    }

    const ustring& Source::line(u32 line) const {
        return lines[line];
    }

    u32 Source::size() const {
        return lines.size();
    }

    Source::View Source::view() const {
        return Source::View(this);
    }

    Source::View Source::expand(stream& io) {
        Source::View view(this, lines.size() - 1, 0);
        while (io.peek() && io.peek() != '\n') {
            uchar c;
            read(io, c);
            add(c);
        }
        if (io.peek() == '\n') add(io.read());
        return view;
    }
}
    
void print(stream& io, const basil::Source& src) {
    for (u32 i = 0; i < src.size(); ++ i) print(io, src.line(i));
}

void print(const basil::Source& src) {
    print(_stdout, src);
}  

void read(stream& io, basil::Source& src) {
    src.load(io);
}

void read(basil::Source& src) {
    read(_stdin, src);
}