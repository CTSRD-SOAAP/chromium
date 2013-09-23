#!/usr/bin/env python

import os.path
import re
import subprocess
import sys

from in_file import InFile
import in_generator
import license


HEADER_TEMPLATE = """
%(license)s

#ifndef %(class_name)s_h
#define %(class_name)s_h

#include <string.h>
#include "wtf/HashFunctions.h"
#include "wtf/HashTraits.h"

namespace WTF {
class AtomicString;
class String;
}

namespace WebCore {

enum CSSPropertyID {
    CSSPropertyInvalid = 0,
    CSSPropertyVariable = 1,
%(property_enums)s
};

const int firstCSSProperty = %(first_property_id)s;
const int numCSSProperties = %(properties_count)s;
const int lastCSSProperty = %(last_property_id)d;
const size_t maxCSSPropertyNameLength = %(max_name_length)d;

const char* getPropertyName(CSSPropertyID);
const WTF::AtomicString& getPropertyNameAtomicString(CSSPropertyID);
WTF::String getPropertyNameString(CSSPropertyID);
WTF::String getJSPropertyName(CSSPropertyID);

inline CSSPropertyID convertToCSSPropertyID(int value)
{
    ASSERT((value >= firstCSSProperty && value <= lastCSSProperty) || value == CSSPropertyInvalid);
    return static_cast<CSSPropertyID>(value);
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::CSSPropertyID> { typedef IntHash<unsigned> Hash; };
template<> struct HashTraits<WebCore::CSSPropertyID> : GenericHashTraits<WebCore::CSSPropertyID> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static void constructDeletedValue(WebCore::CSSPropertyID& slot) { slot = static_cast<WebCore::CSSPropertyID>(WebCore::lastCSSProperty + 1); }
    static bool isDeletedValue(WebCore::CSSPropertyID value) { return value == (WebCore::lastCSSProperty + 1); }
};
}

#endif // %(class_name)s_h
"""

GPERF_TEMPLATE = """
%%{
%(license)s

#include "config.h"
#include "%(class_name)s.h"
#include "core/platform/HashTools.h"
#include <string.h>

#include "wtf/ASCIICType.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
const char* const propertyNameStrings[numCSSProperties] = {
%(property_name_strings)s
};

%%}
%%struct-type
struct Property;
%%omit-struct-type
%%language=C++
%%readonly-tables
%%global-table
%%compare-strncmp
%%define class-name %(class_name)sHash
%%define lookup-function-name findPropertyImpl
%%define hash-function-name propery_hash_function
%%define word-array-name property_wordlist
%%enum
%%%%
%(property_to_enum_map)s
%%%%
const Property* findProperty(register const char* str, register unsigned int len)
{
    return %(class_name)sHash::findPropertyImpl(str, len);
}

const char* getPropertyName(CSSPropertyID id)
{
    if (id < firstCSSProperty)
        return 0;
    int index = id - firstCSSProperty;
    if (index >= numCSSProperties)
        return 0;
    return propertyNameStrings[index];
}

const AtomicString& getPropertyNameAtomicString(CSSPropertyID id)
{
    if (id < firstCSSProperty)
        return nullAtom;
    int index = id - firstCSSProperty;
    if (index >= numCSSProperties)
        return nullAtom;

    static AtomicString* propertyStrings = new AtomicString[numCSSProperties]; // Intentionally never destroyed.
    AtomicString& propertyString = propertyStrings[index];
    if (propertyString.isNull()) {
        const char* propertyName = propertyNameStrings[index];
        propertyString = AtomicString(propertyName, strlen(propertyName), AtomicString::ConstructFromLiteral);
    }
    return propertyString;
}

String getPropertyNameString(CSSPropertyID id)
{
    // We share the StringImpl with the AtomicStrings.
    return getPropertyNameAtomicString(id).string();
}

String getJSPropertyName(CSSPropertyID id)
{
    char result[maxCSSPropertyNameLength + 1];
    const char* cssPropertyName = getPropertyName(id);
    const char* propertyNamePointer = cssPropertyName;
    if (!propertyNamePointer)
        return emptyString();

    char* resultPointer = result;
    while (char character = *propertyNamePointer++) {
        if (character == '-') {
            char nextCharacter = *propertyNamePointer++;
            if (!nextCharacter)
                break;
            character = (propertyNamePointer - 2 != cssPropertyName) ? toASCIIUpper(nextCharacter) : nextCharacter;
        }
        *resultPointer++ = character;
    }
    *resultPointer = '\\0';
    return String(result);
}

} // namespace WebCore
"""


class CSSPropertiesWriter(in_generator.Writer):
    class_name = "CSSPropertyNames"
    defaults = {
        'alias_for': None,
        'condition': None,
    }

    def __init__(self, file_paths, enabled_conditions):
        in_generator.Writer.__init__(self, file_paths, enabled_conditions)
        self._outputs = {(self.class_name + ".h"): self.generate_header,
                         (self.class_name + ".cpp"): self.generate_implementation,
                        }

        all_properties = self.in_file.name_dictionaries
        self._aliases = filter(lambda property: property['alias_for'], all_properties)
        for offset, property in enumerate(self._aliases):
            # Aliases use the enum_name that they are an alias for.
            property['enum_name'] = self._enum_name_from_property_name(property['alias_for'])
            # Aliases do not get an enum_value.

        self._properties = filter(lambda property: not property['alias_for'] and not property['condition'] or property['condition'] in self._enabled_conditions, all_properties)
        if len(self._properties) > 1024:
            print "ERROR : There is more than 1024 CSS Properties, you need to update CSSProperty.h/StylePropertyMetadata m_propertyID accordingly."
            exit(1)
        self._first_property_id = 2  # We start after CSSPropertyInvalid and CSSPropertyVariable.
        property_id = self._first_property_id
        for offset, property in enumerate(self._properties):
            property['enum_name'] = self._enum_name_from_property_name(property['name'])
            property['enum_value'] = self._first_property_id + offset

    def _enum_name_from_property_name(self, property_name):
        return "CSSProperty" + re.sub(r'(^[^-])|-(.)', lambda match: (match.group(1) or match.group(2)).upper(), property_name)

    def _enum_declaration(self, property):
        return "    %(enum_name)s = %(enum_value)s," % property

    def generate_header(self):
        return HEADER_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'property_enums': "\n".join(map(self._enum_declaration, self._properties)),
            'first_property_id': self._first_property_id,
            'properties_count': len(self._properties),
            'last_property_id': self._first_property_id + len(self._properties) - 1,
            'max_name_length': reduce(max, map(len, map(lambda property: property['name'], self._properties))),
        }

    def generate_implementation(self):
        gperf_input = GPERF_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'property_name_strings': '\n'.join(map(lambda property: '    "%(name)s",' % property, self._properties)),
            'property_to_enum_map': '\n'.join(map(lambda property: '%(name)s, %(enum_name)s' % property, self._properties + self._aliases)),
        }
        # FIXME: If we could depend on Python 2.7, we would use subprocess.check_output
        gperf_args = ['gperf', '--key-positions=*', '-D', '-n', '-s', '2']
        gperf = subprocess.Popen(gperf_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        return gperf.communicate(gperf_input)[0]


if __name__ == "__main__":
    in_generator.Maker(CSSPropertiesWriter).main(sys.argv)
