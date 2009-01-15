/* $scrotwm$ */
/*
 * Copyright (c) 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2009 Ryan McBride <mcbride@countersiege.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Much code and ideas taken from dwm under the following license:
 * MIT/X Consortium License
 * 
 * 2006-2008 Anselm R Garbe <garbeam at gmail dot com>
 * 2006-2007 Sander van Dijk <a dot h dot vandijk at gmail dot com>
 * 2006-2007 Jukka Salmi <jukka at salmi dot ch>
 * 2007 Premysl Hruby <dfenze at gmail dot com>
 * 2007 Szabolcs Nagy <nszabolcs at gmail dot com>
 * 2007 Christof Musik <christof at sendfax dot de>
 * 2007-2008 Enno Gottox Boland <gottox at s01 dot de>
 * 2007-2008 Peter Hartlich <sgkkr at hartlich dot com>
 * 2008 Martin Hurton <martin dot hurton at gmail dot com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define	SWM_VERSION	"0.5"

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <locale.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <util.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/param.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

/* #define SWM_DEBUG */
#ifdef SWM_DEBUG
#define DPRINTF(x...)		do { if (swm_debug) fprintf(stderr, x); } while(0)
#define DNPRINTF(n,x...)	do { if (swm_debug & n) fprintf(stderr, x); } while(0)
#define	SWM_D_MISC		0x0001
#define	SWM_D_EVENT		0x0002
#define	SWM_D_WS		0x0004
#define	SWM_D_FOCUS		0x0008
#define	SWM_D_MOVE		0x0010

u_int32_t		swm_debug = 0
			    | SWM_D_MISC
			    | SWM_D_EVENT
			    | SWM_D_WS
			    | SWM_D_FOCUS
			    | SWM_D_MOVE
			    ;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define LENGTH(x)               (sizeof x / sizeof x[0])
#define MODKEY			Mod1Mask
#define CLEANMASK(mask)         (mask & ~(numlockmask | LockMask))

int			(*xerrorxlib)(Display *, XErrorEvent *);
int			other_wm;
int			screen;
int			width, height;
int			running = 1;
int			ignore_enter = 0;
unsigned int		numlockmask = 0;
unsigned long		color_focus = 0xff0000;	/* XXX should this be per ws? */
unsigned long		color_unfocus = 0x888888;
Display			*display;
Window			root;

/* status bar */
int			bar_enabled = 1;
int			bar_height = 0;
unsigned long		bar_border = 0x008080;
unsigned long		bar_color = 0x000000;
unsigned long		bar_font_color = 0xa0a0a0;
Window			bar_window;
GC			bar_gc;
XGCValues		bar_gcv;
XFontStruct		*bar_fs;
char			bar_text[128];
char			*bar_fonts[] = {
			    "-*-terminus-*-*-*-*-*-*-*-*-*-*-*-*",
			    "-*-times-medium-r-*-*-*-*-*-*-*-*-*-*",
			    NULL
};

/* terminal + args */
char				*spawn_term[] = { "xterm", NULL };
char				*spawn_menu[] = { "dmenu_run", NULL };
char				*spawn_scrotwm[] = { "scrotwm", NULL };

/* layout manager data */
void	stack(void);
void	vertical_init(void);
void	vertical_stack(void);
void	horizontal_init(void);
void	horizontal_stack(void);
void	max_init(void);
void	max_stack(void);

struct layout {
	void			(*l_init)(void);	/* init/reset */
	void			(*l_stack)(void);	/* restack windows */
} layouts[] =  {
	/* init			stack */
	{ vertical_init,	vertical_stack},
	{ horizontal_init,	horizontal_stack},
	/* XXX not working yet
	 * { max_init,		max_stack},
	 */
	{ NULL,			NULL},
};

struct ws_win {
	TAILQ_ENTRY(ws_win)	entry;
	Window			id;
	int			x;
	int			y;
	int			width;
	int			height;
	int			floating;
	int			transient;
	XWindowAttributes	wa;
	XSizeHints		sh;
};

TAILQ_HEAD(ws_win_list, ws_win);

/* define work spaces */
#define SWM_WS_MAX		(10)
struct workspace {
	int			visible;	/* workspace visible */
	int			restack;	/* restack on switch */
	struct layout		*cur_layout;	/* current layout handlers */
	struct ws_win		*focus;		/* which win has focus */
	struct ws_win_list	winlist;	/* list of windows in ws */
} ws[SWM_WS_MAX];
int			current_ws = 0;

/* args to functions */
union arg {
	int			id;
#define SWM_ARG_ID_FOCUSNEXT	(0)
#define SWM_ARG_ID_FOCUSPREV	(1)
#define SWM_ARG_ID_FOCUSMAIN	(2)
#define SWM_ARG_ID_SWAPNEXT	(3)
#define SWM_ARG_ID_SWAPPREV	(4)
#define SWM_ARG_ID_SWAPMAIN	(5)
	char			**argv;
};

