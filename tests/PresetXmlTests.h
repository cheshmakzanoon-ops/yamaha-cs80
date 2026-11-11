#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetXml.h"
#include <string>
#include <vector>
#if JUCE_LINUX || JUCE_MAC
 #include <sys/stat.h>
#endif

namespace cs80::tests
{
template <typename Check>
void testPresetXml(Check check)
{
    juce::TemporaryFile fixture("-cs80-xml-admission");
    const auto root = fixture.getFile();
    check(root.createDirectory().wasOk(), "XML admission fixture directory created");
    const auto file = root.getChildFile("admission.cs80xml");
    Cs80AudioProcessor host(root.getChildFile("presets"));
    host.loadFactoryPreset(legacyProgramCount);
    host.setPresetName("Live sound");
    host.setParamValue(P_brilliance, 0.8125f);
    const auto before = host.getAPVTS().copyState();
    const auto metadata = host.getPresetMetadata();
    const auto reject = [&](const std::string& bytes, const char* name)
    {
        check(file.replaceWithData(bytes.data(), bytes.size()), "write invalid XML byte fixture");
        const auto result = host.loadUserPreset(file);
        const auto now = host.getPresetMetadata();
        check(result.failed() && result.getErrorMessage() == "The preset file could not be read as valid XML."
                  && host.getAPVTS().copyState().isEquivalentTo(before)
                  && now.name == metadata.name && now.factoryIndex == metadata.factoryIndex
                  && now.origin == metadata.origin, name);
    };
    const std::string complete = "<CS80_PARAMS/>";
    reject(complete + std::string(1, '\0') + "<CS80_PARAMS/>", "embedded NUL cannot hide trailing document bytes");
    reject(std::string("<CS80_PARAMS presetName='") + char(0xc3) + "'/>", "invalid UTF-8 is rejected before JUCE fallback");
    reject(std::string("<?xml version='1.0' encoding='windows-1252'?><CS80_PARAMS presetName='")
        + char(0xe9) + "'/>", "non-UTF byte encodings are not silently reinterpreted");
    reject("<CS80_PARAMS xmlns:p='urn:test' p:a='1' p:a='2'/>", "duplicate namespaced attributes rejected");
    reject("<CS80_PARAMS unbound:a='1'/>", "unbound XML namespace prefix rejected");
    reject("<CS80_PARAMS><1INVALID/></CS80_PARAMS>", "invalid XML name rejected");
    reject("<CS80_PARAMS><!-- double--dash --></CS80_PARAMS>", "malformed XML comment rejected");
    reject("<!DOCTYPE CS80_PARAMS [<!ENTITY x 'local'>]><CS80_PARAMS presetName='&x;'/>",
           "custom internal entities rejected before expansion");
    reject("<!DOCTYPE CS80_PARAMS [<!ENTITY % x '<!ENTITY y \"value\">'>%x;]><CS80_PARAMS/>",
           "parameter entities rejected before expansion");
    reject("<!DOCTYPE CS80_PARAMS [<!ENTITY x '&x;'>]><CS80_PARAMS/>",
           "recursive unused entity declaration rejected");
    // A recognizable local sentinel verifies both resource routes fail before
    // committing metadata or sound. It is not a filesystem-access trace.
    const auto sentinel = root.getChildFile("cs80-xml-xxe-sentinel.txt");
    check(sentinel.replaceWithText("<!ENTITY leak 'CS80_XML_PRIVATE_SENTINEL'>"), "create isolated XXE sentinel");
    std::printf("XXE sentinel path: %s\n", sentinel.getFullPathName().toRawUTF8());
    const auto path = sentinel.getFullPathName().toStdString();
    reject("<!DOCTYPE CS80_PARAMS SYSTEM '" + path + "'><CS80_PARAMS presetName='&leak;'/>",
           "external DTD cannot supply preset metadata from local sentinel");
    reject("<!DOCTYPE CS80_PARAMS [<!ENTITY leak SYSTEM '" + path + "'>]><CS80_PARAMS>&leak;</CS80_PARAMS>",
           "external general entity cannot consume local sentinel");
    reject("<!DOCTYPE CS80_PARAMS SYSTEM 'https://example.invalid/preset.dtd'><CS80_PARAMS/>",
           "external network DTD rejected without resolution");
   #if JUCE_LINUX || JUCE_MAC
    // CTest's enclosing timeout makes an accidental blocking read-open fail.
    // This only witnesses these resource paths; it is not a syscall trace.
    const auto fifo = root.getChildFile("blocking-xml-entity.fifo");
    const auto fifoPath = fifo.getFullPathName().toStdString();
    const bool madeFifo = ::mkfifo(fifoPath.c_str(), 0600) == 0;
    check(madeFifo, "create unique owned FIFO sentinel without a writer");
    if (madeFifo)
    {
        reject("<!DOCTYPE CS80_PARAMS SYSTEM '" + fifoPath + "'><CS80_PARAMS/>",
               "external DTD rejected without a blocking FIFO read-open");
        reject("<!DOCTYPE CS80_PARAMS [<!ENTITY x SYSTEM '" + fifoPath
                   + "'>]><CS80_PARAMS>&x;</CS80_PARAMS>",
               "external entity rejected without a blocking FIFO read-open");
        check(fifo.deleteFile(), "remove owned FIFO after behavioral check");
    }
   #endif
    reject(complete + std::string(presetXml::maxBytes, ' '), "file byte bound rejects without changing live sound");
    std::string deep = "<CS80_PARAMS>";
    for (int i = 0; i < 256; ++i) deep += "<NODE>";
    for (int i = 0; i < 256; ++i) deep += "</NODE>";
    reject(deep + "</CS80_PARAMS>", "default XML depth bound prevents deep JUCE recursion");
    std::string wide = "<CS80_PARAMS>";
    for (int i = 0; i < presetXml::maxElements; ++i) wide += "<NODE/>";
    reject(wide + "</CS80_PARAMS>", "element count bound rejects excessive tree width");

    const juce::String value = juce::String::fromUTF8("<CS80_PARAMS presetName=\"UTF é 🎹\">")
        + "<PARAM id=\"" + juce::String(paramId(P_brilliance)) + "\" value=\"0.375\"/></CS80_PARAMS>";
    const auto utf16 = [&](bool bigEndian, bool declaration = false, const char* encoding = "UTF-16")
    {
        std::string bytes;
        bytes += bigEndian ? char(0xfe) : char(0xff);
        bytes += bigEndian ? char(0xff) : char(0xfe);
        const auto text = (declaration ? juce::String("<?xml version='1.0' encoding='") + encoding + "'?>" : juce::String()) + value;
        auto chars = text.toUTF16();
        for (auto* c = chars.getAddress(); *c != 0; ++c)
        {
            bytes += char(bigEndian ? (*c >> 8) : (*c & 0xff));
            bytes += char(bigEndian ? (*c & 0xff) : (*c >> 8));
        }
        return bytes;
    };
    reject(utf16(false) + "x", "odd trailing UTF-16 byte rejected");
    auto unpaired = utf16(false);
    unpaired.insert(2, std::string("\x00\xd8", 2));
    reject(unpaired, "unpaired UTF-16 surrogate rejected");
    auto zero16 = utf16(false);
    zero16 += std::string("\0\0", 2);
    reject(zero16, "trailing UTF-16 NUL rejected");

    reject("<?xml version='1.0' encoding='UTF-16'?>" + complete,
           "UTF-8 bytes with conflicting UTF-16 declaration rejected");
    reject("<?xml version='1.0' encoding='ISO-8859-1'?>" + complete,
           "non-UTF declaration rejected even for ASCII bytes");
    reject(utf16(true, true, "UTF-8"), "UTF-16 BOM with conflicting UTF-8 declaration rejected");
    reject(utf16(false, true, "UTF-16BE"), "UTF-16 declaration with conflicting byte order rejected");
    const std::string utf8 = value.toStdString();
    const std::vector<std::string> valid {
        utf8, std::string("\xef\xbb\xbf") + utf8, utf16(false), utf16(true), utf16(false, true), utf16(true, true),
        utf16(false, true, "UTF-16LE"), utf16(true, true, "UTF-16BE"),
        "<?xml version='1.0' encoding='UTF-8'?>" + utf8,
        value.replace("presetName=", "xmlns:p='urn:future' p:extra='kept' presetName=")
             .replace("</CS80_PARAMS>", "<p:FUTURE_DATA/></CS80_PARAMS>").toStdString(),
        "<!DOCTYPE CS80_PARAMS [<!ELEMENT CS80_PARAMS ANY>"
            "<!ATTLIST CS80_PARAMS future CDATA 'ignored-default'>]>" + utf8,
    };
    int validCase = 0;
    for (const auto& bytes : valid)
    {
        check(file.replaceWithData(bytes.data(), bytes.size()), "write valid XML encoding fixture");
        const auto result = host.loadUserPreset(file);
        std::printf("XML valid case %d: admitted=%d brilliance=%g name=%s error=%s\n", validCase++,
                    (int) result.wasOk(), host.getParamValue(P_brilliance), host.getPresetName().toRawUTF8(),
                    result.getErrorMessage().toRawUTF8());
        check(result.wasOk() && host.getParamValue(P_brilliance) == 0.375f
                  && host.getPresetName() == juce::String::fromUTF8("UTF é 🎹")
                  && ! host.getAPVTS().copyState().hasProperty("future"),
              "UTF-8, BOM UTF-16 and nonentity DTD retain JUCE recall semantics");
    }
    const auto decorated = "<CS80_PARAMS presetName='a &lt; b &gt; c &quot;quoted&quot; &apos;single&apos;'>"
        "<![CDATA[<literal/>]]><!-- <!DOCTYPE fake> --><?test > ok?></CS80_PARAMS>";
    check(file.replaceWithText(decorated) && host.loadUserPreset(file).wasOk()
              && host.getPresetName() == "a < b > c \"quoted\" 'single'",
          "built-in entities, CDATA, quoted angles, comments and PI remain valid");
}
} // namespace cs80::tests
