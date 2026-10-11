#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

extern "C" {
int APS5_VABI sceFontMemoryInit(FontMemory*, void*, std::uint32_t, const FontMemoryInterface*, void*, FontMemoryDestroyFunction, void*);
int APS5_VABI sceFontMemoryTerm(FontMemory*);
int APS5_VABI sceFontTextSourceInit(FontTextSource*, const void*, std::uint32_t, FontTextParseFunction, void*);
int APS5_VABI sceFontTextSourceSetDefaultFont(FontTextSource*, FontHandle);
int APS5_VABI sceFontCreateString(const FontMemory*, FontTextSource*, const FontCreateStringDetail*, FontString*);
int APS5_VABI sceFontDestroyString(FontString*);
FontTextCharacter* APS5_VABI sceFontStringRefersTextCharacters(FontString, std::uint32_t*);
int APS5_VABI sceFontCharacterGetSyllableStringState(const FontTextCharacter*, int*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font text check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename F>
static bool Throws(F f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

static std::int32_t APS5_VABI ParseCodes(FontTextSource* source, void** order, FontTextParseResult* result) {
    const auto* current = static_cast<const std::uint32_t*>(source->current);
    *order = const_cast<std::uint32_t*>(current);
    if (current == source->end) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    result->FontCode.font = nullptr;
    result->FontCode.code = *current;
    source->current = current + 1;
    return 1;
}

struct Text {
    FontString string = nullptr;
    FontTextCharacter* characters = nullptr;
    std::uint32_t count = 0;
};

static Text CreateText(const FontMemory& memory, const std::vector<std::uint32_t>& codes, FontHandle font) {
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, codes.data(), static_cast<std::uint32_t>(codes.size() * sizeof(std::uint32_t)), ParseCodes, nullptr) == SCE_FONT_OK);
    Require(sceFontTextSourceSetDefaultFont(&source, font) == SCE_FONT_OK);
    Text text;
    Require(sceFontCreateString(&memory, &source, nullptr, &text.string) == SCE_FONT_OK && text.string != nullptr);
    text.characters = sceFontStringRefersTextCharacters(text.string, &text.count);
    Require(text.characters != nullptr && text.count == codes.size());
    return text;
}

static bool StateIs(const FontTextCharacter* character, int expected) {
    int state = -1;
    return sceFontCharacterGetSyllableStringState(character, &state) == SCE_FONT_OK && state == expected;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontHandleOpaque fontStorage{};
    const FontHandle font = &fontStorage;

    const std::vector<std::uint32_t> plain{'A', 'b', ' ', '(', '"', '[', 0x00E9, 0x03B1, 0x0436, 0x3042, 0x30AB, 0x4E00, 0xFF01};
    Text text = CreateText(memory, plain, font);
    for (std::uint32_t i = 0; i < text.count; ++i) Require(StateIs(&text.characters[i], 0));
    int state = 7;
    Require(sceFontCharacterGetSyllableStringState(nullptr, &state) == SCE_FONT_ERROR_INVALID_PARAMETER && state == 7);
    Require(sceFontCharacterGetSyllableStringState(&text.characters[0], nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDestroyString(&text.string) == SCE_FONT_OK && text.string == nullptr);

    const std::vector<std::uint32_t> marks{'x', 'e', 0x0301, 'y', 'z', 0x0E01, 0x0E35, ' ', 0xAC00};
    Text mixed = CreateText(memory, marks, font);
    Require(StateIs(&mixed.characters[0], 0));
    for (std::uint32_t i = 1; i < mixed.count; ++i) {
        Require(Throws([&] { sceFontCharacterGetSyllableStringState(&mixed.characters[i], &state); }) && state == 7);
    }
    Require(sceFontDestroyString(&mixed.string) == SCE_FONT_OK);

    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(allocations == 0);
    return 0;
}
