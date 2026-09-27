#include <algorithm>

#include "ui/PluginTheme.hpp"
#include "ui/UiFormat.hpp"
#include "ui/VersionTimeline.hpp"

namespace
{
namespace theme = stemhub::plugin::theme;
namespace uiformat = stemhub::uiformat;
using Theme = theme::PluginTheme;
}

//==============================================================================
class VersionTimeline::Row final : public juce::Component
{
public:
    // A placeholder row says the history is empty.
    Row(VersionListItem versionToShow, int numberFromOldest, int rowIndex, bool isLastRow, bool isPlaceholder)
        : version(std::move(versionToShow)), number(numberFromOldest), index(rowIndex), isLast(isLastRow), placeholder(isPlaceholder)
    {
        if (placeholder)
        {
            setTitle("No snapshots yet");
            return;
        }

        setTitle(uiformat::versionTitle(version));
        setDescription("V" + uiformat::twoDigits(number) + ", " + uiformat::timestamp(version.createdAt, false)
                       + (version.isOpenInDaw ? juce::String(", in your DAW") : juce::String()));
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void setSelected(bool isSelected)
    {
        selected = isSelected;
        repaint();
    }

    [[nodiscard]] const juce::String& getVersionId() const noexcept { return version.id; }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        const auto hovered = !placeholder && isMouseOver(true);
        const auto centreY = static_cast<float>(bounds.getCentreY());
        auto block = bounds.withTrimmedLeft(kGutter - 6);

        if (selected)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(block);
        }
        else if (hovered)
        {
            g.setColour(Theme::kSurface);
            g.fillRect(block);
        }

        g.setColour(Theme::kSurfaceBorder);
        g.fillRect(static_cast<float>(kRuleX), 0.0f, 1.0f, isLast ? centreY : static_cast<float>(bounds.getHeight()));

        const juce::Rectangle<float> node { static_cast<float>(kRuleX) + 0.5f - 5.0f, centreY - 5.0f, 10.0f, 10.0f };
        if (placeholder)
        {
            g.setColour(Theme::kBackground);
            g.fillRect(node);
            g.setColour(Theme::kForegroundTertiary);
            g.drawRect(node, 1.0f);
        }
        else if (version.isOpenInDaw)
        {
            g.setColour(Theme::kAccent);
            g.fillRect(node);
        }
        else
        {
            g.setColour(selected ? Theme::kForeground : Theme::kBackground);
            g.fillRect(node);
            g.setColour(Theme::kForeground.withAlpha(selected ? 1.0f : 0.45f));
            g.drawRect(node, 1.0f);
        }

        const auto ink = selected;
        auto area = block.reduced(12, 0);

        if (placeholder)
        {
            auto text = area.withSizeKeepingCentre(area.getWidth(), 36);
            g.setColour(Theme::kForegroundSubtle);
            g.setFont(theme::headingFont(13.5f));
            g.drawText("No snapshots yet", text.removeFromTop(20), juce::Justification::centredLeft, true);
            g.setColour(Theme::kForegroundTertiary);
            g.setFont(theme::bodyFont(11.5f));
            g.drawText("Your first save starts this timeline.", text, juce::Justification::centredLeft, true);
            return;
        }

        const auto relative = uiformat::relativeTime(version.createdAt, juce::Time::getCurrentTime());
        theme::paintMetaText(g, relative, area.removeFromRight(84), ink ? Theme::kInk.withAlpha(0.7f) : Theme::kForegroundSubtle,
                             juce::Justification::centredRight, 9.5f);

        if (version.isOpenInDaw)
        {
            const auto label = juce::String("In DAW");
            const auto width = static_cast<int>(theme::tagWidth(label));
            area.removeFromRight(10);
            const auto tagArea = area.removeFromRight(width).withSizeKeepingCentre(width, 18).toFloat();
            theme::paintTag(g, label, tagArea, ink ? Theme::kInk : Theme::kAccent, ink ? Theme::kInk : Theme::kAccent, true);
        }
        area.removeFromRight(12);

        auto text = area.withSizeKeepingCentre(area.getWidth(), 36);
        g.setColour(ink ? Theme::kInk : Theme::kForeground);
        g.setFont(theme::headingFont(13.5f));
        g.drawText(uiformat::versionTitle(version), text.removeFromTop(20), juce::Justification::centredLeft, true);

        g.setColour(ink ? Theme::kInk.withAlpha(0.7f) : Theme::kForegroundSubtle);
        g.setFont(theme::bodyFont(11.5f));
        g.drawText("V" + uiformat::twoDigits(number) + uiformat::metaSeparator() + uiformat::timestamp(version.createdAt, false),
                   text, juce::Justification::centredLeft, true);

