/******************************************************************
 * Copyright (C) 2003 by SG Software.
 ******************************************************************/

/* FILE NAME   : plist.c
 * PURPOSE     : SG Konsamp. Play list manipulation
 *               functions implementation.
 * PROGRAMMER  : Sergey Galanov
 * LAST UPDATE : 24.02.2003
 * NOTE        : Module prefix 'plist'.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 2 
 * of the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, 
 * MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "error.h"
#include "plist.h"
#include "song.h"
#include "window.h"

/* Check play list validity macro */
#define PLIST_ASSERT(pl) if ((pl) == NULL) return
#define PLIST_ASSERT_RET(pl, ret) if ((pl) == NULL) return (ret)

/* Get real selection start and end */
#define PLIST_GET_SEL(pl, start, end) \
	(((pl)->m_sel_start < (pl)->m_sel_end) ? ((start) = (pl)->m_sel_start, \
	 	(end) = (pl)->m_sel_end) : ((end) = (pl)->m_sel_start, \
	 	(start) = (pl)->m_sel_end))

/* Create a new play list */
plist_t *plist_new( int start_pos, int height )
{
	plist_t *pl;

	/* Try to allocate memory for play list object */
	pl = (plist_t *)malloc(sizeof(plist_t));
	if (pl == NULL)
	{
		error_set_code(ERROR_NO_MEMORY);
		return NULL;
	}

	/* Set play list fields */
	pl->m_start_pos = start_pos;
	pl->m_height = height;
	pl->m_scrolled = 0;
	pl->m_sel_start = pl->m_sel_end = 0;
	pl->m_cur_song = -1;
	pl->m_visual = FALSE;
	pl->m_len = 0;
	pl->m_list = NULL;
	return pl;
} /* End of 'plist_new' function */

/* Destroy play list */
void plist_free( plist_t *pl )
{
	if (pl != NULL)
	{
		if (pl->m_list != NULL)
		{
			int i;
			
			for ( i = 0; i < pl->m_len; i ++ )
				song_free(pl->m_list[i]);
			free(pl->m_list);
		}
		
		free(pl);
	}
} /* End of 'plist_free' function */

/* Add a file to play list */
bool plist_add( plist_t *pl, char *filename )
{
	char str[256];
	FILE *fd;
	int i;
	char fname[256];

	/* Do nothing if path is empty */
	if (!filename[0])
		return FALSE;
	
	/* Get full path of filename */
	strcpy(fname, filename);
	if (filename[0] != '/' && filename[0] != '~')
	{
		char wd[256];
		char fn[256];
		
		getcwd(wd);
		strcpy(fn, fname);
		sprintf(fname, "%s/%s", wd, fn);
	}

	/* Check some symbols in path to respective using escapes */
	for ( i = 0; i < strlen(fname); i ++ )
	{
		if (fname[i] == ' ' || fname[i] == '(' || fname[i] == ')' ||
				fname[i] == '\'' || fname[i] == '\"' || fname[i] == '!' ||
        fname[i] == '&')
		{
			memmove(&fname[i + 1], &fname[i], strlen(fname) - i + 1);
			fname[i ++] = '\\';
		}
	}

	/* Find */
	sprintf(str, "find %s 2>/dev/null | grep -i \"\\.[{mp3}{m3u}{wav}]\"", 
			fname);
	fd = popen(str, "r");
	while (fd != NULL)
	{
		char fn[256];

		fgets(fn, 256, fd);
		if (feof(fd))
			break;
		if (fn[strlen(fn) - 1] == '\n')
			fn[strlen(fn) - 1] = 0;
		plist_add_one_file(pl, fn);
	}
	
	return TRUE;
} /* End of 'plist_add' function */

/* Add single file to play list */
bool plist_add_one_file( plist_t *pl, char *filename )
{
	int i;
	char *ext;

	PLIST_ASSERT_RET(pl, FALSE);

	/* Add song */
	if (!strcmp((char *)util_get_ext(filename), "m3u"))
		plist_add_list(pl, filename);
	else
		plist_add_song(pl, filename, NULL, 0);
	return TRUE;
} /* End of 'plist_add_one_file' function */

/* Add a song to play list */
bool plist_add_song( plist_t *pl, char *filename, char *title, int len )
{
	song_t *song;
	int was_len;
	
	PLIST_ASSERT_RET(pl, FALSE);
	
	/* Try to reallocate memory for play list */
	was_len = pl->m_len;
	if (pl->m_list == NULL)
	{
		pl->m_list = (song_t **)malloc(sizeof(song_t *));
	}
	else
	{
		pl->m_list = (song_t **)realloc(pl->m_list,
				sizeof(song_t *) * (pl->m_len + 1));
	}
	if (pl->m_list == NULL)
	{
		pl->m_len = 0;
		error_set_code(ERROR_NO_MEMORY);
		return FALSE;
	}

	/* Initialize new song and add it to list */
	song = song_new(filename, title, len);
	if (song == NULL)
		return FALSE;
	pl->m_list[pl->m_len ++] = song;

	/* If list was empty - put cursor to the first song */
	if (!was_len)
	{
		pl->m_sel_start = pl->m_sel_end = 0;
		pl->m_visual = FALSE;
	}
	
	return TRUE;
} /* End of 'plist_add_song' function */

