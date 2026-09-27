#include "ui/ProjectTiles.hpp"
#include "ui/PluginTheme.hpp"
#include "ui/UiFormat.hpp"

namespace
{
namespace theme = stemhub::plugin::theme;
namespace uiformat = stemhub::uiformat;
using Theme = theme::PluginTheme;
}

//==============================================================================
NewProjectTile::NewProjectTile()
{
    setInterceptsMouseClicks(false, true);
    setTitle("New project");
}

void NewProjectTile::setFile(const juce::String& path)
{
    filePath = path;
    repaint();
}

void NewProjectTile::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.setColour(Theme::kPaper);
    g.fillRect(bounds);

    auto content = bounds.reduced(kPadding);
    auto header = content.removeFromTop(18);
    g.setColour(Theme::kAccent);
    g.fillRect(header.removeFromLeft(8).withSizeKeepingCentre(8, 8));
    header.removeFromLeft(8);
    theme::paintMetaText(g, "New project", header, Theme::kInk);

    content.removeFromBottom(16 + 8 + 34); // button + link/meta row, laid out by the grid
    content.removeFromTop(12);

    if (filePath.isNotEmpty())
    {
        const juce::File file(filePath);
        theme::paintMetaText(g, "From", content.removeFromTop(14), Theme::kInkSubtle, juce::Justification::centredLeft, 9.5f);
        content.removeFromTop(2);
        g.setColour(Theme::kInk);
        g.setFont(theme::headingFont(14.5f));
        g.drawFittedText(file.getFileName(), content.removeFromTop(20), juce::Justification::centredLeft, 1, 1.0f);
        g.setColour(Theme::kInkSubtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawFittedText(file.getParentDirectory().getFullPathName(), content.removeFromTop(16),
                         juce::Justification::centredLeft, 1, 1.0f);
    }
    else
    {
        g.setColour(Theme::kInkSubtle);
        g.setFont(theme::bodyFont(12.5f));
        g.drawFittedText("Turn the DAW project you are working on into a StemHub project.",
                         content, juce::Justification::topLeft, 3, 1.0f);

        const auto metaRow = bounds.reduced(kPadding).removeFromBottom(16);
        theme::paintMetaText(g, ".flp  " + theme::middleDot() + "  .als", metaRow, Theme::kInkSubtle,
                             juce::Justification::centredLeft, 9.5f);
    }
}

//==============================================================================
ProjectTile::ProjectTile(ProjectListItem projectToShow, const int indexOnGrid, const bool isSelected)
    : project(std::move(projectToShow)), index(indexOnGrid), selected(isSelected)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
    // A click opens the project right away: no focus ring for it.
    setMouseClickGrabsKeyboardFocus(false);
    setTitle(project.name);
    setDescription(project.description);
}

void ProjectTile::setSelected(const bool isSelected)
{
    selected = isSelected;
    repaint();
}

