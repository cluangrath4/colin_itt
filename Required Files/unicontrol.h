
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//Commented by Colin Luangrath

#ifndef PTI_TOOLS_UNITRACE_UNICONTROL_H
#define PTI_TOOLS_UNITRACE_UNICONTROL_H

#include "utils.h"
#include <iostream>

extern char **environ;

/* * This class is used to control the collection of events in the unitrace tool.
 * It checks if the collection is enabled based on environment variables and allows
 * pausing and resuming the collection.
 */
class UniController{
  public:
    static bool IsCollectionEnabled(void) {
      if (conditional_collection_) {
        if (itt_paused_) {
          return false; // If ITT collection is paused, return false (collection disabled)
        }
        // Check the environment variable PTI_ENABLE_COLLECTION
        // If it is not set, we assume collection is enabled
        if (environ != nullptr) {
          char *env;
          char *value = nullptr;
          constexpr int len = sizeof("PTI_ENABLE_COLLECTION") - 1;	// do not count trailing '\0' 
          char **cursor = environ;
          // PTI_ENABLE_COLLECTION is likely at the end if it is set
          while (*cursor) {
            cursor++;	
          }
          cursor--;

          // Really weird way of searching if the variable is PTI_ENABLE_COLLECTION
          for (; (cursor != environ - 1) && ((env = *cursor) != nullptr); cursor--) {
            if ((env[0] == 'P') && (env[1] == 'T') && (env[2] == 'I') && (strncmp(env + 3, "_ENABLE_COLLECTION", len - 3) == 0) && (env[len] == '=')) {
              value = (env + len + 1); 
              break;
            }
          }

          // If the variable is not set or is set to "0", return false
          if ((value == nullptr) || (*value == '0')) {
            return false;
          }
        }
      }
      return true;
    }
    // Pauses the collection by setting PTI_ENABLE_COLLECTION to false
    static void IttPause(void) {
      itt_paused_ = true;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "0");
    }
    // Resums the collection by setting PTI_ENABLE_COLLECTION to true
    static void IttResume(void) {
      itt_paused_ = false;
      utils::SetEnv("PTI_ENABLE_COLLECTION", "1");
    }
  private:
    // Static variables to control the collection state
    inline static bool conditional_collection_ = (utils::GetEnv("UNITRACE_ConditionalCollection") == "1") ? true : false;
    inline static bool itt_paused_ = false;
};
    
#endif // PTI_TOOLS_UNITRACE_UNICONTROL_H