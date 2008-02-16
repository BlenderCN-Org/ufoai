/**
 * @file m_messages.c
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

#include "../client.h"
#include "../cl_global.h"
#include "m_main.h"
#include "m_messages.h"
#include "m_popup.h"

/**
 * @brief Script command to show all messages on the stack
 */
static void CL_ShowMessagesOnStack_f (void)
{
	message_t *m = mn.messageStack;

	while (m) {
		Com_Printf("%s: %s\n", m->title, m->text);
		m = m->next;
	}
}

/**
 * @brief Adds a new message to message stack
 * @note These are the messages that are displayed at geoscape
 * @param[in] title Already translated message/mail title
 * @param[in] text Already translated message/mail body
 * @param[in] popup Show this as a popup, too?
 * @param[in] type The message type
 * @param[in] pedia Pointer to technology (only if needed)
 * @return message_t pointer
 * @sa UP_OpenMail_f
 * @sa CL_EventAddMail_f
 */
message_t *MN_AddNewMessage (const char *title, const char *text, qboolean popup, messagetype_t type, void *pedia)
{
	message_t *mess;

	assert(type < MSG_MAX);

	/* allocate memory for new message - delete this with every new game */
	mess = (message_t *) Mem_PoolAlloc(sizeof(message_t), cl_localPool, CL_TAG_NONE);

	/* push the new message at the beginning of the stack */
	mess->next = mn.messageStack;
	mn.messageStack = mess;

	mess->type = type;
	mess->pedia = pedia;		/* pointer to UFOpaedia entry */

	CL_DateConvert(&ccs.date, &mess->d, &mess->m);
	mess->y = ccs.date.day / 365;
	mess->h = ccs.date.sec / 3600;
	mess->min = ((ccs.date.sec % 3600) / 60);
	mess->s = (ccs.date.sec % 3600) / 60 / 60;

	Q_strncpyz(mess->title, title, sizeof(mess->title));
	mess->text = Mem_PoolStrDup(text, cl_localPool, CL_TAG_NONE);

	/* they need to be translated already */
	if (popup)
		MN_Popup(mess->title, mess->text);

	switch (type) {
	case MSG_DEBUG:
	case MSG_INFO:
	case MSG_STANDARD:
	case MSG_TRANSFERFINISHED:
	case MSG_PROMOTION:
	case MSG_DEATH:
		break;
	case MSG_RESEARCH_PROPOSAL:
	case MSG_RESEARCH_FINISHED:
		assert(pedia);
	case MSG_EVENT:
	case MSG_NEWS:
		/* reread the new mails in UP_GetUnreadMails */
		gd.numUnreadMails = -1;
		/*S_StartLocalSound("misc/mail");*/
		break;
	case MSG_UFOSPOTTED:
	case MSG_TERRORSITE:
	case MSG_BASEATTACK:
	case MSG_CRASHSITE:
		S_StartLocalSound("misc/newmission");
		break;
	case MSG_CONSTRUCTION:
		break;
	case MSG_PRODUCTION:
		break;
	case MSG_MAX:
		break;
	}

	return mess;
}

/**
 * @brief Returns formatted text of a message timestamp
 * @param[in] text Buffer to hold the final result
 * @param[in] message The message to convert into text
 */
void MN_TimestampedText (char *text, message_t *message, size_t textsize)
{
	Com_sprintf(text, textsize, _("%i %s %02i, %02i:%02i: "), message->y, CL_DateGetMonthName(message->m), message->d, message->h, message->min);
}

void MN_RemoveMessage (char *title)
{
	message_t *m = mn.messageStack;
	message_t *prev = NULL;

	while (m) {
		if (!Q_strncmp(m->title, title, MAX_VAR)) {
			if (prev)
				prev->next = m->next;
			Mem_Free(m->text);
			Mem_Free(m);
			return;
		}
		prev = m;
		m = m->next;
	}
	Com_Printf("Could not remove message from stack - %s was not found\n", title);
}

/**< @brief buffer that hold all printed chat messages for menu display */
static char *chatBuffer = NULL;
static menuNode_t* chatBufferNode = NULL;

/**
 * @brief Displays a chat on the hud and add it to the chat buffer
 */
