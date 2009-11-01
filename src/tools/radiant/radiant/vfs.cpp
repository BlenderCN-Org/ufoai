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

//
// Rules:
//
// - Directories should be searched in the following order: ~/.ufoai/<version>/base,
//   install dir (/usr/local/games/ufoai/base)
//
// - Pak files are searched first inside the directories.
// - Case insensitive.
// - Unix-style slashes (/) (windows is backwards .. everyone knows that)
//
// Leonardo Zide (leo@lokigames.com)
//

#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <glib/gslist.h>
#include <glib/gdir.h>
#include <glib/gstrfuncs.h>

#include "AutoPtr.h"
#include "iradiant.h"
#include "idatastream.h"
#include "iarchive.h"
ArchiveModules& FileSystemAPI_getArchiveModules ();
#include "ifilesystem.h"

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "moduleobservers.h"

#define VFS_MAXDIRS 8

// =============================================================================
// Global variables

Archive* OpenDirArchive (const std::string& name);

struct archive_entry_t
{
		std::string name;
		Archive* archive;
		bool is_pakfile;
};

#include <list>

typedef std::list<archive_entry_t> archives_t;

static archives_t g_archives;
static char g_strDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numDirs;
static bool g_bUsePak = true;

static ModuleObservers g_observers;

// =============================================================================
// Static functions

/** @todo Use DirCleaned */
static void AddSlash (char *str)
{
	std::size_t n = strlen(str);
	if (n > 0) {
		if (str[n - 1] != '\\' && str[n - 1] != '/') {
			globalWarningStream() << "Directory path does not end with separator: " << str << "\n";
			strcat(str, "/");
		}
	}
}

/** @todo Use os::standardPath */
static void FixDOSName (char *src)
{
	if (src == 0 || strchr(src, '\\') == 0)
		return;

	globalWarningStream() << "Invalid path separator '\\': " << src << "\n";

	while (*src) {
		if (*src == '\\')
			*src = '/';
		src++;
	}
}

const _QERArchiveTable* GetArchiveTable (ArchiveModules& archiveModules, const std::string& ext)
{
	return archiveModules.findModule(string::toLower(ext).c_str());
}

static void InitPK3File (ArchiveModules& archiveModules, const std::string& filename)
{
	const _QERArchiveTable* table = GetArchiveTable(archiveModules, os::getExtension(filename));

	if (table != 0) {
		archive_entry_t entry;
		entry.name = filename;

		entry.is_pakfile = true;
		entry.archive = table->m_pfnOpenArchive(filename.c_str());
		g_archives.push_back(entry);
		g_message("  pk3 file: %s\n", filename.c_str());
	}
}

inline void pathlist_prepend_unique (GSList*& pathlist, char* path)
{
	if (g_slist_find_custom(pathlist, path, (GCompareFunc) path_compare) == 0) {
		pathlist = g_slist_prepend(pathlist, path);
	} else {
		g_free(path);
	}
}

class DirectoryListVisitor: public Archive::Visitor
{
		GSList*& m_matches;
		const char* m_directory;
	public:
		DirectoryListVisitor (GSList*& matches, const char* directory) :
			m_matches(matches), m_directory(directory)
		{
		}
		void visit (const char* name)
		{
			const char* subname = path_make_relative(name, m_directory);
			if (subname != name) {
				if (subname[0] == '/')
					++subname;
				char* dir = g_strdup(subname);
				char* last_char = dir + strlen(dir);
				if (last_char != dir && *(--last_char) == '/')
					*last_char = '\0';
				pathlist_prepend_unique(m_matches, dir);
			}
		}
};

class FileListVisitor: public Archive::Visitor
{
		GSList*& m_matches;
		const char* m_directory;
		const char* m_extension;
	public:
		FileListVisitor (GSList*& matches, const char* directory, const char* extension) :
			m_matches(matches), m_directory(directory), m_extension(extension)
		{
		}
		void visit (const char* name)
		{
			const char* subname = path_make_relative(name, m_directory);
			if (subname != name) {
				if (subname[0] == '/')
					++subname;
				if (m_extension[0] == '*' || extension_equal(os::getExtension(subname).c_str(), m_extension))
					pathlist_prepend_unique(m_matches, g_strdup(subname));
			}
		}
};

static GSList* GetListInternal (const char *refdir, const char *ext, bool directories, std::size_t depth)
{
	GSList* files = 0;

	ASSERT_MESSAGE(refdir[strlen(refdir) - 1] == '/', "search path does not end in '/'");

	if (directories) {
		for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			DirectoryListVisitor visitor(files, refdir);
			(*i).archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eDirectories, depth), refdir);
		}
	} else {
		for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			FileListVisitor visitor(files, refdir, ext);
			(*i).archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eFiles, depth), refdir);
		}
	}

	files = g_slist_reverse(files);

	return files;
}

inline int ascii_to_upper (int c)
{
	if (c >= 'a' && c <= 'z') {
		return c - ('a' - 'A');
	}
	return c;
}

