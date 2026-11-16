// Password strength from zxcvbn, which recognizes common passwords, names,
// dictionary words, keyboard patterns, dates and l33t speak. Its dictionaries
// weigh about 1.5 MB, so they are only loaded by the forms that need them.

export type StrengthScore = 0 | 1 | 2 | 3 | 4;

export interface PasswordStrength {
    score: StrengthScore;
    warning: string | null;
    suggestions: string[];
}

export type PasswordStrengthChecker = (password: string, userInputs?: string[]) => PasswordStrength;

let checker: Promise<PasswordStrengthChecker> | null = null;

export function loadPasswordStrength(): Promise<PasswordStrengthChecker> {
    checker ??= (async () => {
        const [{ ZxcvbnFactory }, common, en] = await Promise.all([
            import("@zxcvbn-ts/core"),
            import("@zxcvbn-ts/language-common"),
            import("@zxcvbn-ts/language-en"),
        ]);
        const zxcvbn = new ZxcvbnFactory({
            dictionary: { ...common.dictionary, ...en.dictionary },
            graphs: common.adjacencyGraphs,
            translations: en.translations,
        });
        return (password, userInputs = []) => {
            const result = zxcvbn.check(password, userInputs.filter(Boolean));
            return {
                score: result.score as StrengthScore,
                warning: result.feedback.warning || null,
                suggestions: result.feedback.suggestions,
            };
        };
    })();
    // A failed download (offline) can be retried by the next caller.
    checker.catch(() => {
        checker = null;
    });
    return checker;
}
