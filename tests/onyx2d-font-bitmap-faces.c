// SPDX-License-Identifier: GPL-3.0-only
// Host test for tests/onyx2d-font-bitmap-faces.sh: runs the real face-selection loop of
// Onyx2D/O2Font_freetype.m against fontconfig font sets that rank a bitmap face first.
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>

#include SELECT_INC

static int failures;

static FcPattern *face(const char *file, int scalable) /* scalable < 0: property absent */
{
    FcPattern *pat = FcPatternCreate();
    if (file != NULL)
        FcPatternAddString(pat, FC_FILE, (const FcChar8 *) file);
    if (scalable >= 0)
        FcPatternAddBool(pat, FC_SCALABLE, scalable ? FcTrue : FcFalse);
    return pat;
}

static void check(const char *name, const char *const *files, const int *scalable, int count, const char *expected)
{
    FcFontSet *set = FcFontSetCreate();
    for (int i = 0; i < count; i++)
        FcFontSetAdd(set, face(files[i], scalable[i]));
    const char *got = filenameOfFirstScalableFace(set);
    if ((got == NULL) != (expected == NULL) || (got != NULL && strcmp(got, expected) != 0)) {
        printf("FAIL: %s: got %s, expected %s\n", name, got ? got : "(null)", expected ? expected : "(null)");
        failures++;
    }
    FcFontSetDestroy(set);
}

int main(void)
{
    {
        const char *f[] = {"fixed.pcf.gz", "dejavu.ttf", "other.otf"};
        int s[] = {0, 1, 1};
        check("bitmap face ranked first", f, s, 3, "dejavu.ttf");
    }
    {
        const char *f[] = {"a.pcf.gz", "b.pcf.gz", "c.ttf"};
        int s[] = {0, 0, 1};
        check("two bitmap faces first", f, s, 3, "c.ttf");
    }
    {
        const char *f[] = {"a.ttf", "b.pcf.gz"};
        int s[] = {1, 0};
        check("scalable face already first", f, s, 2, "a.ttf");
    }
    {
        const char *f[] = {"a.pcf.gz", "b.pcf.gz"};
        int s[] = {0, 0};
        check("only bitmap faces: first one", f, s, 2, "a.pcf.gz");
    }
    {
        const char *f[] = {"a.pcf.gz", "b.ttf"};
        int s[] = {0, -1};
        check("missing FC_SCALABLE counts as scalable", f, s, 2, "b.ttf");
    }
    {
        const char *f[] = {NULL, "b.ttf"};
        int s[] = {1, 1};
        check("face without FC_FILE skipped", f, s, 2, "b.ttf");
    }
    check("empty set", NULL, NULL, 0, NULL);
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
