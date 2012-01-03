/******************************************************************
 * Copyright (C) 2003 - 2005 by SG Software.
 *
 * SG MPFC. Songs manipulation functions implementation.
 * $Id$
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
#include <string.h>
#include "types.h"
#include "cfg.h"
#include "file.h"
#include "inp.h"
#include "mystring.h"
#include "player.h"
#include "pmng.h"
#include "song.h"
#include "song_info.h"
#include "util.h"
#include "vfs.h"

/* Create a new song */
song_t *song_new( vfs_file_t *file, char *title, int len )
{
	song_t *song;
	char *filename = file->m_name;

	/* Is this a supported format */
	if (file_get_type(filename) == FILE_TYPE_REGULAR)
	{
		if (!pmng_search_format(player_pmng, file->m_name, file->m_extension))
			return NULL;
	}
	
	/* Try to allocate memory for new song */
	song = (song_t *)malloc(sizeof(song_t));
	if (song == NULL)
		return NULL;
	memset(song, 0, sizeof(*song));

	/* Set song fields */
	song->m_ref_count = 0;
	song->m_full_name = strdup(file->m_full_name);
	song->m_file_name = song->m_full_name + (file->m_name - file->m_full_name);
	song->m_short_name = song->m_full_name +
		(file->m_short_name - file->m_full_name);
	song->m_file_ext = song->m_full_name + 
		(file->m_extension - file->m_full_name);
	song->m_info = NULL;
	song->m_flags = 0;
	song->m_len = len;
	song->m_title = NULL;
	song->m_start_time = song->m_end_time = -1;
	pthread_mutex_init(&song->m_mutex, NULL);
	if (title == NULL)
		song_update_title(song);
	else
	{
		song->m_title = str_new(title);
		song->m_default_title = strdup(title);
	}
	return song_add_ref(song);
} /* End of 'song_new' function */

/* Add a reference to the song object */
song_t *song_add_ref( song_t *song )
{
	assert(song);
	assert(song->m_ref_count >= 0);
	song->m_ref_count ++;
	return song;
} /* End of 'song_add_ref' function */

/* Free song */
void song_free( song_t *song )
{
	assert(song);
	assert(song->m_ref_count > 0);

	/* Release reference */
	song->m_ref_count --;

	/* Free object */
	if (song->m_ref_count == 0)
	{
		str_free(song->m_title);
		si_free(song->m_info);
		free(song->m_full_name);
		if (song->m_default_title != NULL)
			free(song->m_default_title);
		pthread_mutex_destroy(&song->m_mutex);
		free(song);
	}
} /* End of 'song_free' function */

/* Update song information */
void song_update_info( song_t *song )
{
	if (song == NULL || (song->m_flags & SONG_INFO_WRITE))
	{
		return;
	}

	song_lock(song);

	song_info_t *new_info = inp_get_info(song->m_inp, song->m_file_name, 
			&song->m_len);
	if (!(song->m_flags & SONG_STATIC_INFO))
	{
		si_free(song->m_info);
		song->m_info = new_info;
	}
	else if (new_info)
		si_free(new_info);

	if (song->m_start_time > -1)
	{
		song->m_len = (song->m_end_time > -1) ? 
			(song->m_end_time - song->m_start_time) : 
				(song->m_len - song->m_start_time);
	}

	song_update_title(song);
	song->m_flags &= (~SONG_INFO_READ);
	song_unlock(song);
} /* End of 'song_update_info' function */

/* Fill song title from data from song info and other parameters */
void song_update_title( song_t *song )
{
	char *fmt;
	str_t *str;
	bool_t finish = FALSE;
	song_info_t *info;

	if (song == NULL || song->m_default_title != NULL)
		return;

	/* Free current title */
	str_free(song->m_title);
	
	/* Case that we have no info */
	info = song->m_info;
	if (info == NULL || !(info->m_flags & SI_INITIALIZED) ||
			(info->m_flags & SI_ONLY_OWN))
	{
		song->m_title = inp_set_song_title(song->m_inp, song->m_file_name);
		if (cfg_get_var_int(cfg_list, "convert-underscores2spaces"))
			str_replace_char(song->m_title, '_', ' ');
		return;
	}

	/* Use specified title format */
	fmt = cfg_get_var(cfg_list, "title-format");
	str = song->m_title = str_new("");
	if (fmt != NULL && (*fmt != 0))
	{
		for ( ; *fmt && !finish; fmt ++ )
		{
			char *g;
			
			if (*fmt == '%')
			{
				fmt ++;
				switch (*fmt)
				{
				case 'p':
					str_cat_cptr(str, info->m_artist);
					break;
				case 'a':
					str_cat_cptr(str, info->m_album);
					break;
				case 'f':
					str_cat_cptr(str, song->m_short_name);
					break;
				case 'F':
					str_cat_cptr(str, song->m_file_name);
					break;
				case 'e':
					str_cat_cptr(str, song->m_file_ext);
					break;
				case 't':
					str_cat_cptr(str, info->m_name);
					break;
				case 'n':
					str_cat_cptr(str, info->m_track);
					break;
				case 'y':
					str_cat_cptr(str, info->m_year);
					break;
				case 'g':
					str_cat_cptr(str, info->m_genre);
					break;
				case 'c':
					str_cat_cptr(str, info->m_comments);
					break;
				case 0:
					finish = TRUE;
					break;
				}
			}
			else
			{
				str_insert_char(str, *fmt, str->m_len);
			}
		}
	}
	else
	{
		str_printf(str, "%s - %s", info->m_artist, info->m_name);
	}
} /* End of 'song_get_title_from_info' function */

/* Write song info */
void song_write_info( song_t *s )
{
	if (!inp_save_info(s->m_inp, s->m_file_name, s->m_info))
	{
		song_update_info(s);
		logger_error(player_log, 0, _("Failed to save info to file %s"),
				s->m_file_name);
	}
	s->m_flags &= ~(SONG_INFO_READ | SONG_INFO_WRITE);
} /* End of 'song_write_info' function */

/* Get input plugin */
in_plugin_t *song_get_inp( song_t *song, file_t **fd )
{
	/* Do nothing if we already no plugin */
	if (fd != NULL)
		(*fd) = NULL;
	if (song->m_inp != NULL)
		return song->m_inp;

	/* Choose appropriate input plugin */
	if (*song->m_file_ext)
		song->m_inp = pmng_search_format(player_pmng, song->m_file_name, song->m_file_ext);
	if (song->m_inp == NULL)
	{
		file_t *cfd = file_open(song->m_file_name, "rb", player_log);
		if (cfd == NULL)
			return NULL;
		song->m_inp = pmng_search_content_type(player_pmng,
				file_get_content_type(cfd));
		if (fd != NULL)
			(*fd) = cfd;
		else
			file_close(cfd);
	}
	return song->m_inp;
} /* End of 'song_get_inp' function */

/* End of 'song.c' file */

