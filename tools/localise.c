/*
 * tools/localise.c
 * @guterion
 * CC-BY-SA-4.0
 * Build the three localised pages from README.md
 *
 *     $FILC/build/bin/clang -g -O -o tools/localise tools/localise.c
 *
 * README.md is the source. Each copy adjusts its asset paths and its
 * language selector, then applies its own dictionary, so the four pages
 * never drift apart. Run this after every edit to README.md.
 *
 * A phrase that the dictionary cannot find is reported rather than
 * skipped in silence, which catches a sentence that moved.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *const ES[][2] = {
	{ "Computer Science and Engineering student at the University of Chile (DCC/FCFM).**\n\n*Focused on systems, open source, Unix-like systems, and software engineering.*",
	  "Estudiante de Ingeniería Civil en Computación en la Universidad de Chile (DCC/FCFM).**\n\n*Enfocado en sistemas, código abierto, sistemas tipo Unix e ingeniería de software.*" },
	{ "## About",
	  "## Sobre mí" },
	{ "I'm Franco Gutiérrez. I study Computer Science and Engineering at the\n**University of Chile**, in the Department of Computer Science\n(**DCC**) of the Faculty of Physical and Mathematical Sciences\n(**FCFM**).",
	  "Soy Franco Gutiérrez. Estudio Ingeniería Civil en Computación en la\n**Universidad de Chile**, en el Departamento de Ciencias de la\nComputación (**DCC**) de la Facultad de Ciencias Físicas y Matemáticas\n(**FCFM**)." },
	{ "My main interests are systems, free and open-source software,\nUnix-like operating systems, and software engineering. I tend to work\nfrom the shell, primarily with **Neovim**.",
	  "Mis principales intereses son los sistemas, el software libre y de\ncódigo abierto, los sistemas operativos tipo Unix y la ingeniería de\nsoftware. Suelo trabajar desde la shell, principalmente con **Neovim**." },
	{ "## Currently",
	  "## Actualmente" },
	{ "My studies at the [**University of Chile**](https://uchile.cl/) are my\nprimary focus, alongside personal software projects and my work at\n[**Venturas**](https://venturas.cl/).",
	  "Mis estudios en la [**Universidad de Chile**](https://uchile.cl/) son mi\nfoco principal, junto con proyectos personales de software y mi trabajo\nen [**Venturas**](https://venturas.cl/)." },
	{ "## University\n",
	  "## Universidad\n" },
	{ "<b>Degree</b></td><td>Computer Science and Engineering<",
	  "<b>Carrera</b></td><td>Ingeniería Civil en Computación<" },
	{ "<b>Department</b></td><td>Department of Computer Science (DCC)<",
	  "<b>Departamento</b></td><td>Departamento de Ciencias de la Computación (DCC)<" },
	{ "<b>Faculty</b></td><td>Faculty of Physical and Mathematical Sciences (FCFM)<",
	  "<b>Facultad</b></td><td>Facultad de Ciencias Físicas y Matemáticas (FCFM)<" },
	{ "<b>University</b></td><td>University of Chile<",
	  "<b>Universidad</b></td><td>Universidad de Chile<" },
	{ "## Computing",
	  "## Computación" },
	{ "<b>Systems</b>",
	  "<b>Sistemas</b>" },
	{ "<b>Working with</b>",
	  "<b>Trabajando con</b>" },
	{ "<b>Web &amp; markup</b>",
	  "<b>Web y marcado</b>" },
	{ "## Projects",
	  "## Proyectos" },
	{ "Advanced Gentoo installation documentation for AMD64 musl systems, covering\nOpenRC, LUKS2, Btrfs, LLVM/Clang, ThinLTO, and the Zen kernel.",
	  "Documentación avanzada de instalación de Gentoo para sistemas musl en AMD64,\nque cubre OpenRC, LUKS2, Btrfs, LLVM/Clang, ThinLTO y el kernel Zen." },
	{ "A terminal colour palette on a warm neutral ramp, as Base16 and Base24\nschemes, in seventeen variants.",
	  "Una paleta de color para terminal sobre una rampa neutra cálida, como\nesquemas Base16 y Base24, en diecisiete variantes." },
	{ "alt=\"The alchemical sign of phosphorus\"",
	  "alt=\"El signo alquímico del fósforo\"" },
	{ "[**All public repositories →**]",
	  "[**Todos los repositorios públicos →**]" },
	{ "## Enterprises",
	  "## Empresas" },
	{ "Where I keep my commercial projects.",
	  "Donde mantengo mis proyectos comerciales." },
	{ "## Contact",
	  "## Contacto" },
	{ "badges/email.svg\" height=\"20\" alt=\"Email\"",
	  "badges/email-es.svg\" height=\"20\" alt=\"Correo\"" },
	{ "aria-label=\"View ORCID record",
	  "aria-label=\"Ver registro ORCID" },
	{ "Email is the best way to reach me. I answer as soon as I can.",
	  "El correo es la mejor forma de contactarme. Respondo apenas puedo." },
	{ "<td>Write to me for an address.</td>",
	  "<td>Escríbame para conseguir una dirección.</td>" },
	{ "<summary>You can support my public work through Liberapay or cryptocurrency. <b>Monero preferred.</b></summary>",
	  "<summary>Puede apoyar mi trabajo público a través de Liberapay o criptomonedas. <b>Monero de preferencia.</b></summary>" },
	{ "alt=\"GPG key\"",
	  "alt=\"Clave GPG\"" },
	{ "alt=\"GPG key",
	  "alt=\"Clave GPG" },
	{ "## Support",
	  "## Apoyo" },
	{ "<summary><sub>Stats</sub></summary>",
	  "<summary><sub>Estadísticas</sub></summary>" },
	{ "label=Followers",
	  "label=Seguidores" },
	{ "label=Stars",
	  "label=Estrellas" },
	{ "label=Issues",
	  "label=Incidencias" },
	{ "label=Pull%20requests",
	  "label=Solicitudes" },
	{ "alt=\"Issues opened\"",
	  "alt=\"Incidencias abiertas\"" },
	{ "alt=\"Pull requests opened\"",
	  "alt=\"Solicitudes abiertas\"" },
	{ "alt=\"GitHub followers\"",
	  "alt=\"Seguidores en GitHub\"" },
	{ "alt=\"GitHub stars\"",
	  "alt=\"Estrellas en GitHub\"" },
	{ "alt=\"The shell\"",
	  "alt=\"La shell\"" },
	{ "alt=\"The C language\"",
	  "alt=\"El lenguaje C\"" },
	{ "alt=\"Free software\"",
	  "alt=\"Software libre\"" },
	{ "alt=\"Football\"",
	  "alt=\"Fútbol\"" },
	{ "alt=\"Dragon Ball\"",
	  "alt=\"Dragon Ball\"" },
	{ "label=VISITORS",
	  "label=VISITAS" },
	{ "alt=\"Visitor count\"",
	  "alt=\"Contador de visitas\"" },
	{ "alt=\"Licence: CC BY-SA 4.0 or later\"",
	  "alt=\"Licencia: CC BY-SA 4.0 o posterior\"" },
	{ NULL, NULL }
};

static const char *const LA[][2] = {
	{ "Computer Science and Engineering student at the University of Chile (DCC/FCFM).**\n\n*Focused on systems, open source, Unix-like systems, and software engineering.*",
	  "Discipulus scientiae computatralis in Universitate Chilensi (DCC/FCFM).**\n\n*Studens systematibus, fonti aperto, systematibus generis Unix et arti ingeniariae programmaturae.*" },
	{ "## About",
	  "## De me" },
	{ "I'm Franco Gutiérrez. I study Computer Science and Engineering at the\n**University of Chile**, in the Department of Computer Science\n(**DCC**) of the Faculty of Physical and Mathematical Sciences\n(**FCFM**).",
	  "Franco Gutiérrez sum. Scientiae computatrali in **Universitate\nChilensi** studeo, in Departimento Scientiarum Computatralium (**DCC**)\nFacultatis Scientiarum Physicarum et Mathematicarum (**FCFM**)." },
	{ "My main interests are systems, free and open-source software,\nUnix-like operating systems, and software engineering. I tend to work\nfrom the shell, primarily with **Neovim**.",
	  "Praecipue me tenent systemata, programmatura libera et aperta,\nsystemata operandi generis Unix, et ars ingeniaria programmaturae.\nPlerumque e cortice imperatorio laboro, **Neovim** utens." },
	{ "## Currently",
	  "## Nunc" },
	{ "My studies at the [**University of Chile**](https://uchile.cl/) are my\nprimary focus, alongside personal software projects and my work at\n[**Venturas**](https://venturas.cl/).",
	  "Studia mea in [**Universitate Chilensi**](https://uchile.cl/) praecipua\nmihi cura sunt, simul cum operibus programmaturae propriis et labore meo\napud [**Venturas**](https://venturas.cl/)." },
	{ "## University\n",
	  "## Universitas\n" },
	{ "<b>Degree</b></td><td>Computer Science and Engineering<",
	  "<b>Curriculum</b></td><td>Scientia computatralis et ars ingeniaria<" },
	{ "<b>Department</b></td><td>Department of Computer Science (DCC)<",
	  "<b>Departimentum</b></td><td>Scientiarum computatralium (DCC)<" },
	{ "<b>Faculty</b></td><td>Faculty of Physical and Mathematical Sciences (FCFM)<",
	  "<b>Facultas</b></td><td>Scientiarum physicarum et mathematicarum (FCFM)<" },
	{ "<b>University</b></td><td>University of Chile<",
	  "<b>Universitas</b></td><td>Universitas Chilensis<" },
	{ "## Computing",
	  "## Computatio" },
	{ "<b>Editor</b>",
	  "<b>Scriptorium</b>" },
	{ "<b>Systems</b>",
	  "<b>Systemata</b>" },
	{ "<b>Working with</b>",
	  "<b>In manibus</b>" },
	{ "<b>Web &amp; markup</b>",
	  "<b>Tela et notatio</b>" },
	{ "## Projects",
	  "## Opera" },
	{ "Advanced Gentoo installation documentation for AMD64 musl systems, covering\nOpenRC, LUKS2, Btrfs, LLVM/Clang, ThinLTO, and the Zen kernel.",
	  "Scriptura provecta de institutione Gentoo in systematibus musl et AMD64,\nquae OpenRC, LUKS2, Btrfs, LLVM/Clang, ThinLTO et nucleum Zen complectitur." },
	{ "A terminal colour palette on a warm neutral ramp, as Base16 and Base24\nschemes, in seventeen variants.",
	  "Pales colorum pro terminali super scalam neutram calidam, ut rationes\nBase16 et Base24, in septendecim formis." },
	{ "alt=\"The alchemical sign of phosphorus\"",
	  "alt=\"Signum alchemicum phosphori\"" },
	{ "[**All public repositories →**]",
	  "[**Omnia repositoria publica →**]" },
	{ "## Enterprises",
	  "## Negotia" },
	{ "Where I keep my commercial projects.",
	  "Ubi opera mea mercatoria servo." },
	{ "## Contact",
	  "## Epistulae" },
	{ "badges/email.svg\" height=\"20\" alt=\"Email\"",
	  "badges/email-la.svg\" height=\"20\" alt=\"Epistula\"" },
	{ "badges/gpg.svg\" height=\"20\" alt=\"GPG key\"",
	  "badges/gpg-la.svg\" height=\"20\" alt=\"Clavis GPG\"" },
	{ "aria-label=\"View ORCID record",
	  "aria-label=\"Vide tabulam ORCID" },
	{ "Email is the best way to reach me. I answer as soon as I can.",
	  "Epistula optima via est ad me contingendum. Respondeo cum primum possum." },
	{ "<td>Write to me for an address.</td>",
	  "<td>Scribe mihi ut inscriptionem accipias.</td>" },
	{ "<summary>You can support my public work through Liberapay or cryptocurrency. <b>Monero preferred.</b></summary>",
	  "<summary>Opus meum publicum per Liberapay vel per nummos cryptographicos sustinere potes. <b>Monero praelatum.</b></summary>" },
	{ "alt=\"GPG key",
	  "alt=\"Clavis GPG" },
	{ "## Support",
	  "## Subsidium" },
	{ "<summary><sub>Stats</sub></summary>",
	  "<summary><sub>Statistica</sub></summary>" },
	{ "label=Followers",
	  "label=Sectatores" },
	{ "label=Stars",
	  "label=Stellae" },
	{ "label=Issues",
	  "label=Quaestiones" },
	{ "label=Pull%20requests",
	  "label=Petitiones" },
	{ "alt=\"Issues opened\"",
	  "alt=\"Quaestiones apertae\"" },
	{ "alt=\"Pull requests opened\"",
	  "alt=\"Petitiones apertae\"" },
	{ "alt=\"GitHub followers\"",
	  "alt=\"Sectatores in GitHub\"" },
	{ "alt=\"GitHub stars\"",
	  "alt=\"Stellae in GitHub\"" },
	{ "alt=\"The shell\"",
	  "alt=\"Cortex imperatorius\"" },
	{ "alt=\"The C language\"",
	  "alt=\"Lingua C\"" },
	{ "alt=\"Free software\"",
	  "alt=\"Programmatura libera\"" },
	{ "alt=\"Football\"",
	  "alt=\"Pediludium\"" },
	{ "label=VISITORS",
	  "label=HOSPITES" },
	{ "alt=\"Visitor count\"",
	  "alt=\"Numerus hospitum\"" },
	{ "alt=\"Licence: CC BY-SA 4.0 or later\"",
	  "alt=\"Licentia: CC BY-SA 4.0 vel posterior\"" },
	{ NULL, NULL }
};

static const char *const SELECTOR[3] = {
	"<img src=\"../../assets/flags/spqr.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[Latine](../la/README.md)** · <img src=\"../../assets/flags/burgundy.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[Español](../es/README.md)** · <img src=\"../../assets/flags/england.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **English**",
	"<img src=\"../../assets/flags/spqr.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[Latine](../la/README.md)** · <img src=\"../../assets/flags/burgundy.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **Español** · <img src=\"../../assets/flags/england.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[English](../en/README.md)**",
	"<img src=\"../../assets/flags/spqr.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **Latine** · <img src=\"../../assets/flags/burgundy.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[Español](../es/README.md)** · <img src=\"../../assets/flags/england.svg\" alt=\"\" height=\"18\" align=\"texttop\"> **[English](../en/README.md)**"
};

/*
 * Replace every occurrence of `find` with `repl`, returning a fresh
 * buffer. The caller owns it. Returns the number of replacements
 * through `hits`, so the caller can report a phrase that never matched.
 */
