/*  greml.c - see http://www.caressa.it/greml for more information. */

/*
    greml - A simple markup processor to produce html ancient greek texts.

    Copyright 2015 by Paolo Caressa <http://www.caressa.it>.
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0"
#define PROGNAME "Greml"
#define AUTHOR "Paolo Caressa <http://www.caressa.it>"
#define DEBUG 1

/*  Standard idiom to compute the size of an array. */
#define SIZE(a) (sizeof(a)/sizeof((a)[0]))

/* Global variables. */

char g_escape = '$';    /* Escape character. */
FILE *g_in = NULL;      /* Current input file. */
FILE *g_out = NULL;     /* Current output file. */
char *g_name = NULL;    /* Name of the current input file. */
int g_line = 0;         /* Line currently under parsing. */
int g_col = 0;          /* Index of the currently scanned character in the
                            current line. */
int g_dq_opened = 0;    /* Flag true if a left double quote has been emitted. */
int g_acute = 0;        /* Flag set when '/' is parsed. */
int g_circumflex = 0;   /* Flag set when '^' is parsed. */
int g_diaeresis = 0;    /* Flag set when '=' is parsed. */
int g_grave = 0;        /* Flag set when '\\' is parsed. */
int g_iota = 0;         /* Flag set when '|' is parsed. */
int g_rough = 0;        /* Flag set when '<' is parsed. */
int g_smooth = 0;       /* Flag set when '>' is parsed. */

/*  Error function: if cond is 1 then prints a message (same syntax as printf)
    and abort execution. If cond is 0 then does nothing. */
void error_on(int cond, char *fmt, ...)
{
    if ( cond ) {
        va_list al;
        va_start(al, fmt);
        fprintf(stderr, "%s:%i:%i ", g_name, g_line, g_col);
        vfprintf(stderr, fmt, al);
        va_end(al);
        exit(EXIT_FAILURE);
    }
}

/*  Wraps fgetc so to adjust line and column pointer. */
int fget_char(void)
{
    int c = fgetc(g_in);
    ++ g_col;
    if ( c == '\n' ) {
        ++ g_line;
        g_col = 1;
    }
    return c;
}

/* Routines used to implement actions triggered by certain characters. */

void do_acute(void) { g_acute = 1; }
void do_circumflex(void) { g_circumflex = 1; }
void do_diaeresis(void) { g_diaeresis = 1; }
void do_grave(void) { g_grave = 1; }
void do_iota(void) { g_iota = 1; }
void do_rough(void) { g_rough = 1; }
void do_smooth(void) { g_smooth = 1; }

void do_closed_brace(void)
{
    /* It may be either a single '}' or a double '}}'. */
    int c = fget_char();
    if ( c != '}' ) {
        /* Nope: it is a single '}'. */
        fputc('}', g_out);
        ungetc(c, g_in);
    } else {
        /* Emits an acute closing parenthesis. */
        fputs("&#x232A;", g_out);
    }
}

void do_closed_bracket(void)
{
    /* It may be either a single ']' or a double ']]'. */
    int c = fget_char();
    if ( c != ']' ) {
        /* Nope: it is a single ']'. */
        fputc(']', g_out);
        ungetc(c, g_in);
    } else {
        /* Emits a double ']'. */
        fputs("&#x301B;", g_out);
    }
}

void do_dquote(void)
{
    fprintf(g_out, g_dq_opened ? "&rdquo;" : "&ldquo;");
    g_dq_opened = !g_dq_opened;
}

void do_hyphen(void)
{
    /* It may be either a single '-' or a double '--'. */
    int c = fget_char();
    if ( c != '-' ) {
        /* Nope: it is a single '-'. */
        fputc('-', g_out);
        ungetc(c, g_in);
    } else {
        /* Emits long hyphen. */
        fputs("&ndash;", g_out);
    }
}

void do_opened_brace(void)
{
    /* It may be either a single '{' or a double '{{'. */
    int c = fget_char();
    if ( c != '{' ) {
        /* Nope: it is a single '{'. */
        fputc('{', g_out);
        ungetc(c, g_in);
    } else {
        /* Emits an acute opening parenthesis. */
        fputs("&#x2329;", g_out);
    }
}

