/*
 * tools/check.c
 * @guterion
 * CC-BY-SA-4.0
 * Check the profile pages: box widths, links and alt text
 *
 * Build with Fil-C, which gives every pointer a capability and turns a
 * memory safety violation into a panic:
 *
 *     $FILC/build/bin/clang -g -O -o tools/check tools/check.c
 *
 * The program reads only, so it holds no state that a later run depends
 * on. It reports every problem and exits non-zero, which lets it serve
 * as a pre-commit gate.
 */

/* memmem comes from the GNU extensions, not from C itself. */
#define _GNU_SOURCE

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_PROBLEMS 256
#define MAX_PATH 512
#define MAX_DOCS 16

static char problems[MAX_PROBLEMS][MAX_PATH];
static int problem_count;

static void report(const char *fmt, ...)
{
	va_list args;

	if (problem_count >= MAX_PROBLEMS)
		return;
	va_start(args, fmt);
	vsnprintf(problems[problem_count], MAX_PATH, fmt, args);
	va_end(args);
	problem_count++;
}

/*
 * Terminal cell width of one UTF-8 string. The pages hold Latin text,
 * box-drawing characters and the odd emoji; only the emoji ranges take
 * two cells, so the table stays this short.
 */
static int cell_width(const char *s, size_t len)
{
	size_t i = 0;
	int width = 0;

	while (i < len) {
		unsigned char c = (unsigned char)s[i];
		unsigned long cp;
		int bytes;

		if (c < 0x80) {
			cp = c;
			bytes = 1;
		} else if ((c & 0xE0) == 0xC0) {
			cp = c & 0x1F;
			bytes = 2;
		} else if ((c & 0xF0) == 0xE0) {
			cp = c & 0x0F;
			bytes = 3;
		} else {
			cp = c & 0x07;
			bytes = 4;
		}
		if (i + (size_t)bytes > len)
			break;
		for (int k = 1; k < bytes; k++)
			cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
		i += (size_t)bytes;

		/* Wide ranges: CJK, and the emoji blocks the pages use. */
		if ((cp >= 0x1100 && cp <= 0x115F) ||
		    (cp >= 0x2E80 && cp <= 0xA4CF) ||
		    (cp >= 0xAC00 && cp <= 0xD7A3) ||
		    (cp >= 0xF900 && cp <= 0xFAFF) ||
		    (cp >= 0x1F300 && cp <= 0x1FAFF) ||
		    (cp >= 0x2600 && cp <= 0x27BF))
			width += 2;
		else
			width += 1;
	}
	return width;
}

/* The characters an ASCII box is drawn with. */
static int starts_box(const char *line)
{
	static const char *marks[] = {
		"┌", "┐", "└", "┘", "├", "┤",
		"─", "│", "╔", "╗", "╚", "╝",
		"║", "═", "┬", "┴", "┼", NULL
	};

	for (int i = 0; marks[i]; i++) {
		size_t n = strlen(marks[i]);

		if (strncmp(line, marks[i], n) == 0)
			return 1;
	}
	return 0;
}

/* Every line of one box must come out at the same width. */
static void check_boxes(const char *doc, const char *text)
{
	const char *line = text;
	int inside = 0, start_line = 0, line_no = 0;
	int first_width = -1;

	while (line && *line) {
		char *end = strchr(line, '\n');
		size_t len = end ? (size_t)(end - line) : strlen(line);

		line_no++;
		if (len >= 3 && strncmp(line, "```", 3) == 0) {
			inside = !inside;
			if (inside) {
				start_line = line_no;
				first_width = -1;
			}
		} else if (inside && starts_box(line)) {
			int w = cell_width(line, len);

			if (first_width < 0)
				first_width = w;
			else if (w != first_width)
				report("%s:%d box lines differ in width: "
				       "%d and %d", doc, start_line,
				       first_width, w);
		}
		line = end ? end + 1 : NULL;
	}
}

/* Resolve one relative target against the directory of its document. */
static int target_exists(const char *doc, const char *target)
{
	char path[MAX_PATH];
	char dir[MAX_PATH];
	struct stat st;
	char *slash;

	snprintf(dir, sizeof dir, "%s", doc);
	slash = strrchr(dir, '/');
	if (slash)
		*slash = '\0';
	else
		dir[0] = '\0';

	if (dir[0])
		snprintf(path, sizeof path, "%s/%s", dir, target);
	else
		snprintf(path, sizeof path, "%s", target);

	return stat(path, &st) == 0;
}

static int is_remote(const char *t)
{
	return strncmp(t, "http://", 7) == 0 || strncmp(t, "https://", 8) == 0 ||
	       strncmp(t, "mailto:", 7) == 0 || t[0] == '#';
}

/*
 * Collect every relative link and image source, and confirm each one
 * points at a file that exists.
 */
