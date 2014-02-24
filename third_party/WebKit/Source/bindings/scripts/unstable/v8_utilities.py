# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Functions shared by various parts of the code generator.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

# FIXME: eliminate this file if possible

import re

from v8_globals import includes
import v8_types

ACRONYMS = ['CSS', 'HTML', 'IME', 'JS', 'SVG', 'URL', 'WOFF', 'XML', 'XSLT']


def has_extended_attribute(definition_or_member, extended_attribute_list):
    return any(extended_attribute in definition_or_member.extended_attributes
               for extended_attribute in extended_attribute_list)


def extended_attribute_value_contains(extended_attribute_value, value):
    return value in re.split('[|&]', extended_attribute_value)


def capitalize(name):
    """Capitalize first letter or initial acronym (used in setter names)."""
    for acronym in ACRONYMS:
        if name.startswith(acronym.lower()):
            return name.replace(acronym.lower(), acronym)
    return name[0].upper() + name[1:]


def strip_suffix(string, suffix):
    if not suffix or not string.endswith(suffix):
        return string
    return string[:-len(suffix)]


def uncapitalize(name):
    """Uncapitalizes first letter or initial acronym (* with some exceptions).

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    Used in method names; exceptions differ from capitalize().
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            name.replace(acronym, acronym.lower())
            return name
    return name[0].lower() + name[1:]


def v8_class_name(interface):
    return v8_types.v8_type(interface.name)


def enum_validation_expression(idl_type):
    if not v8_types.is_enum_type(idl_type):
        return None
    return ' || '.join(['string == "%s"' % enum_value
                        for enum_value in v8_types.enum_values(idl_type)])


# [ActivityLogging]
def activity_logging_world_list(member, access_type=None):
    """Returns a set of world suffixes for which a definition member has activity logging, for specified access type.

    access_type can be 'Getter' or 'Setter' if only checking getting or setting.
    """
    if 'ActivityLogging' not in member.extended_attributes:
        return set()
    activity_logging = member.extended_attributes['ActivityLogging']
    # [ActivityLogging=Access*] means log for all access, otherwise check that
    # value agrees with specified access_type.
    has_logging = (activity_logging.startswith('Access') or
                   (access_type and activity_logging.startswith(access_type)))
    if not has_logging:
        return set()
    includes.add('bindings/v8/V8DOMActivityLogger.h')
    if activity_logging.endswith('ForIsolatedWorlds'):
        return set([''])
    if activity_logging.endswith('ForAllWorlds'):
        return set(['', 'ForMainWorld'])
    raise 'Unrecognized [ActivityLogging] value: "%s"' % activity_logging


# [CallWith]
CALL_WITH_ARGUMENTS = {
    'ScriptState': '&state',
    'ExecutionContext': 'scriptContext',
    'ScriptArguments': 'scriptArguments.release()',
    'ActiveWindow': 'activeDOMWindow()',
    'FirstWindow': 'firstDOMWindow()',
}
# List because key order matters, as we want arguments in deterministic order
CALL_WITH_VALUES = [
    'ScriptState',
    'ExecutionContext',
    'ScriptArguments',
    'ActiveWindow',
    'FirstWindow',
]


def call_with_arguments(call_with_values, contents):
    if not call_with_values:
        return []

    # FIXME: Implement other template values for functions
    contents['is_call_with_script_execution_context'] = extended_attribute_value_contains(call_with_values, 'ExecutionContext')

    return [CALL_WITH_ARGUMENTS[value]
            for value in CALL_WITH_VALUES
            if extended_attribute_value_contains(call_with_values, value)]


# [Conditional]
def generate_conditional_string(definition_or_member):
    if 'Conditional' not in definition_or_member.extended_attributes:
        return None
    conditional = definition_or_member.extended_attributes['Conditional']
    for operator in '&|':
        if operator in conditional:
            conditions = set(conditional.split(operator))
            operator_separator = ' %s%s ' % (operator, operator)
            return operator_separator.join('ENABLE(%s)' % expression for expression in sorted(conditions))
    return 'ENABLE(%s)' % conditional


# [DeprecateAs]
def generate_deprecate_as(member, contents):
    deprecate_as = member.extended_attributes.get('DeprecateAs')
    if not deprecate_as:
        return
    contents['deprecate_as'] = deprecate_as
    includes.update(['core/page/UseCounter.h'])


# [PerContextEnabled]
def per_context_enabled_function_name(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'PerContextEnabled' not in extended_attributes:
        return None
    feature_name = extended_attributes['PerContextEnabled']
    return 'ContextFeatures::%sEnabled' % uncapitalize(feature_name)


# [RuntimeEnabled]
def runtime_enabled_function_name(definition_or_member):
    """Returns the name of the RuntimeEnabledFeatures function.

    The returned function checks if a method/attribute is enabled.
    Given extended attribute RuntimeEnabled=FeatureName, return:
        RuntimeEnabledFeatures::{featureName}Enabled
    """
    extended_attributes = definition_or_member.extended_attributes
    if 'RuntimeEnabled' not in extended_attributes:
        return None
    feature_name = extended_attributes['RuntimeEnabled']
    return 'RuntimeEnabledFeatures::%sEnabled' % uncapitalize(feature_name)


# [ImplementedAs]
def cpp_name(definition_or_member):
    return definition_or_member.extended_attributes.get('ImplementedAs', definition_or_member.name)


# [MeasureAs]
def generate_measure_as(definition_or_member, contents):
    if 'MeasureAs' not in definition_or_member.extended_attributes:
        return
    contents['measure_as'] = definition_or_member.extended_attributes['MeasureAs']
    includes.add('core/page/UseCounter.h')