static char *replace_all(const char *text, const char *find,
			 const char *repl, int *hits)
{
	size_t find_len = strlen(find);
	size_t repl_len = strlen(repl);
	size_t cap = strlen(text) + 1;
	size_t len = 0;
	char *out = malloc(cap);
	const char *p = text;

	*hits = 0;
	if (!out || !find_len)
		return NULL;

	while (*p) {
		const char *at = strstr(p, find);
		size_t chunk = at ? (size_t)(at - p) : strlen(p);

		/* Grow before writing: chunk, then the replacement. */
		while (len + chunk + repl_len + 1 > cap) {
			char *bigger;

			cap *= 2;
			bigger = realloc(out, cap);
			if (!bigger) {
				free(out);
				return NULL;
			}
			out = bigger;
		}
		memcpy(out + len, p, chunk);
		len += chunk;
		if (!at)
			break;
		memcpy(out + len, repl, repl_len);
		len += repl_len;
		p = at + find_len;
		(*hits)++;
	}
	out[len] = '\0';
	return out;
}

/* Apply one substitution in place, freeing the previous buffer. */
static char *apply(char *text, const char *find, const char *repl)
{
	int hits = 0;
	char *next = replace_all(text, find, repl, &hits);

	if (!next) {
		fprintf(stderr, "  out of memory\n");
		exit(1);
	}
	free(text);
	return next;
}

