// encrypt.h — compile-time XOR string encryption
// defeats static string scanning (IDA strings view, YARA, AV signature)

#pragma once
#include <array>
#include <string>

template<size_t N>
struct XStr {
    char data[N]{};
    constexpr XStr(const char (&s)[N], char key) {
        for (size_t i = 0; i < N; i++) data[i] = s[i] ^ (key + i);
    }
    std::string dec(char key) const {
        std::string r(N - 1, '\0');
        for (size_t i = 0; i < N - 1; i++) r[i] = data[i] ^ (key + i);
        return r;
    }
};

#define KEY 0x5A
#define ENC(s) (XStr<sizeof(s)>(s, KEY).dec(KEY))

// usage: std::string procName = ENC("RobRun.exe");
// "RobRun.exe" never appears as plaintext in the binary