void ProjectTile::paint(juce::Graphics& g)
{
    const auto hovered = isEnabled() && isMouseOver(true);
    const auto bounds = getLocalBounds();
    const auto foreground = selected ? Theme::kInk : Theme::kForeground;
    const auto subtle = selected ? Theme::kInk.withAlpha(0.66f) : Theme::kForegroundSubtle;

    g.setColour(selected ? Theme::kAccent : (hovered ? Theme::kSurfaceElevated : Theme::kSurface));
    g.fillRect(bounds);
    if (!selected)
    {
        g.setColour(hovered ? Theme::kForeground.withAlpha(0.35f) : Theme::kSurfaceBorder);
        g.drawRect(bounds, 1);
    }

    auto content = bounds.reduced(kPadding);

    auto header = content.removeFromTop(18);
    theme::paintMetaText(g, uiformat::twoDigits(index + 1), header, subtle);
    const auto visibility = juce::String(project.isPublic ? "Public" : "Private");
    const auto tagArea = header.removeFromRight(static_cast<int>(theme::tagWidth(visibility))).toFloat();
    const auto tagColour = selected ? Theme::kInk : (project.isPublic ? Theme::kAccent : Theme::kForegroundSubtle);
    theme::paintTag(g, visibility, tagArea, tagColour.withMultipliedAlpha(project.isPublic || selected ? 1.0f : 0.6f),
                    tagColour, true);

    content.removeFromTop(14);
    theme::paintSequencerArt(g, content.removeFromTop(38).toFloat(), project.id + project.name, 3, 16,
                             selected ? Theme::kInk.withAlpha(0.28f)
                                      : Theme::kForeground.withAlpha(hovered ? 0.36f : 0.2f),
                             selected ? Theme::kInk : Theme::kAccent);

    auto metaRow = content.removeFromBottom(16);
    if (hovered || selected)
    {
        g.setColour(selected ? Theme::kInk : Theme::kAccent);
        g.setFont(theme::headingFont(15.0f));
        g.drawText(theme::arrowRight(), metaRow.removeFromRight(18), juce::Justification::centredRight, false);
        metaRow.removeFromRight(6);
    }

    const auto category = project.category.trim().isNotEmpty() ? project.category.trim() : juce::String("Project");
    const auto categoryWidth = juce::jmin(metaRow.getWidth(),
                                          static_cast<int>(std::ceil(juce::GlyphArrangement::getStringWidth(
                                              theme::labelFont(9.5f), category.toUpperCase()))));
    theme::paintMetaText(g, category, metaRow.removeFromLeft(categoryWidth), subtle, juce::Justification::centredLeft, 9.5f);

    if (project.description.trim().isNotEmpty() && metaRow.getWidth() > 30)
    {
        metaRow.removeFromLeft(4);
        g.setColour(subtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawText(theme::middleDot() + "  " + project.description.trim(), metaRow, juce::Justification::centredLeft, true);
    }

    content.removeFromBottom(8);
    juce::StringArray words;
    words.addTokens(project.name.toUpperCase(), " ", "");
    const auto fontSize = theme::fitDisplayFontSize(words, static_cast<float>(content.getWidth()), 17.0f, 12.0f);
    theme::paintDisplayText(g, project.name.toUpperCase(), content, fontSize, foreground, 2, true);

    if (hasKeyboardFocus(false))
    {
        g.setColour(selected ? Theme::kInk : Theme::kAccent);
        g.drawRect(bounds, 2);
    }
}

// JUCE still delivers mouse events to disabled components.
void ProjectTile::mouseUp(const juce::MouseEvent& event)
{
    if (event.mouseWasClicked())
        open();
}

bool ProjectTile::keyPressed(const juce::KeyPress& key)
{
    if (key != juce::KeyPress::returnKey && key != juce::KeyPress::spaceKey)
        return false;

    open();
    return true;
}

void ProjectTile::enablementChanged()
{
    setMouseCursor(isEnabled() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler> ProjectTile::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(
        *this,
        juce::AccessibilityRole::button,
        juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [this] { open(); }));
}

void ProjectTile::open()
{
    // Copied first: opening may rebuild the grid, and delete this tile with its callback.
    if (auto callback = onOpen; isEnabled() && callback != nullptr)
        callback();
}

//==============================================================================
PlaceholderTile::PlaceholderTile(juce::String titleText, juce::String messageText, juce::String patternSeed)
    : title(std::move(titleText)), message(std::move(messageText)), seed(std::move(patternSeed))
{
    setInterceptsMouseClicks(false, false);
}

void PlaceholderTile::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.setColour(Theme::kSurfaceBorder);
    g.drawRect(bounds, 1);

    auto content = bounds.reduced(kPadding);
    content.removeFromTop(18 + 14);
    theme::paintSequencerArt(g, content.removeFromTop(38).withWidth(juce::jmin(content.getWidth(), 184)).toFloat(),
                             seed, 3, 16, Theme::kForeground.withAlpha(0.08f), Theme::kForeground.withAlpha(0.16f));

    if (title.isEmpty())
        return;

    auto text = content.removeFromBottom(58);
    theme::paintDisplayText(g, title, text.removeFromTop(22), 17.0f, Theme::kForeground, 1);
    text.removeFromTop(6);
    g.setColour(Theme::kForegroundSubtle);
    g.setFont(theme::bodyFont(12.0f));
    g.drawFittedText(message, text, juce::Justification::topLeft, 2, 1.0f);
}