void do_opened_bracket(void)
{
    /* It may be either a single '[' or a double '[['. */
    int c = fget_char();
    if ( c != '[' ) {
        /* Nope: it is a single '['. */
        fputc('[', g_out);
        ungetc(c, g_in);
    } else {
        /* Emits a double '['. */
        fputs("&#x301A;", g_out);
    }
}

void do_sigma(void)
{
    /* Takes special care about ending sigma: if the next character
        is not a letter nor a diacritic or bracket symbol (which
        may appear inside words) then the final form of the sigma
        is printed on the out file. */
    int c = fget_char();
    fprintf(g_out, !isalpha(c) && strchr("<>\\/^=|[]{}", c) == NULL
        ? "&sigmaf;" : "&sigma;");
    ungetc(c, g_in);
}

/*  Table of "character-actions": keys are characters that, when parsed, cause
    the execution of the corresponding function. */
const struct { int c; void (*fun)(void); } CHR_ACTION[] = {
    {'"', do_dquote},
    {'-', do_hyphen},
    {'/', do_acute},
    {'<', do_rough},
    {'=', do_diaeresis},
    {'>', do_smooth},
    {'[', do_opened_bracket},
    {'\\', do_grave},
    {']', do_closed_bracket},
    {'^', do_circumflex},
    {'s', do_sigma},
    {'{', do_opened_brace},
    {'|', do_iota},
    {'}', do_closed_brace},
};

/* Elements of a set of diacritics. */
#define SMOOTH      (1)
#define ROUGH       (2)
#define ACUTE       (4)
#define GRAVE       (8)
#define CIRCUMFLEX  (16)
#define DIAERESIS   (32)
#define IOTA        (64)

/*  Table of letters with diacritic signs: each entry correspond to a unicode
    code, and it is identified via the combination of diacritics (obtained by
    mixing flags) and the corresponding letter. */