/*!
 This behaves identically to stricmp(a,b), except that ASCII chars
 [\]^`_ come AFTER alphabet chars instead of before. This is because
 it converts all alphabet chars to uppercase before comparison,
 while stricmp converts them to lowercase.
 */
static int string_compare_nocase_upper (const char* a, const char* b)
{
	for (;;) {
		int c1 = ascii_to_upper(*a++);
		int c2 = ascii_to_upper(*b++);

		if (c1 < c2) {
			return -1; // a < b
		}
		if (c1 > c2) {
			return 1; // a > b
		}
		if (c1 == 0) {
			return 0; // a == b
		}
	}
}

/**
 * @note sort pakfiles in reverse order. This ensures that
 * later pakfiles override earlier ones. This because the vfs module
 * returns a filehandle to the first file it can find (while it should
 * return the filehandle to the file in the most overriding pakfile, the
 * last one in the list that is).
 */
class PakLess
{
	public:
		bool operator() (const CopiedString& self, const CopiedString& other) const
		{
			return string_compare_nocase_upper(self.c_str(), other.c_str()) > 0;
		}
};

typedef std::set<CopiedString, PakLess> Archives;

// =============================================================================
// Global functions

/** @brief reads all pak files from a dir */
void InitDirectory (const std::string& directory, ArchiveModules& archiveModules)
{
	if (g_numDirs == (VFS_MAXDIRS - 1))
		return;

	strncpy(g_strDirs[g_numDirs], directory.c_str(), PATH_MAX);
	g_strDirs[g_numDirs][PATH_MAX] = '\0';
	FixDOSName(g_strDirs[g_numDirs]);
	AddSlash(g_strDirs[g_numDirs]);

	const char* path = g_strDirs[g_numDirs];

	g_numDirs++;

	{
		archive_entry_t entry;
		entry.name = path;
		entry.archive = OpenDirArchive(path);
		entry.is_pakfile = false;
		g_archives.push_back(entry);
	}

	if (g_bUsePak) {
		GDir* dir = g_dir_open(path, 0, 0);

		if (dir != 0) {
			g_message("vfs directory: %s\n", path);

			Archives archives;
			Archives archivesOverride;
			for (;;) {
				const char* name = g_dir_read_name(dir);
				if (name == 0)
					break;

				const char *ext = strrchr(name, '.');
				if ((ext == 0) || *(++ext) == '\0' || GetArchiveTable(archiveModules, ext) == 0)
					continue;

				archives.insert(name);
			}

			g_dir_close(dir);

			// add the entries to the vfs
			for (Archives::iterator i = archivesOverride.begin(); i != archivesOverride.end(); ++i) {
				char filename[PATH_MAX];
				strcpy(filename, path);
				strcat(filename, (*i).c_str());
				InitPK3File(archiveModules, filename);
			}
			for (Archives::iterator i = archives.begin(); i != archives.end(); ++i) {
				char filename[PATH_MAX];
				strcpy(filename, path);
				strcat(filename, (*i).c_str());
				InitPK3File(archiveModules, filename);
			}
		} else {
			g_message("vfs directory not found: '%s'\n", path);
		}
	}
}

/** @brief frees all memory that we allocated */
void Shutdown ()
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		delete i->archive;
	}
	g_archives.clear();

	g_numDirs = 0;
}

#define VFS_SEARCH_PAK 0x1
#define VFS_SEARCH_DIR 0x2

int GetFileCount (const char *filename, int flag)
{
	int count = 0;
	char fixed[PATH_MAX + 1];

	strncpy(fixed, filename, PATH_MAX);
	fixed[PATH_MAX] = '\0';
	FixDOSName(fixed);

	if (!flag)
		flag = (VFS_SEARCH_PAK | VFS_SEARCH_DIR);

	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (((*i).is_pakfile && (flag & VFS_SEARCH_PAK) != 0) || (!(*i).is_pakfile && (flag & VFS_SEARCH_DIR) != 0)) {
			if ((*i).archive->containsFile(fixed))
				++count;
		}
	}

	return count;
}

ArchiveFile* OpenFile (const std::string& filename)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveFile* file = (*i).archive->openFile(filename.c_str());
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

ArchiveTextFile* OpenTextFile (const std::string& filename)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveTextFile* file = (*i).archive->openTextFile(filename.c_str());
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

/** @note when loading a file, you have to allocate one extra byte and set it to \0 */
std::size_t LoadFile (const std::string& filename, void **bufferptr, int index)
{
	char fixed[PATH_MAX + 1];

	strncpy(fixed, filename.c_str(), PATH_MAX);
	fixed[PATH_MAX] = '\0';
	FixDOSName(fixed);

	AutoPtr<ArchiveFile> file(OpenFile(fixed));
	if (file) {
		*bufferptr = malloc(file->size() + 1);
		// we need to end the buffer with a 0
		((char*) (*bufferptr))[file->size()] = 0;

		std::size_t length = file->getInputStream().read((InputStream::byte_type*) *bufferptr, file->size());
		return length;
	}

	*bufferptr = 0;
	return 0;
}