/* conf file stuff */
#define	SWM_CONF_WS	"\n= \t"
#define SWM_CONF_FILE	"scrotwm.conf"
int
conf_load(char *filename)
{
	FILE			*config;
	char			*line, *cp, *var, *val;
	size_t			 len, lineno = 0;

	DNPRINTF(SWM_D_MISC, "conf_load: filename %s\n", filename);

	if (filename == NULL)
		return (1);

	if ((config = fopen(filename, "r")) == NULL)
		return (1);

	for (;;) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0)) == NULL)
			if (feof(config))
				break;
		cp = line;
		cp += (long)strspn(cp, SWM_CONF_WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}
		if ((var = strsep(&cp, SWM_CONF_WS)) == NULL || cp == NULL)
			break;
		cp += (long)strspn(cp, SWM_CONF_WS);
		if ((val = strsep(&cp, SWM_CONF_WS)) == NULL)
			break;

		DNPRINTF(SWM_D_MISC, "conf_load: %s=%s\n",var ,val);
		switch (var[0]) {
		case 'b':
			if (!strncmp(var, "bar_enabled", strlen("bar_enabled")))
				bar_enabled = atoi(val);
			else if (!strncmp(var, "bar_border",
			    strlen("bar_border")))
				bar_border = strtol(val, NULL, 16);
			else if (!strncmp(var, "bar_color",
			    strlen("bar_color")))
				bar_color = strtol(val, NULL, 16);
			else if (!strncmp(var, "bar_font_color",
			    strlen("bar_font_color")))
				bar_font_color = strtol(val, NULL, 16);
			else if (!strncmp(var, "bar_font", strlen("bar_font")))
				asprintf(&bar_fonts[0], "%s", val);
			else
				goto bad;
			break;

		case 'c':
			if (!strncmp(var, "color_focus", strlen("color_focus")))
				color_focus = strtol(val, NULL, 16);
			else if (!strncmp(var, "color_unfocus",
			    strlen("color_unfocus")))
				color_unfocus = strtol(val, NULL, 16);
			else
				goto bad;
			break;

		case 's':
			if (!strncmp(var, "spawn_term", strlen("spawn_term")))
				asprintf(&spawn_term[0], "%s", val); /* XXX args? */
			break;
		default:
			goto bad;
		}
		free(line);
	}

	fclose(config);
	return (0);
bad:
	errx(1, "invalid conf file entry: %s=%s", var, val);
}

void
bar_print(void)
{
	time_t			tmt;
	struct tm		tm;

	if (bar_enabled == 0)
		return;

	/* clear old text */
	XSetForeground(display, bar_gc, bar_color);
	XDrawString(display, bar_window, bar_gc, 4, bar_fs->ascent, bar_text,
	    strlen(bar_text));

	/* draw new text */
	time(&tmt);
	localtime_r(&tmt, &tm);
	strftime(bar_text, sizeof bar_text, "%a %b %d %R %Z %Y", &tm);
	XSetForeground(display, bar_gc, bar_font_color);
	XDrawString(display, bar_window, bar_gc, 4, bar_fs->ascent, bar_text,
	    strlen(bar_text));
	XSync(display, False);

	alarm(60);
}

void
bar_signal(int sig)
{
	/* XXX yeah yeah byte me */
	bar_print();
}

void
bar_toggle(union arg *args)
{
	int i;

	DNPRINTF(SWM_D_MISC, "bar_toggle\n");

	if (bar_enabled) {
		bar_enabled = 0;
		height += bar_height; /* correct screen height */
		XUnmapWindow(display, bar_window);
	} else {
		bar_enabled = 1;
		height -= bar_height; /* correct screen height */
		XMapWindow(display, bar_window);
	}
	XSync(display, False);
	for (i = 0; i < SWM_WS_MAX; i++)
		ws[i].restack = 1;

	stack();
	bar_print(); /* must be after stack */
}

void
bar_setup(void)
{
	int			i;

	for (i = 0; bar_fonts[i] != NULL; i++) {
		bar_fs = XLoadQueryFont(display, bar_fonts[i]);
		if (bar_fs)
			break;
	}
	if (bar_fonts[i] == NULL)
			errx(1, "couldn't load font");
	bar_height = bar_fs->ascent + bar_fs->descent + 3;

	bar_window = XCreateSimpleWindow(display, root, 0, 0, width,
	    bar_height - 2, 1, bar_border, bar_color);
	bar_gc = XCreateGC(display, bar_window, 0, &bar_gcv);
	XSetFont(display, bar_gc, bar_fs->fid);
	XSelectInput(display, bar_window, VisibilityChangeMask);
	if (bar_enabled) {
		height -= bar_height; /* correct screen height */
		XMapWindow(display, bar_window);
	}
	DNPRINTF(SWM_D_MISC, "bar_setup: bar_window %d\n", (int)bar_window);

	if (signal(SIGALRM, bar_signal) == SIG_ERR)
		err(1, "could not install bar_signal");
	bar_print();
}

