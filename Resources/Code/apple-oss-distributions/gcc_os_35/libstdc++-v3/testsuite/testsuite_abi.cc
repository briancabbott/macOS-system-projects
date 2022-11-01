// -*- C++ -*-

// Copyright (C) 2004 Free Software Foundation, Inc.

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2, or (at
// your option) any later version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
// MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free
// software library without restriction.  Specifically, if other files
// instantiate templates or use macros or inline functions from this
// file, or you compile this file and link it with other files to
// produce an executable, this file does not by itself cause the
// resulting executable to be covered by the GNU General Public
// License.  This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General
// Public License.

// Benjamin Kosnik  <bkoz@redhat.com>

#include "testsuite_abi.h"
#include <sstream>
#include <fstream>
#include <iostream>

using namespace std;

void 
symbol::init(string& data)
{
  const char delim = ':';
  const char version_delim = '@';
  const string::size_type npos = string::npos;
  string::size_type n = 0;

  // Set the type.
  if (data.find("FUNC") == 0)
    type = symbol::function;
  else if (data.find("OBJECT") == 0)
    type = symbol::object;
  else
    type = symbol::error;
  n = data.find_first_of(delim);
  if (n != npos)
    data.erase(data.begin(), data.begin() + n + 1);

  // Iff object, get size info.
  if (type == symbol::object)
    {
      n = data.find_first_of(delim);
      if (n != npos)
	{
	  string size(data.begin(), data.begin() + n);
	  istringstream iss(size);
	  int x;
	  iss >> x;
	  if (!iss.fail())
	    size = x;
	  data.erase(data.begin(), data.begin() + n + 1);
	}
    }

  // Set the name.
  n = data.find_first_of(version_delim);
  if (n != npos)
    {
      // Found version string.
      name = string(data.begin(), data.begin() + n);
      n = data.find_last_of(version_delim);
      data.erase(data.begin(), data.begin() + n + 1);

      // Set version name.
      version_name = data;
    }
  else
    {
      // No versioning info.
      name = string(data.begin(), data.end());
      data.erase(data.begin(), data.end());
    }

  // Set the demangled name.
  demangled_name = demangle(name);
}

void
symbol::print() const
{
  const char tab = '\t';
  cout << tab << name << endl;
  cout << tab << demangled_name << endl;
  cout << tab << version_name << endl;

  string type_string;
  switch (type)
    {
    case none:
      type_string = "none";
      break;
    case function:
      type_string = "function";
      break;
    case object:
      type_string = "object";
      break;
    case error:
      type_string = "error";
      break;
    default:
      type_string = "<default>";
    }
  cout << tab << type_string << endl;
  
  if (type == object)
    cout << tab << size << endl;

  string status_string;
  switch (status)
    {
    case unknown:
      status_string = "unknown";
      break;
    case added:
      status_string = "added";
      break;
    case subtracted:
      status_string = "subtracted";
      break;
    case compatible:
      status_string = "compatible";
      break;
    case incompatible:
      status_string = "incompatible";
      break;
    default:
      status_string = "<default>";
    }
  cout << tab << status_string << endl;
}


bool
check_version(const symbol& test, bool added)
{
  typedef std::vector<std::string> compat_list;
  static compat_list known_versions;
  if (known_versions.empty())
    {
      known_versions.push_back("GLIBCPP_3.2"); // base version
      known_versions.push_back("GLIBCPP_3.2.1");
      known_versions.push_back("GLIBCPP_3.2.2");
      known_versions.push_back("GLIBCPP_3.2.3"); // gcc-3.3.0
      known_versions.push_back("GLIBCXX_3.4");
      known_versions.push_back("GLIBCXX_3.4.1");
      known_versions.push_back("GLIBCXX_3.4.2");
      known_versions.push_back("CXXABI_1.2");
      known_versions.push_back("CXXABI_1.2.1");
      known_versions.push_back("CXXABI_1.3");
    }
  compat_list::iterator begin = known_versions.begin();
  compat_list::iterator end = known_versions.end();

  // Check version names for compatibility...
  compat_list::iterator it1 = find(begin, end, test.version_name);
  
  // Check for weak label.
  compat_list::iterator it2 = find(begin, end, test.name);

  // Check that added symbols aren't added in the base version.
  bool compat = true;
  if (added && test.version_name == known_versions[0])
    compat = false;

  if (it1 == end && it2 == end)
    compat = false;

  return compat;
}

bool 
check_compatible(const symbol& lhs, const symbol& rhs, bool verbose)
{
  bool ret = true;
  const char tab = '\t';

  // Check to see if symbol_objects are compatible.
  if (lhs.type != rhs.type)
    {
      ret = false;
      if (verbose)
	cout << tab << "incompatible types" << endl;
    }
  
  if (lhs.name != rhs.name)
    {
      ret = false;
      if (verbose)
	cout << tab << "incompatible names" << endl;
    }

  if (lhs.size != rhs.size)
    {
      ret = false;
      if (verbose)
	{
	  cout << tab << "incompatible sizes" << endl;
	  cout << tab << lhs.size << endl;
	  cout << tab << rhs.size << endl;
	}
    }

  if (lhs.version_name != rhs.version_name 
      && !check_version(lhs) && !check_version(rhs))
    {
      ret = false;
      if (verbose)
	{
	  cout << tab << "incompatible versions" << endl;
	  cout << tab << lhs.version_name << endl;
	  cout << tab << rhs.version_name << endl;
	}
    }

  if (verbose)
    cout << endl;

  return ret;
}


bool
has_symbol(const string& mangled, const symbols& s) throw()
{
  const symbol_names& names = s.first;
  symbol_names::const_iterator i = find(names.begin(), names.end(), mangled);
  return i != names.end();
}

symbol&
get_symbol(const string& mangled, const symbols& s)
{
  const symbol_names& names = s.first;
  symbol_names::const_iterator i = find(names.begin(), names.end(), mangled);
  if (i != names.end())
    {
      symbol_objects objects = s.second;
      return objects[mangled];
    }
  else
    {
      ostringstream os;
      os << "get_symbol failed for symbol " << mangled;
      throw symbol_error(os.str());
    }
}

void 
examine_symbol(const char* name, const char* file)
{
  try
    {
      symbols s = create_symbols(file);
      symbol& sym = get_symbol(name, s);
      sym.print();
    }
  catch(...)
    { throw; }
}

void 
compare_symbols(const char* baseline_file, const char* test_file, 
		bool verbose)
{
  // Input both lists of symbols into container.
  symbols baseline = create_symbols(baseline_file);
  symbols test = create_symbols(test_file);
  symbol_names& baseline_names = baseline.first;
  symbol_objects& baseline_objects = baseline.second;
  symbol_names& test_names = test.first;
  symbol_objects& test_objects = test.second;

  //  Sanity check results.
  const symbol_names::size_type baseline_size = baseline_names.size();
  const symbol_names::size_type test_size = test_names.size();
  if (!baseline_size || !test_size)
    {
      cerr << "Problems parsing the list of exported symbols." << endl;
      exit(2);
    }

  // Sort out names.
  // Assuming baseline_names, test_names are both unique w/ no duplicates.
  //
  // The names added to missing_names are baseline_names not found in
  // test_names 
  // -> symbols that have been deleted.
  //
  // The names added to added_names are test_names are names not in
  // baseline_names
  // -> symbols that have been added.
  symbol_names shared_names;
  symbol_names missing_names;
  symbol_names added_names = test_names;
  for (size_t i = 0; i < baseline_size; ++i)
    {
      string what(baseline_names[i]);
      symbol_names::iterator end = added_names.end();
      symbol_names::iterator it = find(added_names.begin(), end, what);
      if (it != end)
	{
	  // Found.
	  shared_names.push_back(what);
	  added_names.erase(it);
	}
      else
	  missing_names.push_back(what);
    }

  // Check missing names for compatibility.
  typedef pair<symbol, symbol> symbol_pair;
  vector<symbol_pair> incompatible;
  for (size_t j = 0; j < missing_names.size(); ++j)
    {
      symbol base = baseline_objects[missing_names[j]];
      incompatible.push_back(symbol_pair(base, base));
    }

  // Check shared names for compatibility.
  for (size_t k = 0; k < shared_names.size(); ++k)
    {
      symbol base = baseline_objects[shared_names[k]];
      symbol test = test_objects[shared_names[k]];
      if (!check_compatible(base, test))
	incompatible.push_back(symbol_pair(base, test));
    }

  // Check added names for compatibility.
  for (size_t l = 0; l < added_names.size(); ++l)
    {
      symbol test = test_objects[added_names[l]];
      if (!check_version(test, true))
	incompatible.push_back(symbol_pair(test, test));
    }

  // Report results.
  if (verbose && added_names.size())
    {
      cout << added_names.size() << " added symbols " << endl;
      for (size_t j = 0; j < added_names.size() ; ++j)
	test_objects[added_names[j]].print();
    }
  
  if (verbose && missing_names.size())
    {
      cout << missing_names.size() << " missing symbols " << endl;
      for (size_t j = 0; j < missing_names.size() ; ++j)
	baseline_objects[missing_names[j]].print();
    }
  
  if (verbose && incompatible.size())
    {
      cout << incompatible.size() << " incompatible symbols " << endl;
      for (size_t j = 0; j < incompatible.size() ; ++j)
	{
	  // First, report name.
	  const symbol& base = incompatible[j].first;
	  const symbol& test = incompatible[j].second;
	  test.print();
	  
	  // Second, report reason or reasons incompatible.
	  check_compatible(base, test, true);
	}
    }
  
  cout << "\n\t\t=== libstdc++-v3 check-abi Summary ===" << endl;
  cout << endl;
  cout << "# of added symbols:\t\t " << added_names.size() << endl;
  cout << "# of missing symbols:\t\t " << missing_names.size() << endl;
  cout << "# of incompatible symbols:\t " << incompatible.size() << endl;
  cout << endl;
  cout << "using: " << baseline_file << endl;
}


symbols
create_symbols(const char* file)
{
  symbols s;
  ifstream ifs(file);
  if (ifs.is_open())
    {
      // Organize file data into container of symbol objects.
      symbol_names& names = s.first;
      symbol_objects& objects = s.second;
      const string empty;
      string line = empty;
      while (getline(ifs, line).good())
	{
	  symbol tmp;
	  tmp.init(line);
	  objects[tmp.name] = tmp;
	  names.push_back(tmp.name);
	  line = empty;
	}
    }
  else
    {
      ostringstream os;
      os << "create_symbols failed for file " << file;
      throw runtime_error(os.str());
    }
  return s;
}


const char*
demangle(const std::string& mangled)
{
  const char* name;
  if (mangled[0] != '_' || mangled[1] != 'Z')
    {
      // This is not a mangled symbol, thus has "C" linkage.
      name = mangled.c_str();
    }
  else
    {
      // Use __cxa_demangle to demangle.
      int status = 0;
      name = abi::__cxa_demangle(mangled.c_str(), 0, 0, &status);
      if (!name)
	{
	  switch (status)
	    {
	    case 0:
	      name = "error code = 0: success";
	      break;
	    case -1:
	      name = "error code = -1: memory allocation failure";
	      break;
	    case -2:
	      name = "error code = -2: invalid mangled name";
	      break;
	    case -3:
	      name = "error code = -3: invalid arguments";
	      break;
	    default:
	      name = "error code unknown - who knows what happened";
	    }
	}
    }
  return name;
}