void FreeFile (void *p)
{
	free(p);
}

void ClearFileDirList (GSList **lst)
{
	while (*lst) {
		g_free((*lst)->data);
		*lst = g_slist_remove(*lst, (*lst)->data);
	}
}

const std::string FindFile (const std::string& relative)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if ((*i).archive->containsFile(relative)) {
			return (*i).name;
		}
	}

	return "";
}

const std::string FindPath (const std::string& absolute)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (path_equal_n(absolute.c_str(), (*i).name.c_str(), string_length((*i).name.c_str()))) {
			return (*i).name;
		}
	}

	return "";
}

class UFOFileSystem: public VirtualFileSystem
{
	public:
		void initDirectory (const std::string& path)
		{
			InitDirectory(path, FileSystemAPI_getArchiveModules());
		}
		void initialise ()
		{
			g_message("filesystem initialised\n");
			g_observers.realise();
		}
		void shutdown ()
		{
			g_observers.unrealise();
			g_message("filesystem shutdown\n");
			Shutdown();
		}

		int getFileCount (const char *filename, int flags)
		{
			return GetFileCount(filename, flags);
		}
		ArchiveFile* openFile (const std::string& filename)
		{
			return OpenFile(filename);
		}
		ArchiveTextFile* openTextFile (const std::string& filename)
		{
			return OpenTextFile(filename);
		}
		std::size_t loadFile (const std::string& filename, void **buffer)
		{
			return LoadFile(filename, buffer, 0);
		}
		void freeFile (void *p)
		{
			FreeFile(p);
		}

		void forEachDirectory (const char* basedir, const FileNameCallback& callback, std::size_t depth)
		{
			GSList* list = GetListInternal(basedir, 0, true, depth);

			for (GSList* i = list; i != 0; i = g_slist_next(i)) {
				callback(reinterpret_cast<const char*> ((*i).data));
			}

			ClearFileDirList(&list);
		}

		void forEachFile (const char* basedir, const char* extension, const FileNameCallback& callback,
				std::size_t depth)
		{
			GSList* list = GetListInternal(basedir, extension, false, depth);

			for (GSList* i = list; i != 0; i = g_slist_next(i)) {
				const char* name = reinterpret_cast<const char*> ((*i).data);
				if (extension_equal(os::getExtension(name).c_str(), extension) || extension_equal(extension, "*")) {
					callback(name);
				}
			}

			ClearFileDirList(&list);
		}

		std::string findFile (const std::string& name)
		{
			return FindFile(name);
		}
		std::string findRoot (const std::string& name)
		{
			return FindPath(name);
		}
		std::string getRelative (const std::string& name)
		{
			const std::string abolsoluteBasePath = FindPath(name);
			return path_make_relative(name.c_str(), abolsoluteBasePath.c_str());
		}

		void attach (ModuleObserver& observer)
		{
			g_observers.attach(observer);
		}
		void detach (ModuleObserver& observer)
		{
			g_observers.detach(observer);
		}

		Archive* getArchive (const char* archiveName)
		{
			for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
				if ((*i).is_pakfile) {
					if (path_equal((*i).name.c_str(), archiveName)) {
						return (*i).archive;
					}
				}
			}
			return 0;
		}
		void forEachArchive (const ArchiveNameCallback& callback)
		{
			for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
				if ((*i).is_pakfile) {
					callback((*i).name.c_str());
				}
			}
		}
};

UFOFileSystem g_UFOFileSystem;

void FileSystem_Init ()
{
}

void FileSystem_Shutdown ()
{
}

VirtualFileSystem& GetFileSystem ()
{
	return g_UFOFileSystem;
}

#include "modulesystem/singletonmodule.h"
#include "modulesystem/modulesmap.h"

class FileSystemDependencies: public GlobalRadiantModuleRef
{
		ArchiveModulesRef m_archive_modules;
	public:
		FileSystemDependencies () :
			m_archive_modules(GlobalRadiant().getRequiredGameDescriptionKeyValue("archivetypes"))
		{
		}
		ArchiveModules& getArchiveModules ()
		{
			return m_archive_modules.get();
		}
};

class FileSystemAPI
{
		VirtualFileSystem* m_filesystem;
	public:
		typedef VirtualFileSystem Type;
		STRING_CONSTANT(Name, "*");

		FileSystemAPI ()
		{
			FileSystem_Init();
			m_filesystem = &GetFileSystem();
		}
		~FileSystemAPI ()
		{
			FileSystem_Shutdown();
		}
		VirtualFileSystem* getTable ()
		{
			return m_filesystem;
		}
};

typedef SingletonModule<FileSystemAPI, FileSystemDependencies> FileSystemModule;

typedef Static<FileSystemModule> StaticFileSystemModule;
StaticRegisterModule staticRegisterFileSystem(StaticFileSystemModule::instance());

ArchiveModules& FileSystemAPI_getArchiveModules ()
{
	return StaticFileSystemModule::instance().getDependencies().getArchiveModules();
}