int
count_win(int wsid, int count_transient)
{
	struct ws_win		*win;
	int			count = 0;

	TAILQ_FOREACH (win, &ws[wsid].winlist, entry) {
		if (count_transient == 0 && win->transient)
			continue;
		count++;
	}
	DNPRINTF(SWM_D_MISC, "count_win: %d\n", count);

	return (count);
}
void
quit(union arg *args)
{
	DNPRINTF(SWM_D_MISC, "quit\n");
	running = 0;
}


void
restart(union arg *args)
{
	XCloseDisplay(display);
	execvp(args->argv[0], args->argv);
	fprintf(stderr, "execvp failed\n");
	perror(" failed");
	quit(NULL);
}

void
spawn(union arg *args)
{
	DNPRINTF(SWM_D_MISC, "spawn: %s\n", args->argv[0]);
	/*
	 * The double-fork construct avoids zombie processes and keeps the code
	 * clean from stupid signal handlers.
	 */
	if(fork() == 0) {
		if(fork() == 0) {
			if(display)
				close(ConnectionNumber(display));
			setsid();
			execvp(args->argv[0], args->argv);
			fprintf(stderr, "execvp failed\n");
			perror(" failed");
		}
		exit(0);
	}
	wait(0);
}

void
focus_win(struct ws_win *win)
{
	DNPRINTF(SWM_D_FOCUS, "focus_win: id: %lu\n", win->id);
	XSetWindowBorder(display, win->id, color_focus);
	XSetInputFocus(display, win->id, RevertToPointerRoot, CurrentTime);
	ws[current_ws].focus = win;
}

void
unfocus_win(struct ws_win *win)
{
	DNPRINTF(SWM_D_FOCUS, "unfocus_win: id: %lu\n", win->id);
	XSetWindowBorder(display, win->id, color_unfocus);
	if (ws[current_ws].focus == win)
		ws[current_ws].focus = NULL;
}

void
switchws(union arg *args)
{
	int			wsid = args->id;
	struct ws_win		*win;

	DNPRINTF(SWM_D_WS, "switchws: %d\n", wsid + 1);

	if (wsid == current_ws)
		return;

	/* map new window first to prevent ugly blinking */
	TAILQ_FOREACH (win, &ws[wsid].winlist, entry)
		XMapRaised(display, win->id);
	ws[wsid].visible = 1;

	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry)
		XUnmapWindow(display, win->id);
	ws[current_ws].visible = 0;

	current_ws = wsid;

	ignore_enter = 1;
	if (ws[wsid].restack) {
		stack();
		bar_print();
	} else {
		if (ws[wsid].focus != NULL)
			focus_win(ws[wsid].focus);
		XSync(display, False);
	}
}

void
swapwin(union arg *args)
{
	struct ws_win		*target;

	DNPRINTF(SWM_D_MOVE, "swapwin: id %d\n", args->id);
	if (ws[current_ws].focus == NULL)
		return;

	switch (args->id) {
	case SWM_ARG_ID_SWAPPREV:
		target = TAILQ_PREV(ws[current_ws].focus, ws_win_list, entry);
		TAILQ_REMOVE(&ws[current_ws].winlist,
		    ws[current_ws].focus, entry);
		if (target == NULL)
			TAILQ_INSERT_TAIL(&ws[current_ws].winlist,
			    ws[current_ws].focus, entry);
		else
			TAILQ_INSERT_BEFORE(target, ws[current_ws].focus,
			    entry);
		break;
	case SWM_ARG_ID_SWAPNEXT: 
		target = TAILQ_NEXT(ws[current_ws].focus, entry);
		TAILQ_REMOVE(&ws[current_ws].winlist,
		    ws[current_ws].focus, entry);
		if (target == NULL)
			TAILQ_INSERT_HEAD(&ws[current_ws].winlist,
			    ws[current_ws].focus, entry);
		else
			TAILQ_INSERT_AFTER(&ws[current_ws].winlist, target,
			    ws[current_ws].focus, entry);
		break;
	case SWM_ARG_ID_SWAPMAIN:
		target = TAILQ_FIRST(&ws[current_ws].winlist);
		if (target == ws[current_ws].focus)
			return;
		TAILQ_REMOVE(&ws[current_ws].winlist, target, entry);
		TAILQ_INSERT_BEFORE(ws[current_ws].focus, target, entry);
		TAILQ_REMOVE(&ws[current_ws].winlist,
		    ws[current_ws].focus, entry);
		TAILQ_INSERT_HEAD(&ws[current_ws].winlist,
		    ws[current_ws].focus, entry);
		break;
	default:
		DNPRINTF(SWM_D_MOVE, "invalid id: %d\n", args->id);
		return;
	}

	ignore_enter = 2;
	stack();
}

