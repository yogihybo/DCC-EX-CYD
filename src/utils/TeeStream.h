#pragma once

#include <Stream.h>
#include <WString.h>
#include <functional>

// Parsed fields from a DCC-EX <i...> server-description response.
struct ServerDescription {
    String board;
    String shield;
    String build;
};

// Parses "iDCC-EX V-x.x.x / BOARD / SHIELD / BUILD" into a ServerDescription.
// Returns true if the command was a valid <i...> response.
inline bool parseServerDescription(const char* cmd, ServerDescription& out) {
    if (!cmd || cmd[0] != 'i') return false;

    auto trimRight = [](const char* s, int len) -> String {
        while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\r' || s[len-1] == '\n')) len--;
        return String(s).substring(0, len);
    };

    const char* p = strchr(cmd, '/');
    if (!p) return false;
    p++; while (*p == ' ') p++;

    const char* next = strstr(p, " / ");
    if (!next) { out.board = trimRight(p, strlen(p)); return true; }
    out.board = trimRight(p, next - p);
    p = next + 3;

    next = strstr(p, " / ");
    if (!next) { out.shield = trimRight(p, strlen(p)); return true; }
    out.shield = trimRight(p, next - p);
    p = next + 3;

    next = strstr(p, " / ");
    out.build = trimRight(p, next ? (next - p) : (int)strlen(p));

    return true;
}

// Transparent Stream wrapper that intercepts complete <...> commands as they
// flow through read(), invoking a callback with the inner text (no angle brackets).
// Writes and all other Stream operations pass straight through to the inner stream.
class TeeStream : public Stream {
public:
  using CommandCallback = std::function<void(const char*)>;

  TeeStream(Stream& inner, CommandCallback onCommand)
      : _inner(inner), _onCommand(std::move(onCommand)) {}

  int    available() override                         { return _inner.available(); }
  int    peek()      override                         { return _inner.peek(); }
  void   flush()     override                         { _inner.flush(); }
  size_t write(uint8_t b) override                    { return _inner.write(b); }
  size_t write(const uint8_t* buf, size_t n) override { return _inner.write(buf, n); }

  int read() override {
    int c = _inner.read();
    if (c >= 0) _sniff((char)c);
    return c;
  }

private:
  Stream&         _inner;
  CommandCallback _onCommand;
  char            _buf[256];
  int             _bufLen = 0;
  bool            _inCmd  = false;

  void _sniff(char c) {
    if (c == '<') {
      _inCmd  = true;
      _bufLen = 0;
    } else if (c == '>') {
      if (_inCmd && _bufLen > 0) {
        _buf[_bufLen] = '\0';
        if (_onCommand) _onCommand(_buf);
      }
      _inCmd  = false;
      _bufLen = 0;
    } else if (_inCmd && _bufLen < (int)sizeof(_buf) - 1) {
      _buf[_bufLen++] = c;
    }
  }
};
