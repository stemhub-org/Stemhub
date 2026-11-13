#include "application/PluginState.hpp"

namespace stemhub::pluginstate
{
namespace
{
constexpr auto kTagName = "StemhubState";
constexpr int kSchema = 1;
}

juce::MemoryBlock encode(const ProjectLink& link)
{
    juce::XmlElement xml(kTagName);
    xml.setAttribute("schema", kSchema);
    xml.setAttribute("projectId", link.projectId);
    xml.setAttribute("branchId", link.branchId);
    xml.setAttribute("workingFile", link.workingFile.getFullPathName());

    const auto text = xml.toString(juce::XmlElement::TextFormat().singleLine().withoutHeader());
    return { text.toRawUTF8(), text.getNumBytesAsUTF8() };
}

ProjectLink decode(const void* data, const size_t sizeInBytes)
{
    if (data == nullptr || sizeInBytes == 0)
        return {};

    const auto xml = juce::parseXML(juce::String::fromUTF8(static_cast<const char*>(data), static_cast<int>(sizeInBytes)));
    if (xml == nullptr || !xml->hasTagName(kTagName))
        return {};

    ProjectLink link;
    link.projectId = xml->getStringAttribute("projectId").trim();
    link.branchId = xml->getStringAttribute("branchId").trim();

    // Saved on another machine, a path may not even be absolute here.
    const auto workingFilePath = xml->getStringAttribute("workingFile").trim();
    if (juce::File::isAbsolutePath(workingFilePath))
        link.workingFile = juce::File(workingFilePath);

    return link;
}
}