void MN_AddChatMessage (const char *text)
{
	/* allocate memory for new chat message */
	chatMessage_t *chat = (chatMessage_t *) Mem_PoolAlloc(sizeof(chatMessage_t), cl_genericPool, CL_TAG_NONE);

	/* push the new chat message at the beginning of the stack */
	chat->next = mn.chatMessageStack;
	mn.chatMessageStack = chat;
	chat->text = Mem_PoolStrDup(text, cl_genericPool, CL_TAG_NONE);

	if (!chatBuffer) {
		chatBuffer = (char*)Mem_PoolAlloc(sizeof(char) * MAX_MESSAGE_TEXT, cl_genericPool, CL_TAG_NONE);
		if (!chatBuffer) {
			Com_Printf("Could not allocate chat buffer\n");
			return;
		}
		/* only link this once */
		mn.menuText[TEXT_CHAT_WINDOW] = chatBuffer;
	}
	if (!chatBufferNode) {
		const menu_t* menu = MN_GetMenu(mn_hud->string);
		if (!menu)
			Sys_Error("Could not get hud menu: %s\n", mn_hud->string);
		chatBufferNode = MN_GetNode(menu, "chatscreen");
	}

	*chatBuffer = '\0'; /* clear buffer */
	do {
		if (strlen(chatBuffer) + strlen(chat->text) >= MAX_MESSAGE_TEXT)
			break;
		Q_strcat(chatBuffer, chat->text, MAX_MESSAGE_TEXT); /* fill buffer */
		chat = chat->next;
	} while (chat);

	/* maybe the hud doesn't have a chatscreen node - or we don't have a hud */
	if (chatBufferNode) {
		Cmd_ExecuteString("unhide_chatscreen");
		chatBufferNode->menu->eventTime = cls.realtime;
	}
}

/**
 * @brief Script command to show all chat messages on the stack
 */
static void CL_ShowChatMessagesOnStack_f (void)
{
	chatMessage_t *m = mn.chatMessageStack;

	while (m) {
		Com_Printf("%s", m->text);
		m = m->next;
	}
}

/**
 * @brief Saved the complete message stack
 * @sa SAV_GameSave
 * @sa MN_AddNewMessage
 */
static void MS_MessageSave (sizebuf_t * sb, message_t * message)
{
	int idx = -1;

	if (!message)
		return;
	/* bottom up */
	MS_MessageSave(sb, message->next);

	/* don't save these message types */
	if (message->type == MSG_INFO)
		return;

	if (message->pedia)
		idx = message->pedia->idx;

	Com_DPrintf(DEBUG_CLIENT, "MS_MessageSave: Save '%s' - '%s'; type = %i; idx = %i\n", message->title, message->text, message->type, idx);
	MSG_WriteString(sb, message->title);
	MSG_WriteString(sb, message->text);
	MSG_WriteByte(sb, message->type);
	/* store script id of event mail */
	if (message->type == MSG_EVENT) {
		MSG_WriteString(sb, message->eventMail->id);
		MSG_WriteByte(sb, message->eventMail->read);
	}
	MSG_WriteLong(sb, idx);
	MSG_WriteLong(sb, message->d);
	MSG_WriteLong(sb, message->m);
	MSG_WriteLong(sb, message->y);
	MSG_WriteLong(sb, message->h);
	MSG_WriteLong(sb, message->min);
	MSG_WriteLong(sb, message->s);
}

/**
 * @sa MS_Load
 * @sa MN_AddNewMessage
 * @sa MS_MessageSave
 */
qboolean MS_Save (sizebuf_t* sb, void* data)
{
	int i = 0;
	message_t* message;

	/* store message system items */
	for (message = mn.messageStack; message; message = message->next) {
		if (message->type == MSG_INFO)
			continue;
		i++;
	}
	MSG_WriteLong(sb, i);
	MS_MessageSave(sb, mn.messageStack);
	return qtrue;
}

/**
 * @sa MS_Save
 * @sa MN_AddNewMessage
 */
qboolean MS_Load (sizebuf_t* sb, void* data)
{
	int i, mtype, idx;
	char title[MAX_VAR], text[MAX_MESSAGE_TEXT];
	message_t *mess;
	eventMail_t *mail;
	qboolean read;

	/* how many message items */
	i = MSG_ReadLong(sb);
	for (; i--;) {
		mail = NULL;
		/* can contain high bits due to utf8 */
		Q_strncpyz(title, MSG_ReadStringRaw(sb), sizeof(title));
		Q_strncpyz(text, MSG_ReadStringRaw(sb), sizeof(text));
		mtype = MSG_ReadByte(sb);
		if (mtype == MSG_EVENT) {
			mail = CL_GetEventMail(MSG_ReadString(sb), qfalse);
			read = MSG_ReadByte(sb);
			if (mail)
				mail->read = read;
		} else
			mail = NULL;
		idx = MSG_ReadLong(sb);
		/* event and not mail means, dynamic mail - we don't save or load them */
		if ((mtype == MSG_EVENT && !mail) || (mtype == MSG_DEBUG && developer->integer != 1)) {
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
			MSG_ReadLong(sb);
		} else {
			mess = MN_AddNewMessage(title, text, qfalse, mtype, RS_GetTechByIDX(idx));
			mess->eventMail = mail;
			mess->d = MSG_ReadLong(sb);
			mess->m = MSG_ReadLong(sb);
			mess->y = MSG_ReadLong(sb);
			mess->h = MSG_ReadLong(sb);
			mess->min = MSG_ReadLong(sb);
			mess->s = MSG_ReadLong(sb);
		}
	}
	return qtrue;
}

void MN_MessageInit (void)
{
	Cmd_AddCommand("chatlist", CL_ShowChatMessagesOnStack_f, "Print all chat messages to the game console");
	Cmd_AddCommand("messagelist", CL_ShowMessagesOnStack_f, "Print all messages to the game console");
}
