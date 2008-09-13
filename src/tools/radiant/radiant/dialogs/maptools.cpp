/**
 * @file maptools.cpp
 * @brief Creates the maptools dialogs - like checking for errors and compiling
 * a map
 */

/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gtk/gtk.h>
#include "maptools.h"
#include "cmdlib.h"   // Q_Exec
#include "os/file.h"  // file_exists
#include "os/path.h"  // path_get_filename_start
#include "scenelib.h" // g_brushCount
#include "gtkutil/messagebox.h"  // gtk_MessageBox
#include "stream/stringstream.h"
#include "../qe3.h" // g_brushCount
#include "../map.h"
#include "../preferences.h"
#include "../mainframe.h"
#ifdef WIN32
#include "../gtkmisc.h"
#endif

enum {
	CHECK_ENTITY,
	CHECK_BRUSH,
	CHECK_MESSAGE,
	CHECK_SELECT,

	CHECK_COLUMNS
};

// master window widget
static GtkWidget *checkDialog = NULL;
static GtkWidget *treeViewWidget; // slave, text widget from the gtk editor

static gint editorHideCallback (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(checkDialog);
	return 1;
}

static gint fixCallback (GtkWidget *widget, gpointer data)
{
	const char* fullname = Map_Name(g_map);
	const char *compilerBinaryWithPath;

	if (!ConfirmModified("Check Map"))
		return 0;

	/* empty map? */
	if (!g_brushCount.get()) {
		gtk_MessageBox(0, "Nothing to fix in this map\n", "Map fixing", eMB_OK, eMB_ICONERROR);
		return 0;
	}

	compilerBinaryWithPath = CompilerBinaryWithPath_get();

	if (file_exists(compilerBinaryWithPath)) {
		char bufParam[1024];
		char bufPath[1024];
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_fix");

		// attach parameter and map name
		snprintf(bufParam, sizeof(bufParam) - 1, "%s %s", compiler_parameter, fullname);
		bufParam[sizeof(bufParam) - 1] = '\0';

		// extract path to get a working dir
		const char *mapcompiler = path_get_filename_start(compilerBinaryWithPath);
		strncpy(bufPath, compilerBinaryWithPath, mapcompiler - compilerBinaryWithPath);
		bufPath[mapcompiler - compilerBinaryWithPath] = '\0';

		char* output = Q_Exec(mapcompiler, bufParam, bufPath, false);
		if (output) {
			// reload after fix
			Map_Reload();
			// refill popup
			ToolsCheckErrors();
			globalOutputStream() << "-------------------\n" << output << "-------------------\n";
			free(output);
		} else {
			globalOutputStream() << "-------------------\nCompiler: " << mapcompiler << "\nParameter: " << bufParam << "\nWorking dir: " << bufPath << "\n-------------------\n";
			return 0;
		}
	}
	return 1;
}

static void selectBrushesViaTreeView (GtkCellRendererToggle *widget, gchar *path, GtkWidget *ignore)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeViewWidget));
	gboolean enabled = TRUE;
	int entnum, brushnum;

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	// get the values from the tree view model list
	gtk_tree_model_get(model, &iter, CHECK_SELECT, &enabled, -1);
	gtk_tree_model_get(model, &iter, CHECK_ENTITY, &entnum, -1);
	gtk_tree_model_get(model, &iter, CHECK_BRUSH, &brushnum, -1);

	// correct brush and ent values
	if (brushnum < 0)
		brushnum = 0;
	if (entnum < 0)
		entnum = 0;

	// the the checkbox value
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, CHECK_SELECT, !enabled, -1);
	// and now do the real selection
	SelectBrush(entnum, brushnum, !enabled);
}


static void CreateCheckDialog (void)
{
	GtkWidget *vbox, *hbox, *button;

	checkDialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(checkDialog), "delete_event", G_CALLBACK(editorHideCallback), NULL);
	gtk_window_set_default_size(GTK_WINDOW(checkDialog), 600, 300);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(checkDialog), GTK_WIDGET(vbox));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn * column;
		GtkListStore *store;
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

		treeViewWidget = gtk_tree_view_new();

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes("Entity", renderer, "text", CHECK_ENTITY, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeViewWidget), column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes("Brush", renderer, "text", CHECK_BRUSH, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeViewWidget), column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes("Message", renderer, "text", CHECK_MESSAGE, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeViewWidget), column);

		renderer = gtk_cell_renderer_toggle_new();
		g_object_set(renderer, "activatable", TRUE, NULL);
		g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(selectBrushesViaTreeView), NULL);
		column = gtk_tree_view_column_new_with_attributes("Select", renderer, "active", CHECK_SELECT, NULL);
		gtk_tree_view_column_set_alignment(column, 0.5);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeViewWidget), column);

		gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(treeViewWidget));
		gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(scr));

		store = gtk_list_store_new(CHECK_COLUMNS, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN);
		gtk_tree_view_set_model(GTK_TREE_VIEW(treeViewWidget), GTK_TREE_MODEL(store));
		/* unreference the list so that is will be deleted along with the tree view */
		g_object_unref(store);
	}

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

	button = gtk_button_new_with_label("Close");
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(editorHideCallback), NULL);
	gtk_widget_set_usize(button, 60, -2);

	button = gtk_button_new_with_label("Fix");
	gtk_widget_set_tooltip_text(button, "Will fix all errors, not only the selected ones");
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(fixCallback), NULL);
	gtk_widget_set_usize(button, 60, -2);
}