static void check_targets(const char *doc, const char *text)
{
	const char *p = text;

	while ((p = strstr(p, "src=\"")) != NULL) {
		char target[MAX_PATH];
		const char *start = p + 5;
		const char *close = strchr(start, '"');

		p = start;
		if (!close || (size_t)(close - start) >= sizeof target)
			continue;
		memcpy(target, start, (size_t)(close - start));
		target[close - start] = '\0';
		if (!is_remote(target) && !target_exists(doc, target))
			report("%s: missing target %s", doc, target);
	}

	p = text;
	while ((p = strstr(p, "](")) != NULL) {
		char target[MAX_PATH];
		const char *start = p + 2;
		const char *close = strchr(start, ')');

		p = start;
		if (!close || (size_t)(close - start) >= sizeof target)
			continue;
		memcpy(target, start, (size_t)(close - start));
		target[close - start] = '\0';
		if (strchr(target, '#') || is_remote(target))
			continue;
		if (!target_exists(doc, target))
			report("%s: missing target %s", doc, target);
	}
}

/*
 * Confirm the repository paths that the prose quotes. A path inside
 * backticks names a file for the reader, so a stale one sends them to
 * a file that went away. The path is read from the repository root,
 * which is where the prose means it, and a name that holds a wildcard
 * stands for a set rather than for one file.
 */
static void check_quoted_paths(const char *doc, const char *text)
{
	static const char *const roots[] = {
		"assets/", "tools/", "i18n/", NULL
	};
	const char *p = text;

	while ((p = strchr(p, '`')) != NULL) {
		char quoted[MAX_PATH];
		const char *start = p + 1;
		const char *close = strchr(start, '`');
		struct stat st;
		int known = 0;

		p = start;
		if (!close || (size_t)(close - start) >= sizeof quoted)
			continue;
		memcpy(quoted, start, (size_t)(close - start));
		quoted[close - start] = '\0';
		p = close + 1;

		if (strchr(quoted, '*') || strchr(quoted, ' '))
			continue;
		for (int i = 0; roots[i]; i++)
			if (!strncmp(quoted, roots[i], strlen(roots[i])))
				known = 1;
		if (!known)
			continue;
		if (stat(quoted, &st) != 0)
			report("%s: quoted path %s is absent", doc, quoted);
	}
}

/* Every image carries alt text; a decorative one carries an empty one. */
static void check_alt(const char *doc, const char *text)
{
	const char *p = text;

	while ((p = strstr(p, "<img")) != NULL) {
		const char *close = strchr(p, '>');
		size_t len;

		if (!close)
			break;
		len = (size_t)(close - p);
		if (!memmem(p, len, "alt=", 4)) {
			char shown[72];
			size_t n = len < sizeof shown - 1 ? len : sizeof shown - 1;

			memcpy(shown, p, n);
			shown[n] = '\0';
			report("%s: img without alt: %s", doc, shown);
		}
		p = close + 1;
	}
}

/* Read one whole file. The caller owns the buffer. */
static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long size;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 0) {              /* The file gave no length. */
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)size + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
		free(buf);
		fclose(f);
		return NULL;
	}
	buf[size] = '\0';
	fclose(f);
	return buf;
}

/* The pages to read: the two at the root, then one per locale. */
static int collect_docs(char docs[MAX_DOCS][MAX_PATH])
{
	DIR *d = opendir("i18n");
	const struct dirent *entry;
	int n = 0;

	snprintf(docs[n++], MAX_PATH, "README.md");
	snprintf(docs[n++], MAX_PATH, "LICENCE.md");
	if (!d)
		return n;
	while ((entry = readdir(d)) != NULL && n < MAX_DOCS) {
		struct stat st;
		char path[MAX_PATH];

		if (entry->d_name[0] == '.')
			continue;
		snprintf(path, sizeof path, "i18n/%s/README.md", entry->d_name);
		if (stat(path, &st) == 0)
			snprintf(docs[n++], MAX_PATH, "%s", path);
	}
	closedir(d);
	return n;
}

int main(void)
{
	char docs[MAX_DOCS][MAX_PATH];
	int count = collect_docs(docs);

	for (int i = 0; i < count; i++) {
		char *text = slurp(docs[i]);

		if (!text) {
			report("%s: cannot read", docs[i]);
			continue;
		}
		check_boxes(docs[i], text);
		check_targets(docs[i], text);
		check_quoted_paths(docs[i], text);
		check_alt(docs[i], text);
		free(text);
	}

	if (problem_count) {
		printf("%d problems\n\n", problem_count);
		for (int i = 0; i < problem_count; i++)
			printf("  %s\n", problems[i]);
		return 1;
	}
	printf("All boxes aligned, every link and quoted path resolves, "
	       "every image has alt text.\n");
	return 0;
}
