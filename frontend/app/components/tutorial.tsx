import type { ReactNode } from "react";

// Content of the "How it works" walkthrough. Every screenshot is the real plugin,
// captured during one session: Nova finishing "Midnight Drive" in FL Studio and
// bringing Kai in for the topline. Names, dates and file counts below match what
// the screenshots show, so keep them in sync if a screenshot is replaced.

export type Shot = { src: string; alt: string; caption: string };

export type Substep = { title: string; body: ReactNode; shot: Shot };

export type Step = {
  number: string;
  title: string;
  summary: string;
  intro: string;
  substeps: Substep[];
  story: { who: string; when: string; text: string };
};

const SHOTS = "/landing/how-it-works";

// A plugin control, named exactly as the plugin labels it.
function Ui({ children }: { children: ReactNode }) {
  return <span className="font-bold">{children}</span>;
}

export const steps: Step[] = [
  {
    number: "01",
    title: "Install the plugin",
    summary: "Add the StemHub plugin to FL Studio.",
    intro: "StemHub lives inside your session. Load it once per project and it remembers you.",
    substeps: [
      {
        title: "Load the plugin",
        body: (
          <>
            Install StemHub, rescan in FL Studio (<Ui>Options → Manage plugins → Find plugins</Ui>) and drop{" "}
            <Ui>Stemhub Session</Ui> on any mixer insert.
          </>
        ),
        shot: {
          src: `${SHOTS}/01-plugin-sign-in.webp`,
          alt: "The StemHub plugin's sign-in screen, with empty email and password fields",
          caption: "The plugin opens on its sign-in screen.",
        },
      },
      {
        title: "Sign in",
        body: (
          <>
            Use your StemHub account and hit <Ui>Sign in</Ui>. Next time you open the project, the plugin signs
            you back in on its own.
          </>
        ),
        shot: {
          src: `${SHOTS}/01-sign-in-filled.webp`,
          alt: "The sign-in form filled in with nova@stemhub.app",
          caption: "Nova signs in with nova@stemhub.app.",
        },
      },
      {
        title: "Land on your projects",
        body: "Every project you own or were invited to shows up here. On a new account the grid is empty, and the first tile is how you fill it.",
        shot: {
          src: `${SHOTS}/01-projects-empty.webp`,
          alt: "The projects screen of a new account: a New project tile and the message Nothing here yet",
          caption: "A new account: nothing here yet.",
        },
      },
    ],
    story: {
      who: "Nova",
      when: "Fri 16 Oct, 21:30",
      text: "An 808 loop has been running for an hour. Before touching anything else, Nova drops Stemhub Session on the master insert of Midnight Drive and signs in.",
    },
  },
  {
    number: "02",
    title: "Save versions",
    summary: "Every save gets a message so you find it again.",
    intro: "Turn your project file into a StemHub project once, then save a version every time the track really moves.",
    substeps: [
      {
        title: "Create the project",
        body: (
          <>
            Click <Ui>Choose project file</Ui>, pick your .flp, then <Ui>Create project</Ui>. The project takes the
            file&apos;s name.
          </>
        ),
        shot: {
          src: `${SHOTS}/02-create-project.webp`,
          alt: "The New project tile showing Midnight Drive.flp from /Users/nova/Music/Midnight Drive and a Create project button",
          caption: "Midnight Drive.flp, about to become a project.",
        },
      },
      {
        title: "Say what changed",
        body: (
          <>
            Type a message (optional, up to 500 characters) and hit <Ui>Save version</Ui>, or press Return.
          </>
        ),
        shot: {
          src: `${SHOTS}/02-describe-changes.webp`,
          alt: "The project dashboard with the message First sketch: drums + 808 typed next to the Save version button",
          caption: "V01, one click away.",
        },
      },
      {
        title: "Let it upload",
        body: "StemHub sends the .flp and the samples in its folder (WAV, MP3, FLAC, AIFF, MIDI and more), and skips every file it already has.",
        shot: {
          src: `${SHOTS}/02-uploading.webp`,
          alt: "The Save version button reading Saving, with the status bar showing Uploading 3 of 7 new files",
          caption: "Uploading 3 of 7 new files.",
        },
      },
      {
        title: "Find it again",
        body: (
          <>
            Each save lands in the history with its message, date and size. The version open in your DAW is
            tagged <Ui>In DAW</Ui>.
          </>
        ),
        shot: {
          src: `${SHOTS}/02-history.webp`,
          alt: "The version history of Midnight Drive: six versions from First sketch to Master v1 for the label, with the details of V06",
          caption: "Six versions in five days.",
        },
      },
    ],
    story: {
      who: "Nova",
      when: "Fri 16 Oct, 21:40 → Wed 21 Oct, 07:42",
      text: "“First sketch: drums + 808” becomes V01. All week she saves after every real move: pads, vocal take 3, a mix pass, the arrangement. Wednesday morning, V06: “Master v1 for the label”.",
    },
  },
  {
    number: "03",
    title: "Branch the idea",
    summary: "Try something wild without touching the original. Compare, restore, keep the best.",
    intro: "A branch is a second history for the same project. Experiment there, and bring back whatever works.",
    substeps: [
      {
        title: "Open a branch",
        body: (
          <>
            Create a branch from your project page on stemhub.io, then pick it from the plugin&apos;s{" "}
            <Ui>Branch</Ui> menu.
          </>
        ),
        shot: {
          src: `${SHOTS}/03-branch-menu.webp`,
          alt: "The Branch menu open, listing main, vocal-ideas and radio-edit",
          caption: "main, vocal-ideas and radio-edit.",
        },
      },
      {
        title: "Go wild on it",
        body: "Save versions on the branch exactly like on main. main doesn't move.",
        shot: {
          src: `${SHOTS}/03-branch-history.webp`,
          alt: "The vocal-ideas branch with two versions: Alt hook: pitched vocal chops and Double-time verse flow",
          caption: "vocal-ideas keeps its own history.",
        },
      },
      {
        title: "Bring a version back",
        body: (
          <>
            Select any version, from any branch, and click <Ui>Restore this version</Ui>.
          </>
        ),
        shot: {
          src: `${SHOTS}/03-restore-confirm.webp`,
          alt: "The Restore version dialog explaining that the version is downloaded into a new folder and opens as a separate copy",
          caption: "The dialog tells you where it goes.",
        },
      },
      {
        title: "Open it side by side",
        body: "It downloads into a new folder next to your project and opens in your DAW as a separate copy. The project you have open stays as it is.",
        shot: {
          src: `${SHOTS}/03-restored.webp`,
          alt: "The dashboard after restoring Double-time verse flow, with the status Version restored to /Users/nova/Music/Midnight Drive",
          caption: "Restored next to the original.",
        },
      },
    ],
    story: {
      who: "Nova",
      when: "Sun 18 Oct, 23:50 → Wed 21 Oct, 08:00",
      text: "Take 3 is too good to risk, so the pitched-chop hook goes on a vocal-ideas branch, then a double-time verse on Monday. Wednesday the label answers Master v1: “Can we hear the double-time verse?” Nova restores it from vocal-ideas, bounces it and sends it. main never moved.",
    },
  },
  {
    number: "04",
    title: "Pass it on",
    summary: "Invite collaborators. Everyone works on their own branch.",
    intro: "Collaborators get the same project in their own plugin, with every file they need to open it.",
    substeps: [
      {
        title: "Invite your crew",
        body: "Add collaborators from the project's settings on stemhub.io. Editors can save versions. The project then shows up in their plugin.",
        shot: {
          src: `${SHOTS}/04-shared-project.webp`,
          alt: "Kai's projects screen showing Midnight Drive, shared by Nova, next to his own project Topline Vault",
          caption: "Kai's plugin: Midnight Drive, next to his own work.",
        },
      },
      {
        title: "Open the project",
        body: "With no copy of their own, opening it pulls the latest version into Documents/StemHub and opens it in their DAW.",
        shot: {
          src: `${SHOTS}/04-latest-pulled.webp`,
          alt: "Midnight Drive open in Kai's plugin, with the status Latest version restored to /Users/kai/Documents",
          caption: "Master v1, pulled into Kai's Documents.",
        },
      },
      {
        title: "Work on your own branch",
        body: (
          <>
            Pick a branch and hit <Ui>Save version</Ui>. Only new files go up.
          </>
        ),
        shot: {
          src: `${SHOTS}/04-uploading-one-file.webp`,
          alt: "Kai saving on the kai-topline branch, with the status bar showing Uploading 1 of 1 new files and 8 files in the project",
          caption: "Uploading 1 of 1 new files. The other 7 were already there.",
        },
      },
      {
        title: "Hand it back",
        body: "The version lands on the branch for the whole project to see. You decide what goes into main.",
        shot: {
          src: `${SHOTS}/04-saved-on-branch.webp`,
          alt: "The kai-topline branch with its first version, Topline: chorus melody + harmonies, tagged In DAW",
          caption: "kai-topline, V01.",
        },
      },
    ],
    story: {
      who: "Kai",
      when: "Wed 21 Oct, 08:19",
      text: "Nova invites Kai, a topliner, as an Editor and opens a kai-topline branch for him. Midnight Drive appears in his plugin; he records a chorus melody over Master v1 and saves it. One new file goes up: his vocal.",
    },
  },
];
