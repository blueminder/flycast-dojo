/* 
 * File:   StringFix.h
 * Author: kjell/ryan
 * https://github.com/KjellKod/StringFix
 *
 * Created on July 30, 2013, 10:46 AM
 */

#pragma once
#include <string>
#include <vector>

namespace stringfix {
   size_t replace(std::string& input, const std::string& from, const std::string& to);
   std::string trim(const std::string& str, const std::string& whitespace = " \t");
   std::string to_lower(const std::string& str);
   std::string to_upper(const std::string& str);
   std::string to_string(const bool& b);
   // This function is defined but NOT implemented. The reason for this is that
   //    there is no way to guarantee that the to_string function is called with
   //    boolean types passed into it. If a user passed in an int, it would be
   //    interpreted as a bool and the function would return the correct string.
   //    To guarantee the behavior we want, we force a compilation error.
   template<typename T> std::string to_string(const T& t);
   bool to_bool(const std::string& str);
   std::vector<std::string> split(const std::string& split_on_any_delimiters, const std::string& stringToSplit);
   std::vector<std::string> explode(const std::string& complete_match, const std::string& stringToExplode);
   std::vector<std::string> extract(const std::string& complete_match_start, const std::string& complete_match_end, const std::string& content);
   bool ContainsElement(const std::vector<std::string>& v, const std::string& s);
   std::string remove_extension(const std::string& s);
   std::string get_extension(const std::string& s);
};