        if (hasKeyboardFocus(false))
        {
            g.setColour(selected ? Theme::kInk : Theme::kAccent);
            g.drawRect(block, 2);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void focusGained(FocusChangeType) override { repaint(); }
    void focusLost(FocusChangeType) override { repaint(); }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked())
            pick();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (placeholder)
            return false;

        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            pick();
            return true;
        }

        if ((key == juce::KeyPress::upKey || key == juce::KeyPress::downKey) && onStep != nullptr)
        {
            onStep(index, key == juce::KeyPress::upKey ? -1 : 1);
            return true;
        }

        return false;
    }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        if (placeholder)
            return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::staticText);

        return std::make_unique<juce::AccessibilityHandler>(
            *this,
            juce::AccessibilityRole::listItem,
            juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [this] { pick(); }));
    }

    std::function<void(const juce::String& versionId)> onPick;
    std::function<void(int index, int stepBy)> onStep;

private:
    void pick()
    {
        // Copied first: picking may rebuild the timeline, and delete this row with its callback.
        if (auto callback = onPick; !placeholder && callback != nullptr)
            callback(version.id);
    }

    VersionListItem version;
    int number { 0 };
    int index { 0 };
    bool isLast { false };
    bool placeholder { false };
    bool selected { false };
};

//==============================================================================
VersionTimeline::VersionTimeline()
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(8);
    setTitle("Version history");

    rebuildRows();
}

VersionTimeline::~VersionTimeline() = default;

void VersionTimeline::setVersions(const std::vector<VersionListItem>& versionItems, const juce::String& selectedVersionId)
{
    const auto isListed = std::any_of(versionItems.begin(), versionItems.end(), [&selectedVersionId](const auto& version)
    {
        return version.id == selectedVersionId;
    });
    const auto nextSelection = isListed ? selectedVersionId : (versionItems.empty() ? juce::String() : versionItems.front().id);

    if (versionItems != versions)
    {
        versions = versionItems;
        selectedId = nextSelection;
        rebuildRows();
    }
    else if (nextSelection != selectedId)
    {
        select(nextSelection, false);
    }
}

void VersionTimeline::resized()
{
    viewport.setBounds(getLocalBounds());
    layoutRows();
}

void VersionTimeline::rebuildRows()
{
    rows.clear(true);

    if (versions.empty())
    {
        auto* row = rows.add(new Row({}, 0, 0, true, true));
        content.addAndMakeVisible(row);
        layoutRows();
        return;
    }

    const auto count = static_cast<int>(versions.size());
    for (int i = 0; i < count; ++i)
    {
        const auto& version = versions[static_cast<size_t>(i)];
        // Newest first: V01 is the first save.
        auto* row = rows.add(new Row(version, count - i, i, i == count - 1, false));
        row->setSelected(version.id == selectedId);
        row->onPick = [this](const juce::String& versionId) { select(versionId, true); };
        row->onStep = [this](int index, int stepBy) { step(index, stepBy); };
        content.addAndMakeVisible(row);
    }

    layoutRows();
}

void VersionTimeline::layoutRows()
{
    const auto listHeight = rows.size() * kRowHeight;
    const auto needsScroll = listHeight > viewport.getHeight();
    const auto rowWidth = viewport.getWidth() - (needsScroll ? 12 : 0);

    int y = 0;
    for (auto* row : rows)
    {
        row->setBounds(0, y, rowWidth, kRowHeight);
        y += kRowHeight;
    }

    content.setSize(rowWidth, y);
}

void VersionTimeline::select(const juce::String& versionId, const bool byUser)
{
    const auto changed = versionId != selectedId;
    selectedId = versionId;

    for (auto* row : rows)
        row->setSelected(row->getVersionId() == selectedId);

    if (byUser && changed && onSelect != nullptr)
        onSelect(selectedId);
}

void VersionTimeline::step(const int index, const int stepBy)
{
    const auto target = juce::jlimit(0, static_cast<int>(versions.size()) - 1, index + stepBy);
    if (target == index)
        return;

    auto* row = rows[target];
    scrollToShow(target);
    row->grabKeyboardFocus();
    select(row->getVersionId(), true);
}

void VersionTimeline::scrollToShow(const int index)
{
    const auto rowTop = index * kRowHeight;
    const auto viewTop = viewport.getViewPositionY();
    const auto viewHeight = viewport.getMaximumVisibleHeight();

    if (rowTop < viewTop)
        viewport.setViewPosition(0, rowTop);
    else if (rowTop + kRowHeight > viewTop + viewHeight)
        viewport.setViewPosition(0, rowTop + kRowHeight - viewHeight);
}
