/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that turns images into character art.
 *
 * Chafa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chafa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Chafa.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"
#include "chafa.h"

/**
 * SECTION:chafa-term-db
 * @title: ChafaTermDb
 * @short_description: A database of terminal information
 *
 * A #ChafaTermDb contains information on terminals, and can be used to obtain
 * a suitable #ChafaTermInfo for a terminal environment.
 **/

/* This is a very naïve implementation, but perhaps good enough for most
 * contemporary terminal emulators. I've kept the API minimal so actual
 * termcap/terminfo subset parsing can be added later if needed without
 * breaking existing applications. */

struct ChafaTermDb
{
    gint refs;
};

typedef struct
{
    ChafaTermSeq seq;
    gchar *str;
}
SeqStr;

static const SeqStr vt220_seqs [] =
{
    { CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT, "\033[!p" },
    { CHAFA_TERM_SEQ_RESET_TERMINAL_HARD, "\033c" },
    { CHAFA_TERM_SEQ_RESET_ATTRIBUTES, "\033[0m" },
    { CHAFA_TERM_SEQ_CLEAR, "\033[2J" },
    { CHAFA_TERM_SEQ_INVERT_COLORS, "\033[7m" },
    { CHAFA_TERM_SEQ_CURSOR_TO_TOP_LEFT, "\033[0H" },
    { CHAFA_TERM_SEQ_CURSOR_TO_BOTTOM_LEFT, "\033[9999;1H" },
    { CHAFA_TERM_SEQ_CURSOR_TO_POS, "\033[%2;%1H" },
    { CHAFA_TERM_SEQ_CURSOR_UP, "\033[%1A" },
    { CHAFA_TERM_SEQ_CURSOR_UP_1, "\033[A" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN, "\033[%1B" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN_1, "\033[B" },
    { CHAFA_TERM_SEQ_CURSOR_LEFT, "\033[%1D" },
    { CHAFA_TERM_SEQ_CURSOR_LEFT_1, "\033[D" },
    { CHAFA_TERM_SEQ_CURSOR_RIGHT, "\033[%1C" },
    { CHAFA_TERM_SEQ_CURSOR_RIGHT_1, "\033[C" },
    { CHAFA_TERM_SEQ_CURSOR_UP_SCROLL, "\033D" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL, "\033M" },
    { CHAFA_TERM_SEQ_INSERT_CELLS, "\033[%1@" },
    { CHAFA_TERM_SEQ_DELETE_CELLS, "\033[%1P" },
    { CHAFA_TERM_SEQ_INSERT_ROWS, "\033[%1L" },
    { CHAFA_TERM_SEQ_DELETE_ROWS, "\033[%1M" },
    { CHAFA_TERM_SEQ_SET_SCROLLING_ROWS, "\033[%1;%2r" },
    { CHAFA_TERM_SEQ_ENABLE_INSERT, "\033[4h" },
    { CHAFA_TERM_SEQ_DISABLE_INSERT,"\033[4l" },
    { CHAFA_TERM_SEQ_ENABLE_CURSOR, "\033[?25h" },
    { CHAFA_TERM_SEQ_DISABLE_CURSOR, "\033[?25l" },
    { CHAFA_TERM_SEQ_ENABLE_ECHO, "\033[12l" },
    { CHAFA_TERM_SEQ_DISABLE_ECHO, "\033[12h" },
    { CHAFA_TERM_SEQ_ENABLE_WRAP, "\033[?7h" },
    { CHAFA_TERM_SEQ_DISABLE_WRAP, "\033[?7l" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr sixel_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_SIXELS, "\033P%1;%2;%3q" },
    { CHAFA_TERM_SEQ_END_SIXELS, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_direct_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT, "\033[38;2;%1;%2;%3m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT, "\033[48;2;%1;%2;%3m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT, "\033[38;2;%1;%2;%3m\033[48;2;%4;%5;%6m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_256_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_256, "\033[38;5;%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_256, "\033[48;5;%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_256, "\033[38;5;%1m\033[48;5;%2m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_16_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_16, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_16, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_16, "\033[%1m\033[%2m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr *color_direct_list [] =
{
    color_direct_seqs,
    color_256_seqs,
    color_16_seqs,
    NULL
};

static const SeqStr *color_256_list [] =
{
    color_256_seqs,
    color_16_seqs,
    NULL
};

static const SeqStr *color_16_list [] =
{
    color_16_seqs,
    NULL
};

static const SeqStr color_fbterm_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_16, "\033[1;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_16, "\033[2;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_16, "\033[1;%1}\033[2;%2}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FG_256, "\033[1;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_256, "\033[2;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_256, "\033[1;%1}\033[2;%2}" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr *color_fbterm_list [] =
{
    color_fbterm_seqs,
    NULL
};

static void
add_seqs (ChafaTermInfo *ti, const SeqStr *seqstr)
{
    gint i;

    if (!seqstr)
        return;

    for (i = 0; seqstr [i].str; i++)
    {
        chafa_term_info_set_seq (ti, seqstr [i].seq, seqstr [i].str, NULL);
    }
}

static void
add_seq_list (ChafaTermInfo *ti, const SeqStr **seqlist)
{
    gint i;

    if (!seqlist)
        return;

    for (i = 0; seqlist [i]; i++)
    {
        add_seqs (ti, seqlist [i]);
    }
}

static void
detect_capabilities (ChafaTermInfo *ti, gchar **envp)
{
    const gchar *term;
    const gchar *colorterm;
    const gchar *vte_version;
    const gchar *tmux;
    const SeqStr **color_seq_list = color_256_list;
    const SeqStr *gfx_seqs = NULL;

    add_seqs (ti, vt220_seqs);

    term = g_environ_getenv (envp, "TERM");
    if (!term) term = "";

    colorterm = g_environ_getenv (envp, "COLORTERM");
    if (!colorterm) colorterm = "";

    vte_version = g_environ_getenv (envp, "VTE_VERSION");
    if (!vte_version) vte_version = "";

    tmux = g_environ_getenv (envp, "TMUX");
    if (!tmux) tmux = "";

    /* Some terminals set COLORTERM=truecolor. However, this env var can
     * make its way into environments where truecolor is not desired
     * (e.g. screen sessions), so check it early on and override it later. */
    if (!g_ascii_strcasecmp (colorterm, "truecolor")
        || !g_ascii_strcasecmp (colorterm, "gnome-terminal")
        || !g_ascii_strcasecmp (colorterm, "xfce-terminal"))
        color_seq_list = color_direct_list;

    /* In a modern VTE we can rely on VTE_VERSION. It's a great terminal emulator
     * which supports truecolor. */
    if (strlen (vte_version) > 0)
        color_seq_list = color_direct_list;

    /* Terminals that advertise 256 colors usually support truecolor too,
     * (VTE, xterm) although some (xterm) may quantize to an indexed palette
     * regardless. */
    if (!strcmp (term, "xterm-256color")
        || !strcmp (term, "xterm-kitty"))
        color_seq_list = color_direct_list;

    /* mlterm's truecolor support seems to be broken; it looks like a color
     * allocation issue. This affects character cells, but not sixels.
     *
     * yaft supports sixels and truecolor escape codes, but it remaps cell
     * colors to a 256-color palette. */
    if (!strcmp (term, "mlterm")
        || !strcmp (term, "yaft")
        || !strcmp (term, "yaft-256color"))
    {
        /* The default canvas mode is truecolor for sixels. 240 colors is
         * the default for symbols. */
        color_seq_list = color_256_list;
        gfx_seqs = sixel_seqs;
    }

    /* rxvt 256-color really is 256 colors only */
    if (!strcmp (term, "rxvt-unicode-256color"))
        color_seq_list = color_256_list;

    /* Regular rxvt supports 16 colors at most */
    if (!strcmp (term, "rxvt-unicode"))
        color_seq_list = color_16_list;

    /* 'screen' does not like truecolor at all, but 256 colors works fine.
     * Sometimes we'll see the outer terminal appended to the TERM string,
     * like so: screen.xterm-256color */
    if (!strncmp (term, "screen", 6))
    {
        color_seq_list = color_256_list;

        /* 'tmux' also sets TERM=screen, but it supports truecolor codes.
         * You may have to add the following to .tmux.conf to prevent
         * remapping to 256 colors:
         *
         * tmux set-option -ga terminal-overrides ",screen-256color:Tc" */
        if (strlen (tmux) > 0)
            color_seq_list = color_direct_list;
    }

    /* If TERM is "linux", we're probably on the Linux console, which supports
     * 16 colors only. It also sets COLORTERM=1.
     *
     * https://github.com/torvalds/linux/commit/cec5b2a97a11ade56a701e83044d0a2a984c67b4
     *
     * In theory we could emit truecolor codes and let the console remap,
     * but we get better results if we do the conversion ourselves, since we
     * can apply preprocessing and exotic color spaces. */
    if (!strcmp (term, "linux"))
        color_seq_list = color_16_list;

    /* FbTerm can use 256 colors through a private extension; see fbterm(1) */
    if (!strcmp (term, "fbterm"))
        color_seq_list = color_fbterm_list;

    add_seq_list (ti, color_seq_list);
    add_seqs (ti, gfx_seqs);
}

/* Public */

/**
 * chafa_term_db_new:
 *
 * Creates a new, blank #ChafaTermDb.
 *
 * Returns: The new #ChafaTermDb
 **/
ChafaTermDb *
chafa_term_db_new (void)
{
    ChafaTermDb *term_db;

    term_db = g_new0 (ChafaTermDb, 1);
    term_db->refs = 1;

    return term_db;
}

/**
 * chafa_term_db_copy:
 * @term_db: A #ChafaTermDb to copy.
 *
 * Creates a new #ChafaTermDb that's a copy of @term_db.
 *
 * Returns: The new #ChafaTermDb
 **/
ChafaTermDb *
chafa_term_db_copy (const ChafaTermDb *term_db)
{
    ChafaTermDb *new_term_db;

    new_term_db = g_new (ChafaTermDb, 1);
    memcpy (new_term_db, term_db, sizeof (ChafaTermDb));
    new_term_db->refs = 1;

    return new_term_db;
}

/**
 * chafa_term_db_ref:
 * @term_db: #ChafaTermDb to add a reference to.
 *
 * Adds a reference to @term_db.
 **/
void
chafa_term_db_ref (ChafaTermDb *term_db)
{
    gint refs;

    g_return_if_fail (term_db != NULL);
    refs = g_atomic_int_get (&term_db->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&term_db->refs);
}

/**
 * chafa_term_db_unref:
 * @term_db: #ChafaTermDb to remove a reference from.
 *
 * Removes a reference from @term_db.
 **/
void
chafa_term_db_unref (ChafaTermDb *term_db)
{
    gint refs;

    g_return_if_fail (term_db != NULL);
    refs = g_atomic_int_get (&term_db->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&term_db->refs))
    {
        g_free (term_db);
    }
}

/**
 * chafa_term_db_detect:
 * @term_db: A #ChafaTermDb.
 * @envp: A strv of environment variables.
 *
 * Builds a new #ChafaTermInfo with capabilities implied by the provided
 * environment variables (principally the TERM variable, but also others).
 *
 * @envp can be gotten from g_get_environ().
 *
 * Returns: A new #ChafaTermInfo.
 **/
ChafaTermInfo *
chafa_term_db_detect (ChafaTermDb *term_db, gchar **envp)
{
    ChafaTermInfo *ti;

    g_return_val_if_fail (term_db != NULL, NULL);

    ti = chafa_term_info_new ();
    detect_capabilities (ti, envp);
    return ti;
}
