# Stemhub Session — JUCE C++ Implementation Spec

**Plugin Dimensions:** 720 x 560 px (fixed, `setSize(720, 560)`)

---

## Design System

### Fonts
- UI text: **Inter** (embed as binary resource)
- Technical/monospace data: **JetBrains Mono** (embed as binary resource)

### Color Palette

| Token | Value | Usage |
|------|------|------|
| bg | #0f0f12 | Main background |
| surface | #18181c | Panels, headers, footers |
| surfaceElevated | #1f1f24 | Elevated cards, active states |
| border | #2a2a30 | Primary borders |
| borderSubtle | #202025 | Section dividers |
| textPrimary | #f5f5f7 | Headings, names |
| textSecondary | #a1a1aa | Labels, secondary info |
| textTertiary | #71717a | Metadata, timestamps, paths |
| accent | #22d3ee | Buttons, highlights |
| accentSubtle | #0e7490 | Accent borders |
| accentGlow | rgba(34, 211, 238, 0.15) | Glow/background tint |
| hover | #252529 | Hover state |
| active | #2f2f35 | Active state |
| success | #10b981 | Synced status |
| successSubtle | #065f46 | Success background |
| warning | #f59e0b | Conflict warnings |
| warningSubtle | #92400e | Warning background |
| error | #ef4444 | Errors |
| errorSubtle | #7f1d1d | Error background |

### UI Rules
- Border radius: 4–8px
- Inputs/buttons: ~6px radius
- Cards: 8px radius
- Icons: 10–18px (Lucide-style vectors)

### Accent Button Pattern
- Background: `accent`
- Text: `#0f0f12`
- Hover: `#06b6d4`
- Glow: `box-shadow: 0 0 20px accentGlow`

---

# VIEW 1: Login View

### Layout
- Centered vertically
- Max width: 340px

### Structure

#### Logo
- 48x48px rounded (8px)
- Accent background
- Hexagon icon (#0f0f12)

#### Title
- "Stemhub Session"
- Inter 24px / 700 / textPrimary

#### Subtitle
- "Sign in to sync your music projects"
- Inter 13px / textTertiary

#### Email Field
- Label: Inter 12px / textSecondary
- Input:
  - Height ~40px
  - Background: surface
  - Border: border
  - Icon: Mail (14px)
  - Placeholder: textTertiary
- Focus:
  - Border: accent
  - Glow: accentGlow

#### Password Field
- Same as email
- Icon: Lock

#### Error Message (conditional)
- Background: errorSubtle
- Border: error
- Icon: AlertCircle
- Text: Inter 12px / error

#### Sign In Button
- Height ~46px
- Background: accent
- Text: #0f0f12
- Icon: ArrowRight
- Hover: #06b6d4

**Loading State**
- Spinner + "Signing in..."
- Opacity 0.7
- Disabled cursor

#### Forgot Password
- Inter 12px
- Underlined
- Hover: textSecondary

#### Continue Offline
- Border button
- Hover: background hover

---

# VIEW 2: Project Selection View

### Layout
- Full frame
- Vertical flex

### Header

#### Title Block
- "Your Projects" (18px / bold)
- Subtitle (12px / textTertiary)

#### New Project Button
- Accent button
- Plus icon

### Search + Filter

#### Search Input
- Background: surfaceElevated
- Icon: Search

#### Filter Toggle
- Options: All / Local / Cloud
- Active: accent
- Inactive: textSecondary

---

### Project List

Each card:
- Background: surface
- Border: border
- Hover:
  - Background: surfaceElevated
  - Border: accent

#### Row 1
- Folder icon + name

#### Row 2 (metadata)
- Clock + time
- Snapshots (JetBrains Mono)
- Collaborators (optional)

#### Row 3
- Storage icon (local/cloud)
- File path (truncated)
- Optional SYNCED badge

---

### Empty State
- Icon (Search 40px)
- Title: "No projects found"
- Subtitle guidance

---

# VIEW 3: Session View

### Layout
- Full frame
- 5 stacked sections

---

## Section 1: Header

- Back button
- Logo + "Stemhub"
- Project name

#### Right Side
- Sync badge
- Collaborators badge
- Settings button

---

## Section 2: Current Snapshot

- Label: "CURRENT SNAPSHOT"
- Name + GitBranch icon
- Changes badge (accent)
- Last saved timestamp

---

## Section 3: Action Bar

### Buttons
- Save Snapshot (primary)
- Sync (secondary)
- Revert (icon-only)

### States
- Disabled: reduced opacity
- Active: glow

### Description Text
- With changes: unsaved warning
- Without: confirmation

---

## Section 4: History List

### Header
- "SESSION HISTORY"

### List Item
- GitCommit icon
- Snapshot name

#### Metadata
- Author
- Timestamp
- Files changed

#### States
- Current:
  - surfaceElevated
  - Left accent border
- Hover:
  - hover background

### Empty State
- "No snapshots yet"

---

## Section 5: Footer

- Local path (HardDrive icon)
- Cloud URL (Network icon)
- Storage usage

---

# VIEW 4: Merging View
Status: next-pass; gated by merge completion.

### Layout
- Full frame
- Vertical flex

---

## Header

- Back button
- GitMerge icon
- Title

### Metadata Row
- Author
- Snapshot name
- Timestamp

---

## Conflict Banner

### Unresolved
- warningSubtle background
- Warning icon
- Conflict count

### Resolved
- successSubtle background
- Check icon

---

## File Changes List

### Header
- "CHANGES (N files)"

### File Row

#### Header
- Expand chevron
- File icon (color-coded)
- File path
- Optional conflict icon

#### Type Badge
- ADDED (green)
- DELETED (red)
- MODIFIED (accent)
- RESOLVED (if applicable)

#### States
- Hover: hover bg
- Selected: surfaceElevated

---

### Conflict Resolution Panel

- Prompt text

#### Options
- "Your Version"
- "Their Version"

##### States
- Unselected:
  - surface bg
- Hover:
  - border highlight
- Selected:
  - accentGlow bg
  - accent border
  - Check icon

---

### Non-conflict Expanded View
- Shows modification timestamp

---

## Footer Actions

### Buttons
- Cancel (secondary)
- Complete Merge (primary)

### State
- Disabled until conflicts resolved

---

# JUCE Implementation Notes

### View Management
- Swap components via `resized()`
- No animations

### Scrolling
- Use `juce::Viewport` for:
  - Project list
  - History list
  - File changes list

### Hover States
- Override:
  - `mouseEnter()`
  - `mouseExit()`
- Call `repaint()`

### Focus Rings
- Draw 3px outline using `accentGlow`

### Button Glow
- Render blurred accent background or rounded rect

### Fonts
- Embed via `BinaryData`
- Load using:
  - `Typeface::createSystemTypefaceFor()`

### Icons
- Use:
  - `juce::Path`
  - or SVG via `Drawable::createFromSVG()`

**Icon Set:**
ArrowLeft, ArrowRight, Mail, Lock, AlertCircle, Folder, Clock, Users, Search, Plus, ChevronRight, ChevronDown, Cloud, CloudOff, HardDrive, Save, Upload, RotateCcw, GitBranch, GitCommit, GitMerge, User, MoreVertical, Settings, Network, FileText, AlertTriangle, Check, X
