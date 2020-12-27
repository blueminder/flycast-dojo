/*
 * File:   StringFix.cpp
 * Author: kjell/ryan
 * https://github.com/KjellKod/StringFix
 *
 * Created on July 30, 2013, 10:46 AM
 */

#include "StringFix.h"
#include <algorithm>
#include <sstream>

namespace stringfix {

   /**
       * Replaces occurrences of string with another string.
       * Example:  string "Hello "World". from: World, to: "El Mundo"
       *        replace(str, from, to) --> string eq. "Hello El Mundo";
       *
       * WARNING: This function is not necessarily efficient.
       * @param str to change
       * @param from substring to match
       * @param to replace the matching substring with
       * @return number of replacements made
       *  */
   size_t replace(std::string& str, const std::string& from, const std::string& to) {
      if (from.empty() || str.empty()) {
         return 0;
      }

      size_t pos = 0;
      size_t count = 0;
      while ((pos = str.find(from, pos)) != std::string::npos) {
         str.replace(pos, from.length(), to);
         pos += to.length();
         ++count;
      }

      return count;
   }


   /** Trims a string's end and beginning from specified characters
    *        such as tab, space and newline i.e. "\t \n" (tab space newline)
    * @param str to clean
    * @param whitespace by default space and tab character
    * @return the cleaned string */
   std::string trim(const std::string& str, const std::string& whitespace) {
      const auto& begin = str.find_first_not_of(whitespace);
      if (std::string::npos == begin) {
         return {};
      }

      const auto& end = str.find_last_not_of(whitespace);
      const auto& range = end - begin + 1;
      return str.substr(begin, range);
   }

   /**
    * Splits a string into tokens. Tokens are split after every matched character in the delimeters string
    * @param delimiters : any matching character with the string will create a new token
    * @param stringToSplit
    * @return vector of tokens
    */
   std::vector<std::string> split(const std::string& delimiters, const std::string& stringToSplit) {
      using namespace std;
      // Skip all the text until the first delimiter is found
      string::size_type start = stringToSplit.find_first_not_of(delimiters, 0);
      string::size_type stop = stringToSplit.find_first_of(delimiters, start);

      std::vector<std::string> tokens;
      while (string::npos != stop || string::npos != start) {
         tokens.push_back(stringToSplit.substr(start, stop - start));
         start = stringToSplit.find_first_not_of(delimiters, stop);
         stop = stringToSplit.find_first_of(delimiters, start);
      }
      return tokens;
   }

   std::string to_lower(const std::string& str) {
      std::string copy(str);
      std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);
      return copy;
   }

   std::string to_upper(const std::string& str) {
      std::string copy(str);
      std::transform(copy.begin(), copy.end(), copy.begin(), ::toupper);
      return copy;
   }
   
   std::string to_string(const bool& b) {
      std::stringstream ss;
      ss << std::boolalpha << b;
      return ss.str();
   }
   
   bool to_bool(const std::string& str) {
      std::string boolStr = str;
      if (str == "1") {
         boolStr = "true";
      } else if (str == "0") {
         boolStr = "false";
      }
      std::string lowerStr = to_lower(boolStr); 
      if (lowerStr != "true" && lowerStr != "false") {
         // Currently using DeathKnell in the tests does not work as expected probably due to lack
         // of g3 integration with this namespace. Since we can't catch abort in the tests, we are 
         // returning false instead.
         return false;
      }

      std::stringstream ss(lowerStr);
      bool enabled;
      ss >> std::boolalpha >> enabled;
      return enabled;
   }

   /**
    * Explodes a string into sub-strings. Each substring is extracted after a complete
    * match of the "delimeter string"
    *
    * Explode work a litte different from split in that it is very similar to explode in php (and other languages)
    * But it uses a complete string match instead of single character (like split)
    *
    *
    * Scenario:  No match made. String returned as is, without exploding
    * Scenario:  Complete match made. Both strings equal. Complete explode. vector with 1 empty string return
    * Scenario:  string {a::c}, explode on {:}. Return { {"a"},{""},{"c"}}. Please notice the empty string
    *
    * @param complete_match : string matching where to split the content and remove the match
    * @param stringToExplode: to split at the matches
    * @return vector of exploded sub-strings
    */
   std::vector<std::string> explode(const std::string& matchAll, const std::string& toExplode) {
      if (matchAll.empty()) {
         return {toExplode};
      }

      std::vector<std::string> foundMatching;
      std::string::size_type position = 0;
      std::string::size_type keySize = matchAll.size();
      std::string::size_type found = 0;

      while ((found != std::string::npos)) {
         found = toExplode.find(matchAll, position);
         std::string item  = toExplode.substr(position, found - position);
         position = found + keySize;

         bool ignore = item.empty() && (std::string::npos == found);
         if (false == ignore) {
            foundMatching.push_back(item);
         }
      }
      return foundMatching;
   }

   /**
   * Extract blocks separated with @param complete_match_start and @param complete_match_end from the @param content
   * @return vector of blocks, including the start and end match keys
   */
   std::vector<std::string> extract(const std::string& complete_match_start, const std::string& complete_match_end,
                                    const std::string& content) {

      if (complete_match_start.size() == 0 || complete_match_end.size() == 0) {
         return {};
      }

      std::vector<std::string> result;
      const std::string::size_type startKeySize = complete_match_start.size();
      const std::string::size_type stopKeySize = complete_match_end.size();
      std::string::size_type found = 0;

      auto ContinueSearch = [](std::string::size_type found) { return found != std::string::npos;};

      while (ContinueSearch(found)) {
         auto found_start = content.find(complete_match_start, found);
         if (!ContinueSearch(found_start)) {
            break;
         }

         auto found_stop = content.find(complete_match_end, found_start + startKeySize);
         if (!ContinueSearch(found_stop)) {
            break;
         }

         found = found_stop + stopKeySize;
         auto size = found - found_start;
         result.push_back(content.substr(found_start, size));
         if (found >= content.size()) {
            break;
         }
      }

      return result;
   }
  
   bool ContainsElement(const std::vector<std::string>& v, const std::string& s) {
     return (std::find(v.begin(), v.end(), s) != v.end()); 
   }

   std::string remove_extension(const std::string& s) {
      return s.substr(0, s.find_last_of(".")); 
   }
   
   std::string get_extension(const std::string& s) {
      auto lastIndexOfPeriod = s.find_last_of(".");
      if (lastIndexOfPeriod != std::string::npos) {
         return s.substr(lastIndexOfPeriod+1, s.size()+1); 
      }
      return "";
   }

} // namespace
