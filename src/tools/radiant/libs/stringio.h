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

#if !defined (INCLUDED_STRINGIO_H)
#define INCLUDED_STRINGIO_H

#include <stdlib.h>
#include <cctype>

#include "math/Vector3.h"
#include "iscriplib.h"
#include "string/string.h"
#include "generic/callback.h"

inline double buffer_parse_floating_literal (const char*& buffer)
{
	return strtod(buffer, const_cast<char**> (&buffer));
}

inline int buffer_parse_signed_decimal_integer_literal (const char*& buffer)
{
	return strtol(buffer, const_cast<char**> (&buffer), 10);
}

inline int buffer_parse_unsigned_decimal_integer_literal (const char*& buffer)
{
	return strtoul(buffer, const_cast<char**> (&buffer), 10);
}

// [+|-][nnnnn][.nnnnn][e|E[+|-]nnnnn]
inline bool string_parse_float (const char* string, float& f)
{
	if (string_empty(string)) {
		return false;
	}
	f = float(buffer_parse_floating_literal(string));
	return string_empty(string);
}

// format same as float
inline bool string_parse_double (const char* string, double& f)
{
	if (string_empty(string)) {
		return false;
	}
	f = buffer_parse_floating_literal(string);
	return string_empty(string);
}

// <float><space><float><space><float>
template<typename Element>
inline bool string_parse_vector3 (const std::string& str, BasicVector3<Element>& v)
{
	if (str.empty() || str[0] == ' ') {
		return false;
	}

	const char* string = str.c_str();
	v[0] = float(buffer_parse_floating_literal(string));
	if (*string++ != ' ') {
		return false;
	}
	v[1] = float(buffer_parse_floating_literal(string));
	if (*string++ != ' ') {
		return false;
	}
	v[2] = float(buffer_parse_floating_literal(string));
	return string_empty(string);
}

// decimal signed integer
inline bool string_parse_int (const std::string& str, int& i)
{
	if (str.empty()) {
		return false;
	}
	const char* string = str.c_str();
	i = buffer_parse_signed_decimal_integer_literal(string);
	return string_empty(string);
}

// decimal unsigned integer
inline bool string_parse_size (const std::string& str, std::size_t& i)
{
	if (str.empty()) {
		return false;
	}
	const char* string = str.c_str();
	i = buffer_parse_unsigned_decimal_integer_literal(string);
	return string_empty(string);
}

inline void Tokeniser_unexpectedError (Tokeniser& tokeniser, const std::string& token, const std::string& expected)
{
	globalErrorStream() << string::toString(tokeniser.getLine()) << ":" << string::toString(tokeniser.getColumn())
			<< ": parse error at '" << (token.length() ? token : "#EOF") << "': expected '" << expected << "'\n";
}

inline bool Tokeniser_getFloat (Tokeniser& tokeniser, float& f)
{
	const std::string token = tokeniser.getToken();
	if (token.length() && string_parse_float(token.c_str(), f)) {
		return true;
	}
	Tokeniser_unexpectedError(tokeniser, token, "#number");
	return false;
}

inline bool Tokeniser_getDouble (Tokeniser& tokeniser, double& f)
{
	const std::string token = tokeniser.getToken();
	if (token.length() && string_parse_double(token.c_str(), f)) {
		return true;
	}
	Tokeniser_unexpectedError(tokeniser, token, "#number");
	return false;
}

inline bool Tokeniser_getSize (Tokeniser& tokeniser, std::size_t& i)
{
	const std::string token = tokeniser.getToken();
	if (token.length() && string_parse_size(token.c_str(), i)) {
		return true;
	}
	Tokeniser_unexpectedError(tokeniser, token, "#unsigned-integer");
	return false;
}

inline bool Tokeniser_parseToken (Tokeniser& tokeniser, const char* expected)
{
	const std::string token = tokeniser.getToken();
	if (token.length() && string_equal(token, expected)) {
		return true;
	}
	Tokeniser_unexpectedError(tokeniser, token, expected);
	return false;
}

inline bool Tokeniser_nextTokenIsDigit (Tokeniser& tokeniser)
{
	const std::string token = tokeniser.getToken();
	if (token.empty()) {
		return false;
	}
	char c = *token.c_str();
	tokeniser.ungetToken();
	return std::isdigit(c) != 0;
}

template<typename TextOutputStreamType>
inline TextOutputStreamType& ostream_write (TextOutputStreamType& outputStream, const Vector3& v)
{
	return outputStream << "(" << v.x() << " " << v.y() << " " << v.z() << ")";
}

inline void StdString_importString (std::string& self, const char* string)
{
	self = string;
}
typedef ReferenceCaller1<std::string, const char*, StdString_importString> StringImportStringCaller;
inline void StdString_exportString (const std::string& self, const StringImportCallback& importer)
{
	importer(self.c_str());
}
typedef ConstReferenceCaller1<std::string, const StringImportCallback&, StdString_exportString>
		StringExportStringCaller;

inline void Bool_importString (bool& self, const char* string)
{
	self = string_equal(string, "true");
}
typedef ReferenceCaller1<bool, const char*, Bool_importString> BoolImportStringCaller;
inline void Bool_exportString (const bool& self, const StringImportCallback& importer)
{
	importer(self ? "true" : "false");
}
typedef ConstReferenceCaller1<bool, const StringImportCallback&, Bool_exportString> BoolExportStringCaller;