const struct { int c; int diacritic; char *value; } DIACRITICS[] = {
    {'a', SMOOTH, "&#x1f00;"},
    {'a', SMOOTH|GRAVE, "&#x1f02;"},
    {'a', SMOOTH|ACUTE, "&#x1f04;"},
    {'a', SMOOTH|CIRCUMFLEX, "&#x1f06;"},
    {'a', ROUGH, "&#x1f01;"},
    {'a', ROUGH|GRAVE, "&#x1f03;"},
    {'a', ROUGH|ACUTE, "&#x1f05;"},
    {'a', ROUGH|CIRCUMFLEX, "&#x1f07;"},
    {'a', IOTA|GRAVE, "&#x1fb2;"},
    {'a', IOTA, "&#x1fb3;"},
    {'a', IOTA|ACUTE, "&#x1fb4;"},
    {'a', CIRCUMFLEX, "&#x1fb6;"},
    {'a', IOTA|CIRCUMFLEX, "&#x1fb7;"},
    {'a', IOTA|SMOOTH, "&#x1f80;"},
    {'a', IOTA|SMOOTH|GRAVE, "&#x1f82;"},
    {'a', IOTA|SMOOTH|ACUTE, "&#x1f84;"},
    {'a', IOTA|SMOOTH|CIRCUMFLEX, "&#x1f86;"},
    {'a', IOTA|ROUGH, "&#x1f81;"},
    {'a', IOTA|ROUGH|GRAVE, "&#x1f83;"},
    {'a', IOTA|ROUGH|ACUTE, "&#x1f85;"},
    {'a', IOTA|ROUGH|CIRCUMFLEX, "&#x1f87;"},
    {'A', SMOOTH, "&#x1f08;"},
    {'A', SMOOTH|GRAVE, "&#x1f0a;"},
    {'A', SMOOTH|ACUTE, "&#x1f0c;"},
    {'A', SMOOTH|CIRCUMFLEX, "&#x1f0e;"},
    {'A', ROUGH, "&#x1f09;"},
    {'A', ROUGH|GRAVE, "&#x1f0b;"},
    {'A', ROUGH|ACUTE, "&#x1f0d;"},
    {'A', ROUGH|CIRCUMFLEX, "&#x1f0f;"},
    {'A', IOTA|SMOOTH, "&#x1f88;"},
    {'A', IOTA|SMOOTH|GRAVE, "&#x1f8a;"},
    {'A', IOTA|SMOOTH|ACUTE, "&#x1f8c;"},
    {'A', IOTA|SMOOTH|CIRCUMFLEX, "&#x1f8e;"},
    {'A', IOTA|ROUGH, "&#x1f89;"},
    {'A', IOTA|ROUGH|GRAVE, "&#x1f8b;"},
    {'A', IOTA|ROUGH|ACUTE, "&#x1f8d;"},
    {'A', IOTA|ROUGH|CIRCUMFLEX, "&#x1f8f;"},
    {'A', GRAVE, "&#x1fba;"},
    {'A', ACUTE, "&#x1fbb;"},
    {'A', IOTA, "&#x1fbc;"},
    {'e', SMOOTH, "&#x1f10;"},
    {'e', SMOOTH|GRAVE, "&#x1f12;"},
    {'e', SMOOTH|ACUTE, "&#x1f14;"},
    {'e', ROUGH, "&#x1f11;"},
    {'e', ROUGH|GRAVE, "&#x1f13;"},
    {'e', ROUGH|ACUTE, "&#x1f15;"},
    {'E', SMOOTH, "&#x1f18;"},
    {'E', SMOOTH|GRAVE, "&#x1f1a;"},
    {'E', SMOOTH|ACUTE, "&#x1f1c;"},
    {'E', ROUGH, "&#x1f19;"},
    {'E', ROUGH|GRAVE, "&#x1f1b;"},
    {'E', ROUGH|ACUTE, "&#x1f1d;"},
    {'h', SMOOTH, "&#x1f20;"},
    {'h', SMOOTH|GRAVE, "&#x1f22;"},
    {'h', SMOOTH|ACUTE, "&#x1f24;"},
    {'h', SMOOTH|CIRCUMFLEX, "&#x1f26;"},
    {'h', ROUGH, "&#x1f21;"},
    {'h', ROUGH|GRAVE, "&#x1f23;"},
    {'h', ROUGH|ACUTE, "&#x1f25;"},
    {'h', ROUGH|CIRCUMFLEX, "&#x1f27;"},
    {'H', SMOOTH, "&#x1f28;"},
    {'H', SMOOTH|GRAVE, "&#x1f2a;"},
    {'H', SMOOTH|ACUTE, "&#x1f2c;"},
    {'H', SMOOTH|CIRCUMFLEX, "&#x1f2e;"},
    {'H', ROUGH, "&#x1f29;"},
    {'H', ROUGH|GRAVE, "&#x1f2b;"},
    {'H', ROUGH|ACUTE, "&#x1f2d;"},
    {'H', ROUGH|CIRCUMFLEX, "&#x1f2f;"},
    {'i', SMOOTH, "&#x1f30;"},
    {'i', SMOOTH|GRAVE, "&#x1f32;"},
    {'i', SMOOTH|ACUTE, "&#x1f34;"},
    {'i', SMOOTH|CIRCUMFLEX, "&#x1f36;"},
    {'i', ROUGH, "&#x1f31;"},
    {'i', ROUGH|GRAVE, "&#x1f33;"},
    {'i', ROUGH|ACUTE, "&#x1f35;"},
    {'i', ROUGH|CIRCUMFLEX, "&#x1f37;"},
    {'I', SMOOTH, "&#x1f38;"},
    {'I', SMOOTH|GRAVE, "&#x1f3a;"},
    {'I', SMOOTH|ACUTE, "&#x1f3c;"},
    {'I', SMOOTH|CIRCUMFLEX, "&#x1f3e;"},
    {'I', ROUGH, "&#x1f39;"},
    {'I', ROUGH|GRAVE, "&#x1f3b;"},
    {'I', ROUGH|ACUTE, "&#x1f3d;"},
    {'I', ROUGH|CIRCUMFLEX, "&#x1f3f;"},
    {'o', SMOOTH, "&#x1f40;"},
    {'o', SMOOTH|GRAVE, "&#x1f42;"},
    {'o', SMOOTH|ACUTE, "&#x1f44;"},
    {'o', ROUGH, "&#x1f41;"},
    {'o', ROUGH|GRAVE, "&#x1f43;"},
    {'o', ROUGH|ACUTE, "&#x1f45;"},
    {'O', SMOOTH, "&#x1f48;"},
    {'O', SMOOTH|GRAVE, "&#x1f4a;"},
    {'O', SMOOTH|ACUTE, "&#x1f4c;"},
    {'O', ROUGH, "&#x1f49;"},
    {'O', ROUGH|GRAVE, "&#x1f4b;"},
    {'O', ROUGH|ACUTE, "&#x1f4d;"},
    {'u', SMOOTH, "&#x1f50;"},
    {'u', SMOOTH|GRAVE, "&#x1f52;"},
    {'u', SMOOTH|ACUTE, "&#x1f54;"},
    {'u', SMOOTH|CIRCUMFLEX, "&#x1f56;"},
    {'u', ROUGH, "&#x1f51;"},
    {'u', ROUGH|GRAVE, "&#x1f53;"},
    {'u', ROUGH|ACUTE, "&#x1f55;"},
    {'u', ROUGH|CIRCUMFLEX, "&#x1f57;"},
    {'U', ROUGH, "&#x1f59;"},
    {'U', ROUGH|GRAVE, "&#x1f5b;"},
    {'U', ROUGH|ACUTE, "&#x1f5d;"},
    {'U', ROUGH|CIRCUMFLEX, "&#x1f5f;"},
    {'w', SMOOTH, "&#x1f60;"},
    {'w', SMOOTH|GRAVE, "&#x1f62;"},
    {'w', SMOOTH|ACUTE, "&#x1f64;"},
    {'w', SMOOTH|CIRCUMFLEX, "&#x1f66;"},
    {'w', ROUGH, "&#x1f61;"},
    {'w', ROUGH|GRAVE, "&#x1f63;"},
    {'w', ROUGH|ACUTE, "&#x1f65;"},
    {'w', ROUGH|CIRCUMFLEX, "&#x1f67;"},
    {'W', SMOOTH, "&#x1f68;"},
    {'W', SMOOTH|GRAVE, "&#x1f6a;"},
    {'W', SMOOTH|ACUTE, "&#x1f6c;"},
    {'W', SMOOTH|CIRCUMFLEX, "&#x1f6e;"},
    {'W', ROUGH, "&#x1f69;"},
    {'W', ROUGH|GRAVE, "&#x1f6b;"},
    {'W', ROUGH|ACUTE, "&#x1f6d;"},
    {'W', ROUGH|CIRCUMFLEX, "&#x1f6f;"},
    {'a', GRAVE, "&#x1f70;"},
    {'e', GRAVE, "&#x1f72;"},
    {'h', GRAVE, "&#x1f74;"},
    {'i', GRAVE, "&#x1f76;"},
    {'o', GRAVE, "&#x1f78;"},
    {'u', GRAVE, "&#x1f7A;"},
    {'w', GRAVE, "&#x1f7C;"},
    {'a', ACUTE, "&#x1f71;"},
    {'e', ACUTE, "&#x1f73;"},
    {'h', ACUTE, "&#x1f75;"},
    {'i', ACUTE, "&#x1f77;"},
    {'o', ACUTE, "&#x1f79;"},
    {'u', ACUTE, "&#x1f7B;"},
    {'w', ACUTE, "&#x1f7D;"},
    {'h', IOTA|SMOOTH, "&#x1f90;"},
    {'h', IOTA|SMOOTH|GRAVE, "&#x1f92;"},
    {'h', IOTA|SMOOTH|ACUTE, "&#x1f94;"},
    {'h', IOTA|SMOOTH|CIRCUMFLEX, "&#x1f96;"},
    {'h', IOTA|ROUGH, "&#x1f91;"},
    {'h', IOTA|ROUGH|GRAVE, "&#x1f93;"},
    {'h', IOTA|ROUGH|ACUTE, "&#x1f95;"},
    {'h', IOTA|ROUGH|CIRCUMFLEX, "&#x1f97;"},
    {'H', IOTA|SMOOTH, "&#x1f98;"},
    {'H', IOTA|SMOOTH|GRAVE, "&#x1f9a;"},
    {'H', IOTA|SMOOTH|ACUTE, "&#x1f9c;"},
    {'H', IOTA|SMOOTH|CIRCUMFLEX, "&#x1f9e;"},
    {'H', IOTA|ROUGH, "&#x1f99;"},
    {'H', IOTA|ROUGH|GRAVE, "&#x1f9b;"},
    {'H', IOTA|ROUGH|ACUTE, "&#x1f9d;"},
    {'H', IOTA|ROUGH|CIRCUMFLEX, "&#x1f9f;"},
    {'w', IOTA|SMOOTH, "&#x1fa0;"},
    {'w', IOTA|SMOOTH|GRAVE, "&#x1fa2;"},
    {'w', IOTA|SMOOTH|ACUTE, "&#x1fa4;"},
    {'w', IOTA|SMOOTH|CIRCUMFLEX, "&#x1fa6;"},
    {'w', IOTA|ROUGH, "&#x1fa1;"},
    {'w', IOTA|ROUGH|GRAVE, "&#x1fa3;"},
    {'w', IOTA|ROUGH|ACUTE, "&#x1fa5;"},
    {'w', IOTA|ROUGH|CIRCUMFLEX, "&#x1fa7;"},
    {'W', IOTA|SMOOTH, "&#x1fa8;"},
    {'W', IOTA|SMOOTH|GRAVE, "&#x1faa;"},
    {'W', IOTA|SMOOTH|ACUTE, "&#x1fac;"},
    {'W', IOTA|SMOOTH|CIRCUMFLEX, "&#x1fae;"},
    {'W', IOTA|ROUGH, "&#x1fa9;"},
    {'W', IOTA|ROUGH|GRAVE, "&#x1fab;"},
    {'W', IOTA|ROUGH|ACUTE, "&#x1fad;"},
    {'W', IOTA|ROUGH|CIRCUMFLEX, "&#x1faf;"},
    {'h', IOTA|GRAVE, "&#x1fc2;"},
    {'h', IOTA, "&#x1fc3;"},
    {'h', IOTA|ACUTE, "&#x1fc4;"},
    {'h', CIRCUMFLEX, "&#x1fc6;"},
    {'h', IOTA|CIRCUMFLEX, "&#x1fc7;"},
    {'w', IOTA|GRAVE, "&#x1ff2;"},
    {'w', IOTA, "&#x1ff3;"},
    {'w', IOTA|ACUTE, "&#x1ff4;"},
    {'w', CIRCUMFLEX, "&#x1ff6;"},
    {'w', IOTA|CIRCUMFLEX, "&#x1ff7;"},
    {'H', GRAVE, "&#x1fca;"},
    {'H', ACUTE, "&#x1fcb;"},
    {'H', IOTA, "&#x1fcc;"},
    {'W', GRAVE, "&#x1ffa;"},
    {'W', ACUTE, "&#x1ffb;"},
    {'W', IOTA, "&#x1ffc;"},
    {'E', GRAVE, "&#x1fc8;"},
    {'E', ACUTE, "&#x1fc9;"},
    {'i', GRAVE|DIAERESIS, "&#x1fd8;"},
    {'i', ACUTE|DIAERESIS, "&#x1fd9;"},
    {'i', CIRCUMFLEX, "&#x1fd6;"},
    {'i', CIRCUMFLEX|DIAERESIS, "&#x1fd7;"},
    {'I', GRAVE, "&#x1fda;"},
    {'I', ACUTE, "&#x1fdb;"},
    {'u', GRAVE|DIAERESIS, "&#x1fe8;"},
    {'u', ACUTE|DIAERESIS, "&#x1fe9;"},
    {'u', CIRCUMFLEX, "&#x1fe6;"},
    {'u', CIRCUMFLEX|DIAERESIS, "&#x1fe7;"},
    {'U', GRAVE, "&#x1fea;"},
    {'U', ACUTE, "&#x1feb;"},
    {'O', GRAVE, "&#x1ff8;"},
    {'O', ACUTE, "&#x1ff9;"},
    {'r', SMOOTH, "&#x1fe4;"},
    {'r', ROUGH, "&#x1fe5;"},
    {'R', ROUGH, "&#x1fec;"},
};

