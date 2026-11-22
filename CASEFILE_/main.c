#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CASES 20
#define MAX_SUSPECTS 100
#define MAX_CLUES 150
#define MAX_DIALOGUES 150
#define LINE 512

struct Case
{
    int id, culprit;
    char title[90], description[200];
};
struct Suspect 
{
    int id, caseId;
    char name[80], alibi[180];
};
struct Clue
{
    int id, caseId;
    char location[80], description[200];
};
struct Dialogue
{
    int id, caseId, suspectId, requiredClue;
    char question[180], answer[250];
};

struct Case cases[MAX_CASES];
struct Suspect suspects[MAX_SUSPECTS];
struct Clue clues[MAX_CLUES];
struct Dialogue dialogues[MAX_DIALOGUES];
int nc, ns, nk, nd;
char player[60];
int activeCase, discovered[MAX_CLUES], asked[MAX_DIALOGUES];
int steps;

static void trim(char *s)
{
    int i = 0;
    while (s[i] != '\0')
    {
        if (s[i] == '\n' || s[i] == '\r')
        {
            s[i] = '\0';
            break;
        }
        i++;
    }
}
static int input_int(const char *prompt)
{
    char buf[80];
    int i, sign, value, valid;
    for (;;)
    {
        printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin))
            exit(0);
        trim(buf);
        i = 0;
        while (buf[i] == ' ' || buf[i] == '\t') i++;
        sign = 1;
        if (buf[i] == '-')
        { 
            sign = -1; i++; 
        }
        else if (buf[i] == '+') i++;
        value = 0;
        valid = 0;
        while (buf[i] >= '0' && buf[i] <= '9')
        {
            valid = 1;
            value = value * 10 + (buf[i] - '0');
            if (value > 100000) { valid = 0; break; }
            i++;
        }
        while (buf[i] == ' ' || buf[i] == '\t') i++;
        if (valid && buf[i] == '\0')
            return sign * value;
        puts("Please enter a valid number between -100000 and 100000.");
    }
}
static void input_text(const char *prompt, char *out, size_t n)
{
    for (;;)
    {
        printf("%s", prompt);
        if (!fgets(out, (int)n, stdin))
            exit(0);
        trim(out);
        if (out[0] && !strchr(out, ','))
            return;
        puts("Enter nonempty text without commas.");
    }
}
static void load_data(void)
{
    FILE *f;
    char line[LINE];
    f = fopen("cases.csv", "r");
    if (!f)
    {
        perror("cases.csv");
        exit(1);
    }
    fgets(line, sizeof(line), f);
    while (nc < MAX_CASES && fgets(line, sizeof(line), f))
    {
        struct Case c;
        if (sscanf(line, "%d,%89[^,],%d,%199[^\r\n]", &c.id, c.title, &c.culprit, c.description) == 4)
            cases[nc++] = c;
    }
    fclose(f);
    f = fopen("suspects.csv", "r");
    if (!f)
    {
        perror("suspects.csv");
        exit(1);
    }
    fgets(line, sizeof(line), f);
    while (ns < MAX_SUSPECTS && fgets(line, sizeof(line), f))
    {
        struct Suspect s;
        if (sscanf(line, "%d,%d,%79[^,],%179[^\r\n]", &s.id, &s.caseId, s.name, s.alibi) == 4)
            suspects[ns++] = s;
    }
    fclose(f);
    f = fopen("clues.csv", "r");
    if (!f)
    {
        perror("clues.csv");
        exit(1);
    }
    fgets(line, sizeof(line), f);
    while (nk < MAX_CLUES && fgets(line, sizeof(line), f))
    {
        struct Clue k;
        if (sscanf(line, "%d,%d,%79[^,],%199[^\r\n]", &k.id, &k.caseId, k.location, k.description) == 4)
            clues[nk++] = k;
    }
    fclose(f);
    f = fopen("dialogues.csv", "r");
    if (!f)
    {
        perror("dialogues.csv");
        exit(1);
    }
    fgets(line, sizeof(line), f);
    while (nd < MAX_DIALOGUES && fgets(line, sizeof(line), f))
    {
        struct Dialogue d;
        if (sscanf(line, "%d,%d,%d,%d,%179[^,],%249[^\r\n]", &d.id, &d.caseId, &d.suspectId, &d.requiredClue, d.question, d.answer) == 6)
            dialogues[nd++] = d;
    }
    fclose(f);
    if (!nc)
    {
        puts("No cases loaded.");
        exit(1);
    }
}
struct Case *get_case(int id)
{
    for (int i = 0; i < nc; i++)
        if (cases[i].id == id)
            return &cases[i];
    return NULL;
}
struct Suspect *get_suspect(int id)
{
    for (int i = 0; i < ns; i++)
        if (suspects[i].id == id)
            return &suspects[i];
    return NULL;
}
int has_clue(int id)
{
    for (int i = 0; i < nk; i++)
        if (clues[i].id == id && clues[i].caseId == activeCase)
            return discovered[i];
    return 0;
}
void ensure_header(const char *path, const char *header)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return;
    }
    f = fopen(path, "w");
    if (f)
    {
        fputs(header, f);
        fclose(f);
    }
}
void append_history(const char *event)
{
    FILE *f = fopen("investigation_history.csv", "a");
    if (f)
    {
        fprintf(f, "%s,%d,%d,%s\n", player, activeCase, ++steps, event);
        fclose(f);
    }
}
void save_progress(void)
{
    /* Rewrite only this player's current case; retain other saved cases. */
    FILE *in = fopen("progress.csv", "r"), *out = fopen("progress.tmp", "w");
    char line[LINE], who[60];
    int cid, oldsteps, found, seen;
    if (!out)
    {
        puts("Cannot save progress.");
        if (in)
            fclose(in);
        return;
    }
    fputs("Player,CaseID,Steps,Clues,Dialogues\n", out);
    if (in)
    {
        fgets(line, sizeof(line), in);
        while (fgets(line, sizeof(line), in))
        {
            if (sscanf(line, "%59[^,],%d,%d", who, &cid, &oldsteps) == 3 && strcmp(who, player) == 0 && cid == activeCase)
                continue;
            fputs(line, out);
        }
        fclose(in);
    }
    fprintf(out, "%s,%d,%d,", player, activeCase, steps);
    found = 0;
    for (int i = 0; i < nk; i++)
        if (clues[i].caseId == activeCase && discovered[i])
        {
            fprintf(out, "%s%d", found ? "|" : "", clues[i].id);
            found++;
        }
    fputc(',', out);
    seen = 0;
    for (int i = 0; i < nd; i++)
        if (dialogues[i].caseId == activeCase && asked[i])
        {
            fprintf(out, "%s%d", seen ? "|" : "", dialogues[i].id);
            seen++;
        }
    fputc('\n', out);
    fclose(out);
    /* On Windows, rename cannot replace an existing destination. */
    if (rename("progress.tmp", "progress.csv") != 0)
    {
        if (remove("progress.csv") != 0 ||
            rename("progress.tmp", "progress.csv") != 0)
            puts("Could not finalize save. Check progress.tmp.");
    }
}
/* Read clue or dialogue IDs separated by | without strtok(). */
void restore_ids(const char *list, int isClue)
{
    int i = 0, id, j;
    while (list[i] != '\0' && list[i] != '\n' && list[i] != '\r')
    {
        id = 0;
        if (list[i] < '0' || list[i] > '9') { i++; continue; }
        while (list[i] >= '0' && list[i] <= '9')
        {
            id = id * 10 + list[i] - '0';
            i++;
        }
        if (isClue)
        {
            for (j = 0; j < nk; j++)
                if (clues[j].id == id && clues[j].caseId == activeCase)
                    discovered[j] = 1;
        }
        else
        {
            for (j = 0; j < nd; j++)
                if (dialogues[j].id == id && dialogues[j].caseId == activeCase)
                    asked[j] = 1;
        }
    }
}
void load_progress(void)
{
    FILE *f = fopen("progress.csv", "r");
    char line[LINE], who[60], *first, *second, *third, *fourth;
    int cid, savedSteps, i;
    for (i = 0; i < MAX_CLUES; i++) discovered[i] = 0;
    for (i = 0; i < MAX_DIALOGUES; i++) asked[i] = 0;
    steps = 0;
    if (!f) return;
    fgets(line, sizeof(line), f); /* Skip CSV header. */
    while (fgets(line, sizeof(line), f))
    {
        first = strchr(line, ',');
        if (!first) continue;
        *first = '\0';
        strcpy(who, line);
        second = strchr(first + 1, ',');
        if (!second) continue;
        *second = '\0';
        third = strchr(second + 1, ',');
        if (!third) continue;
        *third = '\0';
        fourth = strchr(third + 1, ',');
        if (!fourth) continue;
        *fourth = '\0';
        cid = atoi(first + 1);
        savedSteps = atoi(second + 1);
        if (strcmp(who, player) != 0 || cid != activeCase) continue;
        steps = savedSteps;
        restore_ids(third + 1, 1);
        restore_ids(fourth + 1, 0);
        break;
    }
    fclose(f);
}
void examine_scene(void)
{
    char locations[30][80];
    int count = 0;
    for (int i = 0; i < nk; i++)
        if (clues[i].caseId == activeCase)
        {
            int exists = 0;
            for (int j = 0; j < count; j++)
                if (strcmp(locations[j], clues[i].location) == 0)
                    exists = 1;
            if (!exists && count < 30)
                strcpy(locations[count++], clues[i].location);
        }
    if (!count)
    {
        puts("No scenes found.");
        return;
    }
    puts("\nLocations:");
    for (int i = 0; i < count; i++)
        printf("%d. %s\n", i + 1, locations[i]);
    int choice = input_int("Visit location (0 to cancel): ");
    if (choice < 1 || choice > count)
        return;
    printf("\nYou search the %s.\n", locations[choice - 1]);
    for (int i = 0; i < nk; i++)
        if (clues[i].caseId == activeCase && strcmp(clues[i].location, locations[choice - 1]) == 0)
        {
            printf("You notice: %s\n", clues[i].description);
            if (!discovered[i])
            {
                int take = input_int("Collect this evidence? (1=yes, 0=no): ");
                if (take == 1)
                {
                    discovered[i] = 1;
                    puts("Evidence collected!");
                    FILE *f = fopen("evidence.csv", "a");
                    if (f)
                    {
                        fprintf(f, "%s,%d,%d\n", player, activeCase, clues[i].id);
                        fclose(f);
                    }
                    append_history("Collected evidence");
                    save_progress();
                }
            }
            else
                puts("Already collected.");
        }
}
void interrogate(void)
{
    int ids[30], count = 0;
    puts("\nSuspects:");
    for (int i = 0; i < ns; i++)
        if (suspects[i].caseId == activeCase && count < 30)
        {
            ids[count] = suspects[i].id;
            printf("%d. %s\n", ++count, suspects[i].name);
        }
    int choice = input_int("Choose suspect (0 to cancel): ");
    if (choice < 1 || choice > count)
        return;
    int id = ids[choice - 1];
    struct Suspect *s = get_suspect(id);
    printf("\n%s says their alibi is: %s\n", s->name, s->alibi);
    int available[50], n = 0;
    for (int i = 0; i < nd; i++)
        if (dialogues[i].caseId == activeCase && dialogues[i].suspectId == id && n < 50)
        {
            if (dialogues[i].requiredClue == 0 || has_clue(dialogues[i].requiredClue))
            {
                available[n] = i;
                printf("%d. %s%s\n", ++n, dialogues[i].question, asked[i] ? " [asked]" : "");
            }
        }
    if (!n)
    {
        puts("No questions unlocked yet. Search for more clues.");
        return;
    }
    choice = input_int("Ask question (0 to cancel): ");
    if (choice < 1 || choice > n)
        return;
    int idx = available[choice - 1];
    printf("%s: %s\n", s->name, dialogues[idx].answer);
    asked[idx] = 1;
    append_history("Interrogated suspect");
    save_progress();
}
int clue_count(void)
{
    int n = 0;
    for (int i = 0; i < nk; i++)
        if (clues[i].caseId == activeCase && discovered[i])
            n++;
    return n;
}
void review_evidence(void)
{
    printf("\n--- Evidence collected (%d) ---\n", clue_count());
    for (int i = 0; i < nk; i++)
        if (clues[i].caseId == activeCase && discovered[i])
            printf("[%d] %s: %s\n", clues[i].id, clues[i].location, clues[i].description);
}
void accuse(void)
{
    struct Case *c = get_case(activeCase);
    int ids[30], count = 0;
    puts("\nWho do you accuse?");
    for (int i = 0; i < ns; i++)
        if (suspects[i].caseId == activeCase && count < 30)
        {
            ids[count] = suspects[i].id;
            printf("%d. %s\n", ++count, suspects[i].name);
        }
    int choice = input_int("Choose suspect (0 to cancel): ");
    if (choice < 1 || choice > count)
        return;
    int confirm = input_int("Submit final accusation? (1=yes, 0=no): ");
    if (confirm != 1)
        return;
    int accused = ids[choice - 1], correct = (accused == c->culprit), found = clue_count();
    int score = (correct ? 60 : 0) + found * 10;
    if (score > 100)
        score = 100;
    printf("\n========== CASE RESULT ==========\nCase: %s\nAccused: %s\n", c->title, get_suspect(accused)->name);
    printf("Verdict: %s\nCorrect culprit: %s\nEvidence found: %d\nScore: %d/100\n", correct ? "CORRECT" : "INCORRECT", get_suspect(c->culprit) ? get_suspect(c->culprit)->name : "Unknown", found, score);
    FILE *f = fopen("results.csv", "a");
    if (f)
    {
        fprintf(f, "%s,%d,%d,%s,%d\n", player, activeCase, accused, correct ? "Yes" : "No", score);
        fclose(f);
    }
    char filename[120];
    sprintf(filename, "report_%d_%s.txt", activeCase, player);
    f = fopen(filename, "w");
    if (f)
    {
        fprintf(f, "CASEFILE INVESTIGATION REPORT\nPlayer: %s\nCase: %d - %s\nAccused: %s\nCorrect culprit: %s\nVerdict: %s\nEvidence collected: %d\nScore: %d/100\n\nEVIDENCE:\n", player, activeCase, c->title, get_suspect(accused)->name, get_suspect(c->culprit) ? get_suspect(c->culprit)->name : "Unknown", correct ? "Correct" : "Incorrect", found, score);
        for (int i = 0; i < nk; i++)
            if (clues[i].caseId == activeCase && discovered[i])
                fprintf(f, "- %s: %s\n", clues[i].location, clues[i].description);
        fclose(f);
        printf("Report saved: %s\n", filename);
    }
    append_history("Submitted accusation");
    save_progress();
}
void investigate(void)
{
    for (;;)
    {
        struct Case *c = get_case(activeCase);
        printf("\n===== CASE %d: %s =====\n%s\n", c->id, c->title, c->description);
        puts("1. Search a location\n2. Interrogate a suspect\n3. Review collected evidence\n4. Accuse a suspect\n5. Save and return to main menu");
        int choice = input_int("Choice: ");
        switch (choice)
        {
        case 1:
            examine_scene();
            break;
        case 2:
            interrogate();
            break;
        case 3:
            review_evidence();
            break;
        case 4:
            accuse();
            return;
        case 5:
            save_progress();
            puts("Progress saved.");
            return;
        default:
            puts("Invalid option.");
        }
    }
}
void show_results(void)
{
    FILE *f = fopen("results.csv", "r");
    char line[LINE], who[60], status[20];
    int cid, sid, score, found = 0;
    if (!f)
    {
        puts("No results yet.");
        return;
    }
    fgets(line, sizeof(line), f);
    puts("\n--- Your case results ---");
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "%59[^,],%d,%d,%19[^,],%d", who, &cid, &sid, status, &score) == 5 && strcmp(who, player) == 0)
        {
            printf("Case %d | Accused %d | %s | Score %d\n", cid, sid, status, score);
            found = 1;
        }
    if (!found)
        puts("No completed cases yet.");
    fclose(f);
}
int main(void)
{
    load_data();
    ensure_header("evidence.csv", "Player,CaseID,ClueID\n");
    ensure_header("results.csv", "Player,CaseID,AccusedID,Correct,Score\n");
    ensure_header("investigation_history.csv", "Player,CaseID,Step,Action\n");
    puts("========================================\n CASEFILE: DETECTIVE INVESTIGATION GAME\n========================================");
    input_text("Detective name (no commas): ", player, sizeof(player));
    for (;;)
    {
        puts("\n1. Start / Continue a case\n2. View completed cases\n3. Exit");
        int ch = input_int("Choice: ");
        if (ch == 3)
        {
            puts("Goodbye, Detective!");
            break;
        }
        if (ch == 2)
        {
            show_results();
            continue;
        }
        if (ch != 1)
        {
            puts("Invalid option.");
            continue;
        }
        puts("\nAvailable cases:");
        for (int i = 0; i < nc; i++)
            printf("%d - %s\n", cases[i].id, cases[i].title);
        int id = input_int("Enter case ID (0 to cancel): ");
        if (!get_case(id))
        {
            puts("Unknown case.");
            continue;
        }
        activeCase = id;
        load_progress();
        printf("Loaded case %d. Previously collected clues: %d\n", activeCase, clue_count());
        investigate();
    }
    return 0;
}