void ToolsCheckErrors (void)
{
	const char* fullname = Map_Name(g_map);
	const char *compilerBinaryWithPath;

	if (!ConfirmModified("Check Map"))
		return;

	/* empty map? */
	if (!g_brushCount.get()) {
		gtk_MessageBox(0, "Nothing to check in this map\n", "Map compiling", eMB_OK, eMB_ICONERROR);
		return;
	}

	compilerBinaryWithPath = CompilerBinaryWithPath_get();

	if (file_exists(compilerBinaryWithPath)) {
		char bufParam[1024];
		char bufPath[1024];
		int rows = 0;
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_check");

		snprintf(bufParam, sizeof(bufParam) - 1, "%s %s", compiler_parameter, fullname);
		bufParam[sizeof(bufParam) - 1] = '\0';

		// extract path to get a working dir
		const char *mapcompiler = path_get_filename_start(compilerBinaryWithPath);
		strncpy(bufPath, compilerBinaryWithPath, mapcompiler - compilerBinaryWithPath);
		bufPath[mapcompiler - compilerBinaryWithPath] = '\0';

		char* output = Q_Exec(mapcompiler, bufParam, bufPath, false);
		if (output) {
			if (!checkDialog)
				CreateCheckDialog();

			gtk_window_set_title(GTK_WINDOW(checkDialog), "Check output");

			StringTokeniser outputTokeniser(output, "\n");

			GtkTreeIter iter;
			GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeViewWidget)));
			/* start with a fresh list */
			gtk_list_store_clear(store);

			do {
				char entnumbuf[32] = "";
				char brushnumbuf[32] = "";
				const char *line = outputTokeniser.getToken();
				if (string_empty(line))
					break;

				while (*line) {
					if (!strncmp(line, "ent:", 4)) {
						char *bufPos;

						line += 4;

						bufPos = entnumbuf;
						while (*line == '-' || *line == '+' || (*line >= '0' && *line <= '9'))
							*bufPos++ = *line++;
						*bufPos = '\0';

						// the next should be a brush
						if (!strncmp(line, " brush:", 7)) {
							const char *color;

							line += 7;

							bufPos = brushnumbuf;
							while (*line == '-' || *line == '+' || (*line >= '0' && *line <= '9'))
								*bufPos++ = *line++;
							*bufPos = '\0';

							// skip seperator
							line += 3;
							if (*line == '*') {
								// automatically fixable
								color = "#000000";
							} else {
								// show red - manually
								color = "#ff0000";
							}

							gtk_list_store_append(store, &iter);
//							gtk_list_store_set(store, &iter, CHECK_ENTITY, atoi(entnumbuf), CHECK_BRUSH, atoi(brushnumbuf), CHECK_MESSAGE, line, CHECK_SELECT, NULL, -1);
							gtk_list_store_set(store, &iter, CHECK_ENTITY, atoi(entnumbuf), CHECK_BRUSH, atoi(brushnumbuf), CHECK_MESSAGE, line, -1);

							rows++;
						} else
							// no valid check output line
							break;
					}
					line++;
				}
			} while (1);

			if (rows == 0) {
				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, CHECK_ENTITY, "", CHECK_BRUSH, "", CHECK_MESSAGE, "No problems found", CHECK_SELECT, NULL, -1);
			}

			/* trying to show later */
			gtk_widget_show_all(checkDialog);

#ifdef WIN32
			process_gui();
#endif

			free(output);
		} else {
			globalOutputStream() << "-------------------\nCompiler: " << mapcompiler << "\nParameter: " << bufParam << "\nWorking dir: " << bufPath << "\n-------------------\n";
			globalOutputStream() << "No output for checking " << fullname << "\n";
		}
	} else {
		StringOutputStream message(256);
		message << "Could not find the mapcompiler (" << compilerBinaryWithPath << ") check your path settings\n";
		gtk_MessageBox(0, message.c_str(), "Map compiling", eMB_OK, eMB_ICONERROR);
	}
}

/**
 * @todo Implement me
 */
void ToolsCompile (void)
{
	const char *compilerBinaryWithPath;

	if (!ConfirmModified("Compile Map"))
		return;

	/* empty map? */
	if (!g_brushCount.get()) {
		gtk_MessageBox(0, "Nothing to compile in this map\n", "Map compiling", eMB_OK, eMB_ICONERROR);
		return;
	}

	compilerBinaryWithPath = CompilerBinaryWithPath_get();

	if (file_exists(compilerBinaryWithPath)) {
		char bufParam[1024];
		char bufPath[1024];
		const char* fullname = Map_Name(g_map);
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_compile");

		snprintf(bufParam, sizeof(bufParam) - 1, "%s %s", compiler_parameter, fullname);
		bufParam[sizeof(bufParam) - 1] = '\0';

		// extract path to get a working dir
		const char *mapcompiler = path_get_filename_start(compilerBinaryWithPath);
		strncpy(bufPath, compilerBinaryWithPath, mapcompiler - compilerBinaryWithPath);
		bufPath[mapcompiler - compilerBinaryWithPath] = '\0';

		/** @todo thread this and update the main window */
		char* output = Q_Exec(mapcompiler, bufParam, bufPath, false);
		if (output) {
			/** @todo parse and display this in a gtk window */
			globalOutputStream() << output;
			free(output);
		} else
			globalOutputStream() << "-------------------\nCompiler: " << mapcompiler << "\nParameter: " << bufParam << "\nWorking dir: " << bufPath << "\n-------------------\n";
	}
}
