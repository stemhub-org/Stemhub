# Issue Templates Guide

This directory contains GitHub issue templates that standardize how we track work in StemHub.

## Available Templates (3 Total)

### 🎯 Feature Request (`feature.yml`)
**Use for:** New features, enhancements, or improvements (any size)

**Required fields:**
- **Objective**: 1-2 sentence summary of the goal
- **User Story**: "As a [role], I want [capability] so that [benefit]"
- **Scope**: What will be included
- **Acceptance Criteria**: Testable conditions for completion
- **Primary Component**: Backend, Frontend, Plugin, etc.

**Best practices:**
- Define clear acceptance criteria with checkboxes
- Include "Out of Scope" to prevent scope creep
- Add dependencies (blocked by / blocks)
- Estimate size (XS/S/M/L/XL) for planning

**Example:** `#169 - First-run UX polish and guided setup`

**Note:** For large features (2+ weeks), create a feature issue and break it down into sub-tasks using checkboxes in the acceptance criteria.

---

### 🐛 Bug Report (`bug.yml`)
**Use for:** Defects, incorrect behavior, errors

**Required fields:**
- **Bug Description**: What went wrong
- **Steps to Reproduce**: Detailed step-by-step instructions
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Affected Component**: Where the bug occurs
- **Severity**: Critical / High / Medium / Low
- **Environment Details**: OS, DAW, browser, version

**Best practices:**
- Include logs, screenshots, or error messages
- Provide exact reproduction steps
- Note any workarounds
- Set severity appropriately (Critical = data loss, security, blocker)

**Example:** `[BUG] Plugin crashes when loading project with 100+ tracks`

---

### 🔧 Technical Debt (`tech-debt.yml`)
**Use for:** Refactoring, code quality, maintenance work

**Required fields:**
- **Technical Problem**: Current state that needs improvement
- **Impact**: Why this matters (problems it causes)
- **Proposed Solution**: How to refactor/improve
- **Acceptance Criteria**: How to know it's complete
- **Affected Component**: Which part of codebase
- **Priority**: High / Medium / Low

**Best practices:**
- Explain the "why" (impact on development, bugs, performance)
- Ensure refactoring has no behavior changes
- Maintain or improve test coverage
- Document risks (regressions, conflicts with feature work)

**Example:** `[TECH DEBT] Split PluginProcessor.cpp into focused files`

---

## Template Selection Guide

| If you want to... | Use this template |
|:------------------|:------------------|
| Add a new feature or improvement (any size) | **Feature Request** |
| Report something broken | **Bug Report** |
| Refactor or improve code quality | **Technical Debt** |

**Note:** For large features that need breakdown, create a Feature Request and use checkboxes in the acceptance criteria to track sub-tasks.

---

## Workflow for Creating Issues

### For feature work:
1. **Check for duplicates** - Search existing issues first
2. **Choose Feature Request template**
3. **Fill required fields** - Especially acceptance criteria
4. **For large features** - Break down into sub-tasks using checkboxes in acceptance criteria
5. **Add labels** - Component (backend/frontend/plugin), priority, size
6. **Assign to milestone** - M1-M7 or Backlog
7. **Link dependencies** - Blocked by / blocks other issues
8. **Assign owner** - Who will work on this (if known)

### For bugs:
1. **Verify reproduction** - Ensure you can reproduce consistently
2. **Search for duplicates** - Check if already reported
3. **Fill bug template** - Include logs, screenshots, steps
4. **Set severity** - Critical, High, Medium, or Low
5. **Add labels** - Component, `bug`, severity label
6. **Triage** - Team will prioritize and assign

---

## Labels Overview

### Component Labels
- `backend` - FastAPI/Python backend
- `frontend` - Next.js/React frontend
- `plugin` - JUCE/C++ DAW plugin
- `parser` - PyFLP parser work
- `storage` - Storage backend

### Type Labels
- `enhancement` - New feature or improvement
- `bug` - Defect or incorrect behavior
- `tech-debt` - Code quality, refactoring
- `documentation` - Docs work
- `security` - Security vulnerability
- `performance` - Performance issue

### Priority Labels (to be added)
- `p0-critical` - Blocker, data loss, security
- `p1-high` - Important feature, major bug
- `p2-medium` - Standard work
- `p3-low` - Nice to have

### Size Labels (to be added)
- `size-XS` - < 2 hours
- `size-S` - 2-8 hours
- `size-M` - 1-3 days
- `size-L` - 1-2 weeks
- `size-XL` - 2+ weeks (requires breakdown)

### Status Labels (to be added)
- `blocked` - Cannot proceed
- `needs-design` - Requires UX/UI design
- `needs-review` - Code complete, awaiting review
- `in-progress` - Currently being worked

### Stage Labels
- `roadmap` - Roadmap tracking item
- `beta` - Beta-specific work
- `greenlight` - Greenlight criteria

---

## Definition of Done

An issue is considered complete when:

- ✅ All acceptance criteria are met
- ✅ Code is reviewed and merged
- ✅ Tests are added and passing
- ✅ Documentation is updated
- ✅ Deployed to relevant environment
- ✅ Issue closed with summary comment

**Example closure comment:**
```markdown
Closed via #345 (PR)

**Implemented:**
- Feature X with Y functionality
- Added tests in `backend/tests/test_x.py`
- Updated docs in `docs/features/x.md`

**Deployed:** 2026-03-15
**Verified:** Beta testing confirms functionality works as expected
```

---

## Tips for Writing Great Issues

### DO ✅
- Write clear, specific titles: `[FEATURE] Add branch switcher to plugin UI`
- Define testable acceptance criteria
- Include context and business value
- Link related issues and dependencies
- Add screenshots, mockups, or examples
- Estimate size for planning
- Update issue as work progresses

### DON'T ❌
- Write vague titles: `Improve UI` or `Fix bug`
- Leave acceptance criteria empty
- Create duplicates (search first!)
- Mix multiple unrelated features in one issue
- Let issues become stale (close or update)
- Skip the template (helps everyone understand context)

---

## Questions?

- **Discord:** [Join #development channel](https://discord.gg/stemhub)
- **Documentation:** [Read the full docs](https://docs.stemhub.io)
- **Maintainers:** Tag @stemhub-org/maintainers for guidance

---

## Template Improvements

These templates are living documents. If you have suggestions for improvements:
1. Open a discussion in the repo
2. Propose changes via PR
3. Discuss with maintainers

Last updated: 2026-03-17
