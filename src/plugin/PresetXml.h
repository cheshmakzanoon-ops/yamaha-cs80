#pragma once

#include <juce_core/juce_core.h>
#include <libxml/parser.h>
#include <libxml/SAX2.h>

#include <memory>

namespace cs80::presetXml
{
// Limits apply to user-file admission, never session state or the audio path.
// A saved patch is tens of KiB. These are resource bounds, not a time guarantee.
inline constexpr int maxBytes = 16 * 1024 * 1024;
inline constexpr int maxElements = 65536;

inline bool isCompleteDocument(const void* data, int size)
{
    if (size <= 0 || size > maxBytes || data == nullptr)
        return false;
    std::unique_ptr<xmlParserCtxt, decltype(&xmlFreeParserCtxt)> parser(
        xmlNewParserCtxt(), xmlFreeParserCtxt);
    if (! parser)
        return false;
    struct Limits { int elements = 0; bool rejected = false; } limits;
    xmlCtxtSetPrivate(parser.get(), &limits);
    auto sax = *xmlCtxtGetSaxHandler(parser.get());
    sax.internalSubset = [](void* context, const xmlChar* name,
                            const xmlChar* externalId, const xmlChar* systemId)
    {
        auto* ctxt = static_cast<xmlParserCtxt*>(context);
        if (externalId != nullptr || systemId != nullptr)
        {
            static_cast<Limits*>(xmlCtxtGetPrivate(ctxt))->rejected = true;
            xmlStopParser(ctxt);
            return;
        }
        xmlSAX2InternalSubset(context, name, externalId, systemId);
    };
    // Stop at the declaration, before any custom entity can be expanded. JUCE
    // will read the same bytes after validation and must not expand DTD entities.
    sax.entityDecl = [](void* context, const xmlChar*, int, const xmlChar*,
                        const xmlChar*, xmlChar*)
    {
        auto* ctxt = static_cast<xmlParserCtxt*>(context);
        static_cast<Limits*>(xmlCtxtGetPrivate(ctxt))->rejected = true;
        xmlStopParser(ctxt);
    };
    sax.startElementNs = [](void* context, const xmlChar* name, const xmlChar* prefix,
                           const xmlChar* uri, int numNamespaces, const xmlChar** namespaces,
                           int numAttributes, int numDefaults, const xmlChar** attributes)
    {
        auto* ctxt = static_cast<xmlParserCtxt*>(context);
        auto& state = *static_cast<Limits*>(xmlCtxtGetPrivate(ctxt));
        if (++state.elements > maxElements)
        {
            state.rejected = true;
            xmlStopParser(ctxt);
            return;
        }
        xmlSAX2StartElementNs(context, name, prefix, uri, numNamespaces, namespaces,
                             numAttributes, numDefaults, attributes);
    };
    if (xmlCtxtSetSaxHandler(parser.get(), &sax) != 0)
        return false;
    xmlCtxtSetErrorHandler(parser.get(), [](void*, const xmlError*) {}, nullptr);
    xmlCtxtSetResourceLoader(parser.get(),
        [](void*, const char*, const char*, xmlResourceType, xmlParserInputFlags,
           xmlParserInput** out)
        {
            *out = nullptr;
            return XML_IO_LOAD_ERROR;
        }, nullptr);
    // Match the encoding we will give JUCE, rather than let the XML declaration
    // select another decoder. Unlike JUCE's fallback, invalid UTF-8 is rejected.
    const auto* bytes = static_cast<const char*>(data);
    const bool bigEndian = size >= 2 && juce::CharPointer_UTF16::isByteOrderMarkBigEndian(bytes);
    const bool utf16 = bigEndian
        || (size >= 2 && juce::CharPointer_UTF16::isByteOrderMarkLittleEndian(bytes));
    // An explicit generic UTF-16 decoder in libxml2 defaults to little endian;
    // select the byte order ourselves from the same BOM JUCE will consume.
    const auto* encoding = utf16 ? (bigEndian ? "UTF-16BE" : "UTF-16LE") : "UTF-8";
    constexpr int options = XML_PARSE_NONET | XML_PARSE_NO_XXE;
    std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> document(
        xmlCtxtReadMemory(parser.get(), bytes, size, nullptr, encoding, options), xmlFreeDoc);
    // No RECOVER, HUGE, NOENT, DTDLOAD, DTDATTR or DTDVALID. libxml2's default
    // depth (256), name and text limits remain enabled. No process-global hooks.
    if (document == nullptr || limits.rejected || xmlCtxtGetStatus(parser.get()) != 0)
        return false;
    const auto* declared = xmlCtxtGetDeclaredEncoding(parser.get());
    if (declared == nullptr)
        return true;
    const juce::String declaration(reinterpret_cast<const char*>(declared));
    if (! utf16)
        return declaration.equalsIgnoreCase("UTF-8");
    return declaration.equalsIgnoreCase("UTF-16")
        || declaration.equalsIgnoreCase(bigEndian ? "UTF-16BE" : "UTF-16LE");
}

inline std::unique_ptr<juce::XmlElement> readCompleteDocument(const juce::File& file)
{
    juce::FileInputStream input(file);
    if (! input.openedOk())
        return {};
    juce::MemoryBlock bytes;
    const auto read = input.readIntoMemoryBlock(bytes, maxBytes + 1);
    if (input.getStatus().failed() || read <= 0 || read > maxBytes
        || ! isCompleteDocument(bytes.getData(), static_cast<int>(read)))
        return {};
    // One bounded file read: no validation/recall TOCTOU, no FileInputSource
    // through which JUCE could resolve an external DTD or local-file entity.
    juce::XmlDocument document(juce::String::createStringFromData(bytes.getData(), static_cast<int>(read)));
    auto xml = document.getDocumentElement();
    return document.getLastParseError().isEmpty() ? std::move(xml) : nullptr;
}
} // namespace cs80::presetXml
