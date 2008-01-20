/**
 * @file m_messages.h
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CLIENT_MENU_M_MESSAGES_H
#define CLIENT_MENU_M_MESSAGES_H

#include "../../game/q_shared.h"

/* message systems */
typedef enum {
	MSG_DEBUG,			/**< only save them in debug mode */
	MSG_INFO,			/**< don't save these messages */
	MSG_STANDARD,
	MSG_RESEARCH_PROPOSAL,
	MSG_RESEARCH_FINISHED,
	MSG_CONSTRUCTION,
	MSG_UFOSPOTTED,
	MSG_TERRORSITE,
	MSG_BASEATTACK,
	MSG_TRANSFERFINISHED,
	MSG_PROMOTION,
	MSG_PRODUCTION,
	MSG_NEWS,
	MSG_DEATH,
	MSG_CRASHSITE,
	MSG_EVENT,

	MSG_MAX
} messagetype_t;

/** @brief also used for chat message buffer */
#define MAX_MESSAGE_TEXT 1024
#define TIMESTAMP_TEXT 21
typedef struct message_s {
	char title[MAX_VAR];
	char *text;
	messagetype_t type;
	struct technology_s *pedia;		/**< link to ufopedia if a research has finished. */
	struct eventMail_s *eventMail;
	struct message_s *next;
	int d, m, y, h, min, s;
} message_t;

/** @brief Stores all chat messages from a multiplayer game */
typedef struct chatMessage_s {
	char *text;
	struct chatMessage_s *next;
} chatMessage_t;

message_t *MN_AddNewMessage(const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia);
void MN_RemoveMessage(char *title);
void MN_AddChatMessage(const char *text);
void MN_TimestampedText(char *text, message_t *message, size_t textsize);

void MN_MessageInit(void);

#endif