void
focus(union arg *args)
{
	struct ws_win		*winfocus, *winlostfocus;

	DNPRINTF(SWM_D_FOCUS, "focus: id %d\n", args->id);
	if (ws[current_ws].focus == NULL)
		return;

	winlostfocus = ws[current_ws].focus;

	switch (args->id) {
	case SWM_ARG_ID_FOCUSPREV:
		winfocus = TAILQ_PREV(ws[current_ws].focus, ws_win_list, entry);
		if (winfocus == NULL)
			winfocus =
			    TAILQ_LAST(&ws[current_ws].winlist, ws_win_list);
		break;

	case SWM_ARG_ID_FOCUSNEXT:
		winfocus = TAILQ_NEXT(ws[current_ws].focus, entry);
		if (winfocus == NULL)
			winfocus = TAILQ_FIRST(&ws[current_ws].winlist);
		break;

	case SWM_ARG_ID_FOCUSMAIN:
		winfocus = TAILQ_FIRST(&ws[current_ws].winlist);
		break;

	default:
		return;
	}

	if (winfocus == winlostfocus)
		return;

	unfocus_win(winlostfocus);
	focus_win(winfocus);
	XSync(display, False);
}

void
cycle_layout(union arg *args)
{
	DNPRINTF(SWM_D_EVENT, "cycle_layout: workspace: %d\n", current_ws);

	ws[current_ws].cur_layout++;
	if (ws[current_ws].cur_layout->l_stack == NULL)
		ws[current_ws].cur_layout = &layouts[0];
	stack();
}

void
stack(void) {
	DNPRINTF(SWM_D_EVENT, "stack: workspace: %d\n", current_ws);

	ws[current_ws].restack = 0;
	ws[current_ws].cur_layout->l_stack();
	XSync(display, False);
}

void
stack_floater(struct ws_win *win)
{
	unsigned int		mask;
	XWindowChanges		wc;

	bzero(&wc, sizeof wc);
	wc.border_width = 1;
	mask = CWX | CWY | CWBorderWidth;

	/* use obsolete width height */
	if (win->sh.flags & USPosition) {
		win->width = wc.width = win->sh.width;
		win->height = wc.height = win->sh.height;
		mask |= CWWidth | CWHeight;
	}

	/* try min max */
	if (win->sh.flags & PMinSize) {
		/* some hints are retarded */
		if (win->sh.min_width < width / 10)
			win->sh.min_width = width / 3;
		if (win->sh.min_height < height / 10)
			win->wa.height = height / 3;

		win->width = wc.width = win->sh.min_width * 2;
		win->height = wc.height = win->sh.min_height * 2;
		mask |= CWWidth | CWHeight;
	}
	if (win->sh.flags & PMaxSize) {
		/* potentially override min values */
		if (win->sh.max_width < width) {
			win->width = wc.width = win->sh.max_width;
			mask |= CWWidth;
		}
		if (win->sh.max_height < height) {
			win->height = wc.height = win->sh.max_height;
			mask |= CWHeight;
		}
	}

	/* make sure we don't clobber the screen */
	if ((mask & CWWidth) && win->wa.width > width)
		win->wa.width = width - 4;
	if ((mask & CWHeight) && win->wa.height > height)
		win->wa.height = height - 4;

	/* supposed to be obsolete */
	if (win->sh.flags & USPosition) {
		win->x = wc.x = win->sh.x;
		win->y = wc.y = win->sh.y;
	} else {
		win->x = wc.x = (width - win->wa.width) / 2;
		win->y = wc.y = (height - win->wa.height) / 2;
	}
	DNPRINTF(SWM_D_EVENT, "stack_floater: win %d x %d y %d w %d h %d\n",
	    win, wc.x, wc.y, wc.width, wc.height);

	XConfigureWindow(display, win->id, mask, &wc);
}

void
vertical_init(void)
{
	DNPRINTF(SWM_D_EVENT, "vertical_init: workspace: %d\n", current_ws);
}

/* I know this sucks but it works well enough */
void
vertical_stack(void) {
	XWindowChanges		wc;
	struct ws_win		wf, *win, *winfocus = &wf;
	int			i, h, w, x, y, hrh, winno;
	unsigned int		mask;

	DNPRINTF(SWM_D_EVENT, "vertical_stack: workspace: %d\n", current_ws);

	winfocus->id = root;

	winno = count_win(current_ws, 0);
	if (winno == 0)
		return;

	if (winno > 1)
		w = width / 2;
	else
		w = width;

	if (winno > 2)
		hrh = height / (winno - 1);
	else
		hrh = 0;

	x = 0;
	y = bar_enabled ? bar_height : 0;
	h = height;
	i = 0;
	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (i == 1) {
			x += w + 2;
			w -= 2;
		}
		if (i != 0 && hrh != 0) {
			/* correct the last window for lost pixels */
			if (win == TAILQ_LAST(&ws[current_ws].winlist,
			    ws_win_list)) {
				h = height - (i * hrh);
				if (h == 0)
					h = hrh;
				else
					h += hrh;
				y += hrh;
			} else {
				h = hrh - 2;
				/* leave first right hand window at y = 0 */
				if (i > 1)
					y += h + 2;
			}
		}

		if (win->transient != 0 || win->floating != 0)
			stack_floater(win);
		else {
			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->x = wc.x = x;
			win->y = wc.y = y;
			win->width = wc.width = w;
			win->height = wc.height = h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);
		}

		if (win == ws[current_ws].focus)
			winfocus = win;
		else
			unfocus_win(win);
		XMapRaised(display, win->id);
		i++;
	}

	focus_win(winfocus); /* this has to be done outside of the loop */
}

