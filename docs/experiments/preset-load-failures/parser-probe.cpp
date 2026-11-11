#include <juce_core/juce_core.h>
#include <cstdio>
int main()
{
    for (const char* input : { "<CS80_PARAMS", "<CS80_PARAMS><" })
    {
        juce::XmlDocument document { juce::String(input) };
        const auto xml = document.getDocumentElement();
        std::printf("input=%s ; parsed-root=%s ; error=%s\n", input,
            xml ? xml->getTagName().toRawUTF8() : "<null>",
            document.getLastParseError().toRawUTF8());
    }
}
