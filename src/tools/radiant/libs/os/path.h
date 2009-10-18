/**
 * @file path.h
 * @brief OS file-system path comparison, decomposition and manipulation.
 * Paths are c-style null-terminated-character-arrays.
 * Path separators must be forward slashes (unix style).
 * Directory paths must end in a separator.
 * Paths must not contain the ascii characters \\ : * ? " < > or |.
 * Paths may be encoded in UTF-8 or any extended-ascii character set.
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#if !defined (INCLUDED_OS_PATH_H)
#define INCLUDED_OS_PATH_H

#include <string>

/** General utility functions for OS-related tasks
 */

namespace os
{
	/** Convert the slashes in a path to forward-slashes
	 */
	inline std::string standardPath (const std::string& inPath)
	{
		std::string newStr = inPath;
		std::string::size_type pos = newStr.find("\\");

		while (std::string::npos != pos) {
			newStr.replace(pos, 1, "/");
			pos = newStr.find("\\");
		}

		return newStr;
	}

	/** Return the extension for the given path, which is equal to the characters
	 * following the final period.
	 * If there is no period in the given string the full string will be returned
	 */
	inline std::string getExtension (const std::string& path)
	{
		return path.substr(path.rfind(".") + 1);
	}

	/** Return the filename of the given path, which is equal to the characters
	 * following the final slash.
	 * If there is no slash in the given string the full string will be returned
	 */
	inline std::string getFilenameFromPath (const std::string& path)
	{
		return path.substr(path.rfind("/") + 1);
	}

	/** Return the path up to the character before the last / of the given filename.
	 * If there is no slash in the given string the full string will be returned
	 */
	inline std::string stripFilename (const std::string& filename)
	{
		return filename.substr(0, filename.rfind("/"));
	}

	/**
	 * Will cut away the characters following the final dot.
	 * @param filename The filename to extract the basename from.
	 * @return The filename without extension
	 */
	inline std::string stripExtension (const std::string& filename)
	{
		std::string::size_type pos = filename.rfind(".");
		return filename.substr(0, pos);
	}
}

#include "string/string.h"

#include <glib.h>
#include <glib/gstdio.h>

#if defined(WIN32)
# define OS_CASE_INSENSITIVE
# ifndef PATH_MAX
#  define PATH_MAX 260
# endif
#endif

/// \brief Returns a pointer to the first character of the filename component of \p path.
/// O(n)
inline const char* path_get_filename_start (const char* path)
{
	// not strictly necessary,since paths should not contain '\'
	// mixed paths on windows containing both '/' and '\', use last of both for this
	const char* last_forward_slash = strrchr(path, '/');
	const char* last_backward_slash = strrchr(path, '\\');
	if (last_forward_slash > last_backward_slash) {
		return last_forward_slash + 1;
	} else if (last_forward_slash < last_backward_slash) {
		return last_backward_slash + 1;
	}

	return path;
}

/// \brief Returns true if \p path is lexicographically sorted before \p other.
/// If both \p path and \p other refer to the same file, neither will be sorted before the other.
/// O(n)
inline bool path_less (const char* path, const char* other)
{
#if defined(OS_CASE_INSENSITIVE)
	return string_less_nocase(path, other);
#else
	return string_less(path, other);
#endif
}

/// \brief Returns <0 if \p path is lexicographically less than \p other.
/// Returns >0 if \p path is lexicographically greater than \p other.
/// Returns 0 if both \p path and \p other refer to the same file.
/// O(n)
inline int path_compare (const char* path, const char* other)
{
#if defined(OS_CASE_INSENSITIVE)
	return string_compare_nocase(path, other);
#else
	return string_compare(path, other);
#endif
}

/// \brief Returns true if \p path and \p other refer to the same file or directory.
/// O(n)
inline bool path_equal (const char* path, const char* other)
{
#if defined(OS_CASE_INSENSITIVE)
	return string_equal_nocase(path, other);
#else
	return string_equal(path, other);
#endif
}

/// \brief Returns true if the first \p n bytes of \p path and \p other form paths that refer to the same file or directory.
/// If the paths are UTF-8 encoded, [\p path, \p path + \p n) must be a complete path.
/// O(n)
inline bool path_equal_n (const char* path, const char* other, std::size_t n)
{
#if defined(OS_CASE_INSENSITIVE)
	return string_equal_nocase_n(path, other, n);
#else
	return string_equal_n(path, other, n);
#endif
}

/// \brief Returns a pointer to the character after the end of the filename component of \p path - either the extension separator or the terminating null character.
/// O(n)
inline const char* path_get_filename_base_end (const char* path)
{
	const char* last_period = strrchr(path_get_filename_start(path), '.');
	return (last_period != 0) ? last_period : path + string_length(path);
}

/// \brief Returns the length of the filename component (not including extension) of \p path.
/// O(n)
inline std::size_t path_get_filename_base_length (const char* path)
{
	return path_get_filename_base_end(path) - path;
}

/// \brief If \p path is a child of \p base, returns the subpath relative to \p base, else returns \p path.
/// O(n)
inline const char* path_make_relative (const char* path, const char* base)
{
	const std::size_t length = string_length(base);
	if (path_equal_n(path, base, length)) {
		return path + length;
	}
	return path;
}

/// \brief Returns true if \p extension is of the same type as \p other.
/// O(n)
inline bool extension_equal (const char* extension, const char* other)
{
	return path_equal(extension, other);
}

template<typename Functor>
class MatchFileExtension
{
		const char* m_extension;
		const Functor& m_functor;
	public:
		MatchFileExtension (const char* extension, const Functor& functor) :
			m_extension(extension), m_functor(functor)
		{
		}
		void operator() (const char* name) const
		{
			const std::string extension = os::getExtension(name);
			if (extension_equal(extension.c_str(), m_extension)) {
				m_functor(name);
			}
		}
};

class DirectoryCleaned
{
	public:
		const char* m_path;
		DirectoryCleaned (const char* path) :
			m_path(path)
		{
		}
};

/// \brief Writes \p path to \p ostream with dos-style separators replaced by unix-style separators, and appends a separator if necessary.
template<typename TextOutputStreamType>
TextOutputStreamType& ostream_write (TextOutputStreamType& ostream, const DirectoryCleaned& path)
{
	const char* i = path.m_path;
	for (; *i != '\0'; ++i) {
		if (*i == '\\') {
			ostream << '/';
		} else {
			ostream << *i;
		}
	}
	const char c = *(i - 1);
	if (c != '/' && c != '\\') {
		ostream << '/';
	}
	return ostream;
}

#endif