/* Add a play list file to play list */
bool plist_add_list( plist_t *pl, char *filename )
{
	FILE *fd;
	char str[256];

	PLIST_ASSERT_RET(pl, FALSE);
	
	/* Try to open file */
	fd = fopen(filename, "rt");
	if (fd == NULL)
	{
		error_set_code(ERROR_NO_SUCH_FILE);
		return FALSE;
	}

	/* Read file head */
	fgets(str, sizeof(str), fd);
	if (strcmp(str, "#EXTM3U\n"))
	{
		error_set_code(ERROR_UNKNOWN_FILE_TYPE);
		return FALSE;
	}
		
	/* Read file contents */
	while (!feof(fd))
	{
		char len[10], title[80];
		int i, j, song_len;
		
		/* Read song length and title string */
		fgets(str, sizeof(str), fd);
		if (feof(fd))
			break;

		/* Extract song length from string read */
		for ( i = 8, j = 0; str[i] && str[i] != ','; i ++, j ++ )
			len[j] = str[i];
		len[j] = 0;
		if (str[i])
			song_len = atoi(len);

		/* Extract song title from string read */
		strcpy(title, &str[i + 1]);
		title[strlen(title) - 1] = 0;

		/* Read song file name */
		fgets(str, sizeof(str), fd);
		str[strlen(str) - 1] = 0;

		/* Add this song to list */
		plist_add_song(pl, str, title, song_len);
	}

	/* Close file */
	fclose(fd);
	return TRUE;
} /* End of 'plist_add_list' function */

/* Save play list */
bool plist_save( plist_t *pl, char *filename )
{
	FILE *fd;
	int i;

	PLIST_ASSERT_RET(pl, FALSE);
	
	/* Try to create file */
	fd = fopen(filename, "wt");
	if (fd == NULL)
	{
		error_set_code(ERROR_NO_SUCH_FILE);
		return FALSE;
	}
	
	/* Write list head */
	fprintf(fd, "#EXTM3U\n");
	for ( i = 0; i < pl->m_len; i ++ )
	{
		fprintf(fd, "#EXTINF:%i,%s\n%s\n", pl->m_list[i]->m_len,
				pl->m_list[i]->m_title, pl->m_list[i]->m_file_name);
	}

	/* Close file */
	fclose(fd);
	return TRUE;
} /* End of 'plist_save' function */

/* Sort play list */
void plist_sort( plist_t *pl, bool global, int criteria )
{
	int start, end, i;
	
	PLIST_ASSERT(pl);

	/* Get sort start and end */
	if (global)
		start = 0, end = pl->m_len - 1;
	else
		PLIST_GET_SEL(pl, start, end);

	/* Sort */
	for ( i = start; i < end; i ++ )
	{
		int k = i + 1, j;
		song_t *s = pl->m_list[k];
		char *str1, *str2;

		/* Get first string */
		switch (criteria)
		{
		case PLIST_SORT_BY_TITLE:
			str1 = s->m_title;
			break;
		case PLIST_SORT_BY_PATH:
			str1 = s->m_file_name;
			break;
		}
		
		for ( j = i; j >= 0; j -- )
		{
			/* Get second string */
			switch (criteria)
			{
			case PLIST_SORT_BY_TITLE:
				str2 = pl->m_list[j]->m_title;
				break;
			case PLIST_SORT_BY_PATH:
				str2 = pl->m_list[j]->m_file_name;
				break;
			}

			/* Save current preferred position */
			if (strcmp(str1, str2) < 0)
				k = j;
			else
				break;
		}

		/* Paste string to its place */
		if (k <= i)
		{
			memmove(&pl->m_list[k + 1], &pl->m_list[k],
					(i - k + 1) * sizeof(*pl->m_list));
			pl->m_list[k] = s;
		}
	}
} /* End of 'plist_sort' function */

/* Remove selected songs from play list */
void plist_rem( plist_t *pl )
{
	int start, end, i, cur;
	
	PLIST_ASSERT(pl);

	/* Get real selection bounds */
	PLIST_GET_SEL(pl, start, end);

	/* Check if we have anything to delete */
	if (!pl->m_len)
		return;

	/* Shift songs list and reallocate memory */
	cur = (pl->m_cur_song >= start && pl->m_cur_song <= end);
	if (!cur)
	{
		memmove(&pl->m_list[start], &pl->m_list[end + 1],
				(pl->m_len - end - 1) * sizeof(*pl->m_list));
	}
	else
	{
		int j;

		for ( i = start, j = start; i < pl->m_len; i ++ )
		{
			if (i == pl->m_cur_song || i > end)
				pl->m_list[j ++] = pl->m_list[i];
		}
	}
	pl->m_len -= (end - start + 1 - cur);
	if (pl->m_len)
		pl->m_list = (song_t **)realloc(pl->m_list, 
				pl->m_len * sizeof(*pl->m_list));
	else
	{
		free(pl->m_list);
		pl->m_list = NULL;
	}

	/* Fix cursor */
	plist_move(pl, start, FALSE);
	pl->m_sel_start = pl->m_sel_end;
	if (cur)
		pl->m_cur_song = pl->m_sel_start;
	else if (pl->m_cur_song > end)
		pl->m_cur_song -= (end - start + 1);
} /* End of 'plist_rem' function */

