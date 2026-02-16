# Personas & Use Cases Documentation - StemHub

This documentation details the target user profiles and the concrete problems StemHub solves through various use case scenarios.

## 1. Tom Beats: The Music Producer
**Profile**: Talented young music producer (22 years old, Paris).

### Bio
- **Activity**: Regularly collaborates with various artists.
- **Tools**: Primarily uses FL Studio and Ableton.
- **Goal**: To get noticed by major artists.
- **Pain Point**: Struggles significantly with file management (multiplication of "Save As" versions).

### Use Case: Branch-based Version Management
**The Challenge**: A client asks for a version "that's faster with a different bass" while keeping the original. Tom creates a `Project_V2_Fast.flp` file. Later, he fixes the piano mixing in V2 but has to reopen V1 to manually redo the same settings to maintain consistency.

**The StemHub Solution**: Tom uses the Branching system.
1. He creates a `feature-fast-tempo` branch.
2. He makes his changes on this branch.
3. When he fixes the piano, he does so on the `main` branch.
4. He Merges the fix into the `feature-fast-tempo` branch.

**Result**: Both versions coexist. The piano is fixed everywhere instantly. The client accesses both versions via a single web link without multiple exports.

---

## 2. Frank Bass: The Professional Bassist
**Profile**: Renowned Bassist (40 years old, Marseille).

### Bio
- **Activity**: Records for large scale projects remotely.
- **Tools**: Records his instruments via Ableton.
- **Goal**: Expand professional network.
- **Pain Point**: His regular changes get lost in a forest of `.zip` files; he forgets his old bass lines.

### Use Case: Recovering Lost Tracks
**User Story**: As Frank Bass, I want to retrieve my old bass lines lost between `.zip` files so I don't waste time in my disorganized archives.

**User Journey**:
1. Frank realizes he lost a bass line recorded a week ago.
2. He opens the StemHub app and checks the project's visual history.
3. He selects "Bassline Groove V2" from 7 days ago and clicks "Restore this track".
4. Ableton updates automatically with the old track without affecting the rest of the current project.

**Priority (MoSCoW)**: **Must Have (M)**. This is a vital feature for the MVP to solve file chaos.

---

## 3. Mabé: The Bedroom Beatmaker
**Profile**: Bedroom beatmaker (23 years old, Chicago).

### Bio
- **Activity**: Produces to relax after class, collaborates via Discord.
- **Constraints**: Limited storage space (laptop disk often full).
- **Pain Point**: Fear of losing 3 years of projects if his disk crashes. Often accidentally overwrites good ideas while testing new effects. Finds WeTransfer too heavy for simple "jams".

### Use Case: Specific Track Restoration
**User Story**: As Mabé, I want to browse my old versions to recover a bass line deleted by mistake.

**User Journey**:
1. Mabé regrets a bass deleted two days ago.
2. He checks the visual history in the application.
3. He selects "Bassline V1" and clicks "Restore".
4. The project synchronizes and restores the track instantly.

**Priority (MoSCoW)**: **Must Have (M)**. Vital to avoid data loss and storage-related stress.

---

## 4. Laura: The Sound Designer
**Profile**: Sound Designer (29 years old, Paris).

### Bio
- **Problem**: Works on complex projects with thousands of files.
- **Need**: To know exactly what changed between two versions (e.g., track volume).
- **Motivation**: Control and security. She hates opening a heavy project just to verify a modification.

### Use Case: Contribution Workflow (Collaboration)
**The Challenge**: Laura (in Brussels) needs to lay down vocals on a production coming from London (15 tracks to send back in raw WAV).

**The Classic Problem**: Having to export 15 tracks one by one, verify the start point (0:00), create a 2GB ZIP, and wait 1 hour for it to upload to WeTransfer. Confusion guaranteed at the slightest phrase modification.

**The StemHub Solution**:
1. Laura records her vocals normally in her DAW.
2. She clicks "Push". StemHub intelligently detects that only new audio tracks have been added.
3. The system sends only this data, compressed without loss.

**Result**: The producer in London receives a notification. He clicks "Pull". Laura's vocals appear instantly in his session, perfectly aligned on the grid. No manual export required.