void
horizontal_init(void)
{
	DNPRINTF(SWM_D_EVENT, "horizontal_init: workspace: %d\n", current_ws);
}

void
horizontal_stack(void) {
	XWindowChanges		wc;
	struct ws_win		wf, *win, *winfocus = &wf;
	int			i, h, w, x, y, hrw, winno;
	unsigned int		mask;

	DNPRINTF(SWM_D_EVENT, "horizontal_stack: workspace: %d\n", current_ws);

	winfocus->id = root;

	winno = count_win(current_ws, 0);
	if (winno == 0)
		return;

	if (winno > 1)
		h = height / 2;
	else
		h = height;

	if (winno > 2)
		hrw = width / (winno - 1);
	else
		hrw = 0;

	x = 0;
	y = bar_enabled ? bar_height : 0;
	w = width;
	i = 0;
	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (i == 1) {
			y += h + 2;
			h -= (2 - height % 2);
		}
		if (i != 0 && hrw != 0) {
			/* correct the last window for lost pixels */
			if (win == TAILQ_LAST(&ws[current_ws].winlist,
			    ws_win_list)) {
				w = width - (i * hrw);
				if (w == 0)
					w = hrw;
				else
					w += hrw;
				x += hrw;
			} else {
				w = hrw - 2;
				/* leave first bottom window at x = 0 */
				if (i > 1)
					x += w + 2;
			}
		}

		if (win->transient != 0 || win->floating != 0)
			stack_floater(win);
		else {
			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->x = wc.x = x;
			win->y = wc.y = y;
			win->width = wc.width = w;
			win->height = wc.height = h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);
		}

		if (win == ws[current_ws].focus)
			winfocus = win;
		else
			unfocus_win(win);
		XMapRaised(display, win->id);
		i++;
	}

	focus_win(winfocus); /* this has to be done outside of the loop */
}

/* fullscreen view */
void
max_init(void)
{
	DNPRINTF(SWM_D_EVENT, "max_init: workspace: %d\n", current_ws);
}

void
max_stack(void) {
	XWindowChanges		wc;
	struct ws_win		wf, *win, *winfocus = &wf;
	int			i, h, w, x, y, winno;
	unsigned int mask;

	DNPRINTF(SWM_D_EVENT, "max_stack: workspace: %d\n", current_ws);

	winfocus->id = root;

	winno = count_win(current_ws, 0);
	if (winno == 0)
		return;

	x = 0;
	y = bar_enabled ? bar_height : 0;
	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (i == 1 && win->transient == 0 && win->floating != 0) {
			h = height - 2;
			w = width - 2;

			winfocus = win;

			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->x = wc.x = x;
			win->y = wc.y = y;
			win->width = wc.width = w;
			win->height = wc.height = h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);

			XMapRaised(display, win->id);
		} else {
			/* hide all but the master window */
			XUnmapWindow(display, win->id);
		}
		i++;
	}

	focus_win(winfocus); /* this has to be done outside of the loop */
}

void
send_to_ws(union arg *args)
{
	int			wsid = args->id;
	struct ws_win		*win = ws[current_ws].focus;

	DNPRINTF(SWM_D_MOVE, "send_to_ws: win: %lu\n", win->id);

	XUnmapWindow(display, win->id);

	/* find a window to focus */
	ws[current_ws].focus = TAILQ_PREV(win, ws_win_list, entry);
	if (ws[current_ws].focus == NULL)
		ws[current_ws].focus = TAILQ_FIRST(&ws[current_ws].winlist);
	if (ws[current_ws].focus == win)
		ws[current_ws].focus = NULL;

	TAILQ_REMOVE(&ws[current_ws].winlist, win, entry);

	TAILQ_INSERT_TAIL(&ws[wsid].winlist, win, entry);
	if (count_win(wsid, 1) == 1)
		ws[wsid].focus = win;
	ws[wsid].restack = 1;

	stack();
}