/*  Returns the address of a string which contains the currently activated
    diacritic signs according to the parameters. */
char *print_diacritics(int c)
{
    static char buffer[BUFSIZ];
    sprintf(buffer, "%c with ", c);
    if ( g_rough ) strcat(buffer, "rough ");
    if ( g_smooth ) strcat(buffer, "smooth ");
    if ( g_acute ) strcat(buffer, "acute ");
    if ( g_grave ) strcat(buffer, "grave ");
    if ( g_circumflex ) strcat(buffer, "circumflex ");
    if ( g_diaeresis ) strcat(buffer, "diaeresis ");
    if ( g_iota ) strcat(buffer, "iota subscript");
    return buffer;
}

/*  Table of "character-string": this is a 96 long array of strings (runnning
    from ASCII code 0x20 (the 0th element) to ASCII code 0x7F (the 96th element),
    where at the i-th element is stored the string to substitute for the character
    of ASCII encoding i - 0x20, or NULL if no substitution has to be done. */
const char *CHR_TRANS[96] = {
    NULL, NULL, NULL, NULL,
    NULL, NULL, "&kappa;&alpha;&#x1f76;", "&#x1fbf;",
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, "&#x00b7;",
    NULL, NULL, NULL, NULL,
    NULL, "&Alpha;", "&Beta;", "&Chi;",
    "&Delta;", "&Epsilon;", "&Phi;", "&Gamma;",
    "&Eta;", "&Iota;", "&#x3f9;", "&Kappa;",
    "&Lambda;", "&Mu;", "&Nu;", "&Omicron;",
    "&Pi;", "&Theta;", "&Rho;", "&Sigma;",
    "&Tau;", "&Upsilon;", "&#x3dc;", "&Omega;",
    "&Xi;", "&Psi;", "&Zeta;", NULL,
    NULL, NULL, NULL, NULL, "&#x1ffe;",
    "&alpha;", "&beta;", "&chi;", "&delta;",
    "&epsilon;", "&phi;", "&gamma;", "&eta;",
    "&iota;", "&sigmaf;", "&kappa;", "&lambda;",
    "&mu;", "&nu;", "&omicron;", "&pi;",
    "&theta;", "&rho;", NULL, "&tau;",
    "&upsilon;", "&#x3dd;", "&omega;", "&xi;",
    "&psi;", "&zeta;", NULL, NULL,
    NULL, NULL, NULL,
};