inline void Int_importString (int& self, const char* string)
{
	if (!string_parse_int(string, self)) {
		self = 0;
	}
}
typedef ReferenceCaller1<int, const char*, Int_importString> IntImportStringCaller;
inline void Int_exportString (const int& self, const StringImportCallback& importer)
{
	char buffer[16];
	sprintf(buffer, "%d", self);
	importer(buffer);
}
typedef ConstReferenceCaller1<int, const StringImportCallback&, Int_exportString> IntExportStringCaller;

inline void Size_importString (std::size_t& self, const char* string)
{
	int i;
	if (string_parse_int(string, i) && i >= 0) {
		self = i;
	} else {
		self = 0;
	}
}
typedef ReferenceCaller1<std::size_t, const char*, Size_importString> SizeImportStringCaller;
inline void Size_exportString (const std::size_t& self, const StringImportCallback& importer)
{
	char buffer[16];
	sprintf(buffer, "%u", Unsigned(self));
	importer(buffer);
}
typedef ConstReferenceCaller1<std::size_t, const StringImportCallback&, Size_exportString> SizeExportStringCaller;

inline void Float_importString (float& self, const char* string)
{
	if (!string_parse_float(string, self)) {
		self = 0;
	}
}
typedef ReferenceCaller1<float, const char*, Float_importString> FloatImportStringCaller;
inline void Float_exportString (const float& self, const StringImportCallback& importer)
{
	char buffer[16];
	sprintf(buffer, "%g", self);
	importer(buffer);
}
typedef ConstReferenceCaller1<float, const StringImportCallback&, Float_exportString> FloatExportStringCaller;

inline void Vector3_importString (Vector3& self, const char* string)
{
	//self(std::string(string));
	if (!string_parse_vector3(string, self)) {
		self = Vector3(0, 0, 0);
	}
}
typedef ReferenceCaller1<Vector3, const char*, Vector3_importString> Vector3ImportStringCaller;
inline void Vector3_exportString (const Vector3& self, const StringImportCallback& importer)
{
	char buffer[64];
	sprintf(buffer, "%g %g %g", self[0], self[1], self[2]);
	importer(buffer);
}
typedef ConstReferenceCaller1<Vector3, const StringImportCallback&, Vector3_exportString> Vector3ExportStringCaller;

template<typename FirstArgument, typename Caller, typename FirstConversion>
class ImportConvert1
{
	public:
		static void thunk (void* environment, FirstArgument firstArgument)
		{
			Caller::thunk(environment, FirstConversion(firstArgument));
		}
};

class BoolFromString
{
		bool m_value;
	public:
		BoolFromString (const char* string)
		{
			Bool_importString(m_value, string);
		}
		operator bool () const
		{
			return m_value;
		}
};

inline void Bool_toString (const StringImportCallback& self, bool value)
{
	Bool_exportString(value, self);
}
typedef ConstReferenceCaller1<StringImportCallback, bool, Bool_toString> BoolToString;

template<typename Caller>
inline StringImportCallback makeBoolStringImportCallback (const Caller& caller)
{
	return StringImportCallback(caller.getEnvironment(), ImportConvert1<StringImportCallback::first_argument_type,
			Caller, BoolFromString>::thunk);
}

template<typename Caller>
inline StringExportCallback makeBoolStringExportCallback (const Caller& caller)
{
	return StringExportCallback(caller.getEnvironment(), ImportConvert1<StringExportCallback::first_argument_type,
			Caller, BoolToString>::thunk);
}

class IntFromString
{
		int m_value;
	public:
		IntFromString (const char* string)
		{
			Int_importString(m_value, string);
		}
		operator int () const
		{
			return m_value;
		}
};

inline void Int_toString (const StringImportCallback& self, int value)
{
	Int_exportString(value, self);
}
typedef ConstReferenceCaller1<StringImportCallback, int, Int_toString> IntToString;

template<typename Caller>
inline StringImportCallback makeIntStringImportCallback (const Caller& caller)
{
	return StringImportCallback(caller.getEnvironment(), ImportConvert1<StringImportCallback::first_argument_type,
			Caller, IntFromString>::thunk);
}

template<typename Caller>
inline StringExportCallback makeIntStringExportCallback (const Caller& caller)
{
	return StringExportCallback(caller.getEnvironment(), ImportConvert1<StringExportCallback::first_argument_type,
			Caller, IntToString>::thunk);
}

class SizeFromString
{
		std::size_t m_value;
	public:
		SizeFromString (const char* string)
		{
			Size_importString(m_value, string);
		}
		operator std::size_t () const
		{
			return m_value;
		}
};

inline void Size_toString (const StringImportCallback& self, std::size_t value)
{
	Size_exportString(value, self);
}
typedef ConstReferenceCaller1<StringImportCallback, std::size_t, Size_toString> SizeToString;

template<typename Caller>
inline StringImportCallback makeSizeStringImportCallback (const Caller& caller)
{
	return StringImportCallback(caller.getEnvironment(), ImportConvert1<StringImportCallback::first_argument_type,
			Caller, SizeFromString>::thunk);
}

template<typename Caller>
inline StringExportCallback makeSizeStringExportCallback (const Caller& caller)
{
	return StringExportCallback(caller.getEnvironment(), ImportConvert1<StringExportCallback::first_argument_type,
			Caller, SizeToString>::thunk);
}

#endif