/* key definitions */
struct key {
	unsigned int		mod;
	KeySym			keysym;
	void			(*func)(union arg *);
	union arg		args;
} keys[] = {
	/* modifier		key	function		argument */
	/* XXX alt-c is temporary, until I figure out how to grab spacebar */
	{ MODKEY,		XK_c,		cycle_layout,	{0} }, 
	{ MODKEY,		XK_Return,	swapwin,	{.id = SWM_ARG_ID_SWAPMAIN} },
	{ MODKEY,		XK_j,		focus,		{.id = SWM_ARG_ID_FOCUSNEXT} },
	{ MODKEY,		XK_k,		focus,		{.id = SWM_ARG_ID_FOCUSPREV} },
	{ MODKEY | ShiftMask,	XK_j,		swapwin,	{.id = SWM_ARG_ID_SWAPNEXT} },
	{ MODKEY | ShiftMask,	XK_k,		swapwin,	{.id = SWM_ARG_ID_SWAPPREV} },
	{ MODKEY | ShiftMask,	XK_Return,	spawn,		{.argv = spawn_term} },
	{ MODKEY,		XK_p,		spawn,		{.argv = spawn_menu} },
	{ MODKEY | ShiftMask,	XK_q,		quit,		{0} },
	{ MODKEY,		XK_q,		restart,	{.argv = spawn_scrotwm } },
	{ MODKEY,		XK_m,		focus,		{.id = SWM_ARG_ID_FOCUSMAIN} },
	{ MODKEY,		XK_1,		switchws,	{.id = 0} },
	{ MODKEY,		XK_2,		switchws,	{.id = 1} },
	{ MODKEY,		XK_3,		switchws,	{.id = 2} },
	{ MODKEY,		XK_4,		switchws,	{.id = 3} },
	{ MODKEY,		XK_5,		switchws,	{.id = 4} },
	{ MODKEY,		XK_6,		switchws,	{.id = 5} },
	{ MODKEY,		XK_7,		switchws,	{.id = 6} },
	{ MODKEY,		XK_8,		switchws,	{.id = 7} },
	{ MODKEY,		XK_9,		switchws,	{.id = 8} },
	{ MODKEY,		XK_0,		switchws,	{.id = 9} },
	{ MODKEY | ShiftMask,	XK_1,		send_to_ws,	{.id = 0} },
	{ MODKEY | ShiftMask,	XK_2,		send_to_ws,	{.id = 1} },
	{ MODKEY | ShiftMask,	XK_3,		send_to_ws,	{.id = 2} },
	{ MODKEY | ShiftMask,	XK_4,		send_to_ws,	{.id = 3} },
	{ MODKEY | ShiftMask,	XK_5,		send_to_ws,	{.id = 4} },
	{ MODKEY | ShiftMask,	XK_6,		send_to_ws,	{.id = 5} },
	{ MODKEY | ShiftMask,	XK_7,		send_to_ws,	{.id = 6} },
	{ MODKEY | ShiftMask,	XK_8,		send_to_ws,	{.id = 7} },
	{ MODKEY | ShiftMask,	XK_9,		send_to_ws,	{.id = 8} },
	{ MODKEY | ShiftMask,	XK_0,		send_to_ws,	{.id = 9} },
	{ MODKEY,		XK_b,		bar_toggle,	{0} },
	{ MODKEY,		XK_Tab,		focus,		{.id = SWM_ARG_ID_FOCUSNEXT} },
	{ MODKEY | ShiftMask,	XK_Tab,		focus,		{.id = SWM_ARG_ID_FOCUSPREV} },
};

void
updatenumlockmask(void)
{
	unsigned int		i, j;
	XModifierKeymap		*modmap;

	DNPRINTF(SWM_D_MISC, "updatenumlockmask\n");
	numlockmask = 0;
	modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			  == XKeysymToKeycode(display, XK_Num_Lock))
				numlockmask = (1 << i);

	XFreeModifiermap(modmap);
}

void
grabkeys(void)
{
	unsigned int		i, j;
	KeyCode			code;
	unsigned int		modifiers[] =
	    { 0, LockMask, numlockmask, numlockmask | LockMask };

	DNPRINTF(SWM_D_MISC, "grabkeys\n");
	updatenumlockmask();

	XUngrabKey(display, AnyKey, AnyModifier, root);
	for(i = 0; i < LENGTH(keys); i++) {
		if((code = XKeysymToKeycode(display, keys[i].keysym)))
			for(j = 0; j < LENGTH(modifiers); j++)
				XGrabKey(display, code,
				    keys[i].mod | modifiers[j], root,
				    True, GrabModeAsync, GrabModeAsync);
	}
}
void
expose(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "expose: window: %lu\n", e->xexpose.window);
}

void
keypress(XEvent *e)
{
	unsigned int		i;
	KeySym			keysym;
	XKeyEvent		*ev = &e->xkey;

	DNPRINTF(SWM_D_EVENT, "keypress: window: %lu\n", ev->window);

	keysym = XKeycodeToKeysym(display, (KeyCode)ev->keycode, 0);
	for(i = 0; i < LENGTH(keys); i++)
		if(keysym == keys[i].keysym
		   && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		   && keys[i].func)
			keys[i].func(&(keys[i].args));
}