/*  Translates the content of a greek region, until che end of file or the
    next escape character is found. */
void greml_transliterate(void)
{
    int c, i;
    
    /* Resets diacritic flags. */
    g_acute = 0;
    g_circumflex = 0;
    g_diaeresis = 0;
    g_grave = 0;
    g_iota = 0;
    g_rough = 0;
    g_smooth = 0;
    
    /* Scanning loop. */
    while ( (c = fget_char()) != EOF && c != g_escape ) {
        int breathing = g_rough + g_smooth;
        int accent = g_acute + g_circumflex + g_grave;
        int other = g_diaeresis + g_iota;

        /* Check against diacritics coherence. */
        error_on( breathing > 1, "Cannot mix different breathings: %s",
            print_diacritics(c));
        error_on( accent > 1, "Cannot mix different accents: %s",
            print_diacritics(c));
        error_on( breathing + accent + other > 3,
            "More than three diacritics together: %s",
                print_diacritics(c));
        
        /* Take care of diacritics... */
        if ( breathing + accent + other > 0 && isalpha(c) ) {
            /* Produces the bit string corresponding to diacritic flags. */
            int flag = g_rough * ROUGH + g_smooth * SMOOTH + g_acute * ACUTE +
                g_grave * GRAVE + g_circumflex * CIRCUMFLEX +
                g_diaeresis * DIAERESIS + g_iota * IOTA;
            for ( i = 0; i < SIZE(DIACRITICS); ++ i ) {
                if ( DIACRITICS[i].c == c && DIACRITICS[i].diacritic == flag ) {
                    fprintf(g_out, DIACRITICS[i].value);
                    g_rough = g_smooth = g_acute = g_grave = g_circumflex
                        = g_diaeresis = g_iota = 0;
                    break;  /* actually will continue the outer while loop. */
                }
            }
            error_on(i == SIZE(DIACRITICS), "Invalid diacritic combination: %s",
                print_diacritics(c));
            continue;   /* the outer while loop. */
        }
        /* Here there aren't diacritics, so any kind of character is expected. */
        
        /* Check against an "alias characters". */
        if ( c >= 0x20 && c < 0x80 && CHR_TRANS[c - 0x20] != NULL ) {
            fputs(CHR_TRANS[c - 0x20], g_out);
            continue;   /* the outer while loop. */
        }
        /* Check against an "executable character". */
        for ( i = 0; i < SIZE(CHR_ACTION); ++ i ) {
            if ( CHR_ACTION[i].c == c ) {
                (*CHR_ACTION[i].fun)();
                goto Continue;  /* continue the outer while loop. */
            }
        }
        /* Here c is a bare character to print! */
        fputc(c, g_out);
Continue:
        ;
    }
}

