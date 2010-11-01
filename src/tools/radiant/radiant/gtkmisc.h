/*
 Copyright (c) 2001, Loki software, inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list
 of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of Loki software nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific prior
 written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(INCLUDED_GTKMISC_H)
#define INCLUDED_GTKMISC_H

#include <string>
#include <gtk/gtkmain.h>
#include "radiant_i18n.h"

inline void process_gui ()
{
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
}

typedef struct _GtkMenu GtkMenu;
typedef struct _GtkMenuItem GtkMenuItem;
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;

// greebo: This is the new function to add a menu item to the given <menu> and connect it to the passed <commandName>
GtkMenuItem* createMenuItemWithMnemonic(GtkMenu* menu, const std::string& caption, const std::string& commandName);
GtkMenuItem* createCheckMenuItemWithMnemonic(GtkMenu* menu, const std::string& caption, const std::string& commandName);
GtkMenuItem* createSeparatorMenuItem(GtkMenu* menu);

typedef struct _GtkButton GtkButton;
typedef struct _GtkToggleButton GtkToggleButton;
typedef struct _GtkToolbar GtkToolbar;

template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;
bool color_dialog (GtkWidget *parent, Vector3& color, const std::string& title = _("Choose Color"));

typedef struct _GtkEntry GtkEntry;
void button_clicked_entry_browse_file (GtkWidget* widget, GtkEntry* entry);
void button_clicked_entry_browse_directory (GtkWidget* widget, GtkEntry* entry);

#endif