void
buttonpress(XEvent *e)
{
	XButtonPressedEvent	*ev = &e->xbutton;
#ifdef SWM_CLICKTOFOCUS
	struct ws_win		*win;
#endif


	DNPRINTF(SWM_D_EVENT, "buttonpress: window: %lu\n", ev->window);

	if (ev->window == root)
		return;
	if (ev->window == ws[current_ws].focus->id)
		return;
#ifdef SWM_CLICKTOFOCUS
	TAILQ_FOREACH(win, &ws[current_ws].winlist, entry)
		if (win->id == ev->window) {
			/* focus in the clicked window */
			XSetWindowBorder(display, ev->window, 0xff0000);
			XSetWindowBorder(display,
			    ws[current_ws].focus->id, 0x888888);
			XSetInputFocus(display, ev->window, RevertToPointerRoot,
			    CurrentTime);
			ws[current_ws].focus = win;
			XSync(display, False);
			break;
	}
#endif
}

struct ws_win *
manage_window(Window id)
{
	struct ws_win		*win;

	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (win->id == id)
			return (win);	/* already being managed */
	}

	if ((win = calloc(1, sizeof(struct ws_win))) == NULL)
		errx(1, "calloc: failed to allocate memory for new window");

	win->id = id;
	TAILQ_INSERT_TAIL(&ws[current_ws].winlist, win, entry);

	XSelectInput(display, id, ButtonPressMask | EnterWindowMask |
	    FocusChangeMask | ExposureMask);

	return win;
}

void
configurerequest(XEvent *e)
{
	XConfigureRequestEvent	*ev = &e->xconfigurerequest;
	Window			trans;
	struct ws_win		*win;

	DNPRINTF(SWM_D_EVENT, "configurerequest: window: %lu\n", ev->window);


	win = manage_window(ev->window);
	ws[current_ws].focus = win; /* make new win focused */

	XGetTransientForHint(display, win->id, &trans);
	if (trans) {
		win->transient = trans;
		DNPRINTF(SWM_D_MISC, "configurerequest: win %u transient %u\n",
		    (unsigned)win->id, win->transient);
	}
	XGetWindowAttributes(display, win->id, &win->wa);
	XGetNormalHints(display, win->id, &win->sh);
#if 0
	XClassHint ch = { 0 };
	if(XGetClassHint(display, win->id, &ch)) {
		fprintf(stderr, "class: %s name: %s\n", ch.res_class, ch.res_name);
		if (!strcmp(ch.res_class, "Gvim") && !strcmp(ch.res_name, "gvim")) {
			win->floating = 0;
		}
		if(ch.res_class)
			XFree(ch.res_class);
		if(ch.res_name)
			XFree(ch.res_name);
	}
#endif
	stack();
}

void
configurenotify(XEvent *e)
{

	DNPRINTF(SWM_D_EVENT, "configurenotify: window: %lu\n",
	    e->xconfigure.window);
}

void
destroynotify(XEvent *e)
{
	struct ws_win		*win;
	XDestroyWindowEvent	*ev = &e->xdestroywindow;

	DNPRINTF(SWM_D_EVENT, "destroynotify: window %lu\n", ev->window);

	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (ev->window == win->id) {
			/* find a window to focus */
			ws[current_ws].focus = TAILQ_PREV(win,
			    ws_win_list, entry);
			if (ws[current_ws].focus == NULL)
				ws[current_ws].focus =
				    TAILQ_FIRST(&ws[current_ws].winlist);
			if (win == ws[current_ws].focus)
				ws[current_ws].focus = NULL;
	
			TAILQ_REMOVE(&ws[current_ws].winlist, win, entry);
			free(win);
			break;
		}
	}

	stack();
}

void
enternotify(XEvent *e)
{
	XCrossingEvent		*ev = &e->xcrossing;
	struct ws_win		*win;

	DNPRINTF(SWM_D_EVENT, "enternotify: window: %lu\n", ev->window);

	if((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
	    ev->window != root)
		return;
	if (ignore_enter) {
		/* eat event(s) to prevent autofocus */
		ignore_enter--;
		return;
	}
	TAILQ_FOREACH (win, &ws[current_ws].winlist, entry) {
		if (win->id == ev->window)
			focus_win(win);
		else
			unfocus_win(win);
	}
}

void
focusin(XEvent *e)
{
	XFocusChangeEvent	*ev = &e->xfocus;

	DNPRINTF(SWM_D_EVENT, "focusin: window: %lu\n", ev->window);

	XSync(display, False); /* not sure this helps redrawing graphic apps */

	if (ev->window == root)
		return;
	/*
	 * kill grab for now so that we can cut and paste , this screws up
	 * click to focus
	 */
	/*
	DNPRINTF(SWM_D_EVENT, "focusin: window: %lu grabbing\n", ev->window);
	XGrabButton(display, Button1, AnyModifier, ev->window, False,
	    ButtonPress, GrabModeAsync, GrabModeSync, None, None);
	*/
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent		*ev = &e->xmapping;

	DNPRINTF(SWM_D_EVENT, "mappingnotify: window: %lu\n", ev->window);

	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "maprequest: window: %lu\n",
	    e->xmaprequest.window);

	manage_window(e->xmaprequest.window);
	stack();
}