/*  Scans the inname file and dumps on the outname file, by transliterating
    to Greek inside "greek regions", delimited by g_escape. */
void greml(char *inname, char *outname)
{
    int c;
    
    g_in = fopen(inname, "r");
    g_out = fopen(outname, "w");
    error_on(g_in == NULL, "Cannot open file %s for reading", inname);
    error_on(g_out == NULL, "Cannot open file %s for writing", outname);
    
    g_line = 1;
    g_col = 1;

    g_name = inname;

    while ( (c = fget_char()) != EOF ) {
        ++ g_col;
        if ( c == '\n' ) {
            ++ g_line;
            g_col = 1;
        }
        if ( c != g_escape ) {
            fputc(c, g_out);
        } else {
            c = fget_char();
            if ( c == g_escape ) {
                /* A repeated escape character dumps itself on the output file. */
                fputc(g_escape, g_out);
            } else {
                ungetc(c, g_in);
                greml_transliterate();
            }
        }
    }
    fclose(g_out);
    fclose(g_in);
}

void finally(void)
{
    char buf[sizeof(int)];
    fprintf(stderr, "\nPress <ENTER> to terminate...");
    fgets(buf, sizeof(buf), stdin);
}

const char USAGE[] =
"USAGE:    greml [options] file1 ... filen\n"
"Options:\n"
"          -e c     Sets the escape character to c\n";

int main(int argc, char **argv)
{
    int i;
    char *s;
    char out_name[FILENAME_MAX];
    
    atexit(finally);
    
    for ( i = 1; i < argc; ++ i ) {
        if ( argv[i][0] == '-' && argv[i][0] == '\0' ) {
            switch( argv[i][1] ) {
            case 'e':
                /* Option "-e character", */
                if ( i < argc - 1 ) {
                    ++ i;
                    g_escape = argv[i][0];
                    continue;
                }
                /* Fall through!!! */
            default:
                fputs(USAGE, stderr);
                exit(EXIT_FAILURE);
            }
        } else {
            /* Creates the name of the output file. */
            strncpy(out_name, argv[i], FILENAME_MAX);
            s = strrchr(out_name, '.');
            if ( s != NULL ) {
                /* Drops the originary extension. */
                *s = '\0';
            }
            strncat(out_name, ".html", FILENAME_MAX);
            greml(argv[i], out_name);
        }
    }
    return EXIT_SUCCESS;
}