/* Search for string */
bool plist_search( plist_t *pl, char *str, int dir )
{
	int i, count = 0;
	bool found = FALSE;

	PLIST_ASSERT_RET(pl, FALSE);
	if (!pl->m_len)
		return;

	/* Search */
	for ( i = pl->m_sel_end, count = 0; count < pl->m_len && !found; count ++ )
	{
		/* Go to next song */
		i += dir;
		if (i < 0 && dir < 0)
			i = pl->m_len - 1;
		else if (i >= pl->m_len && dir > 0)
			i = 0;

		/* Search for specified string in song title */
		found = util_search_str(str, pl->m_list[i]->m_title);
		if (found)
			plist_move(pl, i, FALSE);
	} 

	return found;
} /* End of 'plist_search' function */

/* Move cursor in play list */
void plist_move( plist_t *pl, int y, bool relative )
{
	int old_end;
	
	PLIST_ASSERT(pl);

	/* If we have empty list - set position to 0 */
	if (!pl->m_len)
	{
		pl->m_sel_start = pl->m_sel_end = pl->m_scrolled = 0;
	}
	
	/* Change play list selection end */
	old_end = pl->m_sel_end;
	pl->m_sel_end = (relative * pl->m_sel_end) + y;
	if (pl->m_sel_end < 0)
		pl->m_sel_end = 0;
	else if (pl->m_sel_end >= pl->m_len)
		pl->m_sel_end = pl->m_len - 1;

	/* Scroll if need */
	if (pl->m_sel_end < pl->m_scrolled || 
			pl->m_sel_end >= pl->m_scrolled + pl->m_height)
	{
		pl->m_scrolled += (pl->m_sel_end - old_end);
		if (pl->m_scrolled < 0)
			pl->m_scrolled = 0;
		else if (pl->m_scrolled >= pl->m_len)
			pl->m_scrolled = pl->m_len - 1;
	}

	/* Let selection start follow the end in non-visual mode */
	if (!pl->m_visual)
		pl->m_sel_start = pl->m_sel_end;
} /* End of 'plist_move' function */

/* Centrize view */
void plist_centrize( plist_t *pl )
{
	int index;
	
	PLIST_ASSERT(pl);

	if (!pl->m_len)
		return;

	index = pl->m_cur_song;
	if (index >= 0)
	{
		pl->m_sel_end = index;
		if (!pl->m_visual)
			pl->m_sel_start = pl->m_sel_end;

		pl->m_scrolled = pl->m_sel_end - (pl->m_height + 1) / 2;
		if (pl->m_scrolled < 0)
			pl->m_scrolled = 0;
	}
} /* End of 'plist_centrize' function */

/* Display play list */
void plist_display( plist_t *pl, wnd_t *wnd )
{
	int i, j, start, end, l_time = 0, s_time = 0;
	char time_text[80];
	
	PLIST_ASSERT(pl);
	PLIST_GET_SEL(pl, start, end);

	/* Display each song */
	wnd_move(wnd, 0, pl->m_start_pos);
	for ( i = 0, j = pl->m_scrolled; i < pl->m_height; i ++, j ++ )
	{
		int attrib;
		
		/* Set respective print attributes */
		if (j >= start && j <= end)
			attrib = A_REVERSE;
		else
			attrib = A_NORMAL;
		if (j == pl->m_cur_song)
			attrib |= A_BOLD;
		wnd_set_attrib(wnd, attrib);
		
		/* Print song title or blank line (if we are after end of list) */
		if (j < pl->m_len)
		{
			song_t *s = pl->m_list[j];
			char len[10];
			int x;
			
			wnd_printf(wnd, "%i. %s", j + 1, s->m_title);
			sprintf(len, "%i:%02i", s->m_len / 60, s->m_len % 60);
			x = wnd->m_width - strlen(len) - 1;
			while (wnd_getx(wnd) != x)
				wnd_printf(wnd, " ");
			wnd_printf(wnd, "%s\n", len);
		}
		else
			wnd_printf(wnd, "\n");
	}
	wnd_set_attrib(wnd, A_NORMAL);

	/* Display play list time */
	if (pl->m_len)
	{
		for ( i = 0; i < pl->m_len; i ++ )
			l_time += pl->m_list[i]->m_len;
		for ( i = start; i <= end; i ++ )
			s_time += pl->m_list[i]->m_len;
	}
	sprintf(time_text, "%i:%02i:%02i/%i:%02i:%02i",
			s_time / 3600, (s_time % 3600) / 60, s_time % 60,
			l_time / 3600, (l_time % 3600) / 60, l_time % 60);
	wnd_move(wnd, wnd->m_width - strlen(time_text) - 1, wnd_gety(wnd));
	wnd_printf(wnd, "%s\n", time_text);
} /* End of 'plist_display' function */

/* End of 'plist.c' file */