void
propertynotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "propertynotify: window: %lu\n",
	    e->xproperty.window);
}

void
unmapnotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "unmapnotify: window: %lu\n", e->xunmap.window);
}

void
visibilitynotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "visibilitynotify: window: %lu\n", e->xvisibility.window);

	if (e->xvisibility.window == bar_window &&
	    e->xvisibility.state == VisibilityUnobscured)
		bar_print();
}

void			(*handler[LASTEvent])(XEvent *) = {
				[Expose] = expose,
				[KeyPress] = keypress,
				[ButtonPress] = buttonpress,
				[ConfigureRequest] = configurerequest,
				[ConfigureNotify] = configurenotify,
				[DestroyNotify] = destroynotify,
				[EnterNotify] = enternotify,
				[FocusIn] = focusin,
				[MappingNotify] = mappingnotify,
				[MapRequest] = maprequest,
				[PropertyNotify] = propertynotify,
				[UnmapNotify] = unmapnotify,
				[VisibilityNotify] = visibilitynotify,
};

int
xerror_start(Display *d, XErrorEvent *ee)
{
	other_wm = 1;
	return (-1);
}

int
xerror(Display *d, XErrorEvent *ee)
{
	fprintf(stderr, "error: %p %p\n", display, ee);

	return (-1);
}

int
active_wm(void)
{
	other_wm = 0;
	xerrorxlib = XSetErrorHandler(xerror_start);

	/* this causes an error if some other window manager is running */
	XSelectInput(display, DefaultRootWindow(display),
	    SubstructureRedirectMask);
	XSync(display, False);
	if(other_wm)
		return (1);

	XSetErrorHandler(xerror);
	XSync(display, False);
	return (0);
}

int
main(int argc, char *argv[])
{
	struct passwd		*pwd;
	char			conf[PATH_MAX], *cfile = NULL;
	struct stat		sb;
	XEvent			e;
	unsigned int		i, num;
	Window			d1, d2, *wins = NULL;
	XWindowAttributes	wa;

	fprintf(stderr, "Welcome to scrotwm V%s\n", SWM_VERSION);
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		warnx("no locale support");

	if(!(display = XOpenDisplay(0)))
		errx(1, "can not open display");

	if (active_wm())
		errx(1, "other wm running");

	screen = DefaultScreen(display);
	root = RootWindow(display, screen);
	width = DisplayWidth(display, screen) - 2;
	height = DisplayHeight(display, screen) - 2;

	/* look for local and global conf file */
	pwd = getpwuid(getuid());
	if (pwd == NULL)
		errx(1, "invalid user %d", getuid());

	snprintf(conf, sizeof conf, "%s/.%s", pwd->pw_dir, SWM_CONF_FILE);
	if (stat(conf, &sb) != -1) {
		if (S_ISREG(sb.st_mode))
			cfile = conf;
	} else {
		/* try global conf file */
		snprintf(conf, sizeof conf, "/etc/%s", SWM_CONF_FILE);
		if (!stat(conf, &sb))
			if (S_ISREG(sb.st_mode))
				cfile = conf;
	}
	if (cfile)
		conf_load(cfile);

	/* init all workspaces */
	for (i = 0; i < SWM_WS_MAX; i++) {
		ws[i].visible = 0;
		ws[i].restack = 1;
		ws[i].focus = NULL;
		TAILQ_INIT(&ws[i].winlist);
		ws[i].cur_layout = &layouts[0];
	}
	/* make work space 1 active */
	ws[0].visible = 1;

	/* grab existing windows */
	if (XQueryTree(display, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
                        if (!XGetWindowAttributes(display, wins[i], &wa)
			    || wa.override_redirect ||
			    XGetTransientForHint(display, wins[i], &d1))
				continue;
			manage_window(wins[i]);
                }
                if(wins)
                        XFree(wins);
        }
	ws[0].focus = TAILQ_FIRST(&ws[0].winlist);

	/* setup status bar */
	bar_setup();

	XSelectInput(display, root, SubstructureRedirectMask |
	    SubstructureNotifyMask | ButtonPressMask | KeyPressMask |
	    EnterWindowMask | LeaveWindowMask | StructureNotifyMask |
	    FocusChangeMask | PropertyChangeMask | ExposureMask);

	grabkeys();
	stack();

	while (running) {
		XNextEvent(display, &e);
		if (handler[e.type])
			handler[e.type](&e);
	}

	XCloseDisplay(display);

	return (0);
}
