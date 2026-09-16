// SPDX-FileCopyrightText: 2026 BitLoupe developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_USERDEFINITIONS_H
#define CORE_USERDEFINITIONS_H

/*
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://bitloupe.org/schemas/user-definitions.json",
  "title": "BitLoupe User Definitions",
  "type": "object",
  "required": ["startupDefinitions"],
  "additionalProperties": false,
  "properties": {
    "startupDefinitions": {
      "type": "array",
      "items": { "type": "string" }
    }
  }
}
*/

class Settings;

namespace UserDefinitions {
inline constexpr const char* kStartupDefinitionsJsonKey = "startupDefinitions";

void loadInto(Settings* settings);
void saveFrom(const Settings* settings);
}

#endif // CORE_USERDEFINITIONS_H