static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long size;

	if (!f) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc((size_t)size + 1);
	if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "cannot read %s\n", path);
		exit(1);
	}
	buf[size] = '\0';
	fclose(f);
	return buf;
}

static void spit(const char *path, const char *text)
{
	FILE *f = fopen(path, "wb");

	if (!f) {
		fprintf(stderr, "cannot write %s\n", path);
		exit(1);
	}
	fwrite(text, 1, strlen(text), f);
	fclose(f);
}

/* The selector line in README.md, found by its opening. */
static char *selector_line(const char *text)
{
	const char *start = strstr(text, "<img src=\"assets/flags/spqr.svg\"");
	const char *end;
	char *line;

	if (!start) {
		fprintf(stderr, "selector line not found in README.md\n");
		exit(1);
	}
	end = strchr(start, '\n');
	if (!end)
		end = start + strlen(start);
	line = malloc((size_t)(end - start) + 1);
	memcpy(line, start, (size_t)(end - start));
	line[end - start] = '\0';
	return line;
}

static void build(const char *lang, const char *readme, const char *selector,
		  const char *const table[][2])
{
	char header_from[128], header_to[160], path[128], dir[128];
	const char *name = strcmp(lang, "en") == 0 ? "English"
			 : strcmp(lang, "es") == 0 ? "Spanish" : "Latin";
	char *text = strdup(readme);
	char *sel = selector_line(readme);
	int hits;

	/* Hold the selector aside while the asset paths shift down a level. */
	text = apply(text, sel, "@@SEL@@");
	text = apply(text, "src=\"assets/", "src=\"../../assets/");
	text = apply(text, "href=\"LICENCE.md\"", "href=\"../../LICENCE.md\"");
	text = apply(text, "@@SEL@@", selector);

	snprintf(header_from, sizeof header_from,
		 "README.md\n@guterion\nCC-BY-SA-4.0\nProfile page in English");
	snprintf(header_to, sizeof header_to,
		 "i18n/%s/README.md\n@guterion\nCC-BY-SA-4.0\n"
		 "Profile page in %s", lang, name);
	text = apply(text, header_from, header_to);

	/*
	 * Report the pairs that the page no longer holds before any
	 * replacement runs. A pair checked mid-run can look absent only
	 * because an earlier pair consumed the text it looked for.
	 */
	for (int i = 0; table && table[i][0]; i++)
		if (!strstr(text, table[i][0])) {
			char shown[56];
			size_t n = strlen(table[i][0]) < sizeof shown - 1
				 ? strlen(table[i][0]) : sizeof shown - 1;

			memcpy(shown, table[i][0], n);
			shown[n] = '\0';
			printf("  MISS [%s] \"%s\"\n", lang, shown);
		}

	for (int i = 0; table && table[i][0]; i++)
		text = apply(text, table[i][0], table[i][1]);

	snprintf(dir, sizeof dir, "i18n/%s", lang);
	mkdir("i18n", 0755);
	mkdir(dir, 0755);
	snprintf(path, sizeof path, "i18n/%s/README.md", lang);
	spit(path, text);
	printf("  %s: %zu bytes\n", lang, strlen(text));

	free(text);
	free(sel);
	(void)hits;
}

int main(void)
{
	char *readme = slurp("README.md");

	build("en", readme, SELECTOR[0], NULL);
	build("es", readme, SELECTOR[1], ES);
	build("la", readme, SELECTOR[2], LA);
	free(readme);
	return 0;
}
